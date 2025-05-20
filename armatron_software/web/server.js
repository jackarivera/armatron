const express = require('express');
const path = require('path');
const http = require('http');
const { Server } = require('socket.io');
const net = require('net');
const fs = require('fs');
const { exec } = require('child_process');

const SOCKET_PATH = '/home/debian/.armatron/robot_socket';
const DEBUG = false;

const app = express();
const httpServer = http.createServer(app);
const io = new Server(httpServer);

// Add JSON body parsing middleware
app.use(express.json());

let client = null; // we'll hold a single connection to the daemon

// Service logs streaming
let logStreamProcess = null;
let logStreamClients = new Set();

function connectToDaemon() {
    client = net.createConnection(SOCKET_PATH, () => {
        console.log("[Node] Connected to real_time_daemon via UNIX socket.");
    });

    client.on('error', (err) => {
        console.error("[Node] Socket error:", err);
        setTimeout(connectToDaemon, 2000);
    });

    client.on('close', () => {
        console.log("[Node] Socket closed, retrying in 2s...");
        setTimeout(connectToDaemon, 2000);
    });

    let buffer = '';
    client.on('data', (data) => {
        buffer += data.toString();
        let lines = buffer.split('\n');
        buffer = lines.pop(); // the last part may be partial
        lines.forEach((line) => {
            line = line.trim();
            if (line.length === 0) return;
            // Basic validation: check that the line starts with '{' and ends with '}'
            if (line[0] !== '{' || line[line.length - 1] !== '}') {
                console.error("[Node] Received invalid JSON line, skipping:", line);
                return;
            }
            try {
                let msg = JSON.parse(line);
                if (DEBUG) {
                    console.log(`[Node] Received JSON: ${line}`);
                }
                if (msg.type === 'motorStates') {
                    io.emit('motorStates', msg);
                }
            } catch (e) {
                console.error("[Node] parse error: ", e);
            }
        });
        // Add an immediate read event to ensure fast reading
        process.nextTick(() => {
            client.resume();
        });
    });
}

// Serve static files from the dist directory
app.use(express.static(path.join(__dirname, 'dist')));

// Position storage routes
const STORAGE_FILE = path.join(__dirname, 'saved_positions.json');

// Initialize storage file if it doesn't exist
if (!fs.existsSync(STORAGE_FILE)) {
  fs.writeFileSync(STORAGE_FILE, JSON.stringify([]));
}

app.get('/api/positions', (req, res) => {
  try {
    const data = fs.readFileSync(STORAGE_FILE, 'utf8');
    res.json(JSON.parse(data));
  } catch (error) {
    console.error('Error loading positions:', error);
    res.status(500).json({ error: 'Failed to load positions' });
  }
});

app.post('/api/positions', (req, res) => {
  try {
    const positions = JSON.parse(fs.readFileSync(STORAGE_FILE, 'utf8'));
    positions.push(req.body);
    fs.writeFileSync(STORAGE_FILE, JSON.stringify(positions, null, 2));
    res.json(req.body);
  } catch (error) {
    console.error('Error saving position:', error);
    res.status(500).json({ error: 'Failed to save position' });
  }
});

app.delete('/api/positions/:id', (req, res) => {
  try {
    const positions = JSON.parse(fs.readFileSync(STORAGE_FILE, 'utf8'));
    const filteredPositions = positions.filter(pos => pos.id !== req.params.id);
    fs.writeFileSync(STORAGE_FILE, JSON.stringify(filteredPositions, null, 2));
    res.json({ success: true });
  } catch (error) {
    console.error('Error deleting position:', error);
    res.status(500).json({ error: 'Failed to delete position' });
  }
});

app.put('/api/positions/:id', (req, res) => {
  try {
    const positions = JSON.parse(fs.readFileSync(STORAGE_FILE, 'utf8'));
    const index = positions.findIndex(pos => pos.id === req.params.id);
    if (index === -1) {
      return res.status(404).json({ error: 'Position not found' });
    }
    positions[index] = { ...positions[index], ...req.body };
    fs.writeFileSync(STORAGE_FILE, JSON.stringify(positions, null, 2));
    res.json(positions[index]);
  } catch (error) {
    console.error('Error updating position:', error);
    res.status(500).json({ error: 'Failed to update position' });
  }
});

// Service logs endpoint
app.get('/api/service-logs', (req, res) => {
    exec('journalctl -u armatron-control.service -n 100 --no-pager', (error, stdout, stderr) => {
        if (error) {
            console.error('Error fetching service logs:', error);
            return res.status(500).json({ error: 'Failed to fetch service logs' });
        }
        if (stderr) {
            console.error('Stderr from journalctl:', stderr);
        }
        res.json({ logs: stdout });
    });
});

// Handle client-side routing - must be after static file serving
app.get('*', (req, res) => {
    res.sendFile(path.join(__dirname, 'dist', 'index.html'));
});

// Example route
app.get('/hello', (req, res) => {
    res.send("Hello from Node web_app!");
});

function startLogStream() {
    if (logStreamProcess) {
        console.log("[Node] Log stream already running");
        return;
    }

    console.log("[Node] Starting log stream");
    logStreamProcess = exec('journalctl -u armatron-control.service -f --no-pager', (error) => {
        if (error) {
            console.error('[Node] Error in log stream:', error);
            logStreamProcess = null;
            setTimeout(startLogStream, 1000);
        }
    });

    logStreamProcess.stdout.on('data', (data) => {
        const logLines = data.toString().split('\n').filter(line => line.trim());
        if (logLines.length > 0) {
            console.log(`[Node] Sending ${logLines.length} log lines to ${logStreamClients.size} clients`);
            io.emit('serviceLogs', { logs: logLines });
        }
    });

    logStreamProcess.stderr.on('data', (data) => {
        console.error('[Node] Log stream stderr:', data.toString());
    });
}

function stopLogStream() {
    if (logStreamProcess) {
        console.log("[Node] Stopping log stream");
        logStreamProcess.kill();
        logStreamProcess = null;
    }
}

// Socket.IO events from the browser
io.on('connection', (socket) => {
    console.log("[Node] Browser connected via socket.io");

    socket.on('subscribeToLogs', () => {
        console.log(`[Node] Client ${socket.id} subscribed to logs`);
        logStreamClients.add(socket.id);
        if (!logStreamProcess) {
            startLogStream();
        }
    });

    socket.on('unsubscribeFromLogs', () => {
        console.log(`[Node] Client ${socket.id} unsubscribed from logs`);
        logStreamClients.delete(socket.id);
        if (logStreamClients.size === 0) {
            stopLogStream();
        }
    });

    socket.on('sendCommand', (msg) => {
        console.log("[Node] Received command from browser:", msg);
        // Forward command to daemon
        if (client) {
            let line = JSON.stringify(msg) + "\n";
            client.write(line);
            console.log("[Node] Command forwarded to daemon:", line);
        } else {
            console.error("[Node] No client connection available to forward command.");
        }
    });

    socket.on('disconnect', () => {
        console.log("[Node] Browser disconnected");
        logStreamClients.delete(socket.id);
        if (logStreamClients.size === 0) {
            stopLogStream();
        }
    });
});

// Start the server
const PORT = process.env.PORT || 8888;
httpServer.listen(PORT, () => {
    console.log(`[Node] Web server running on http://<BBB_IP>:${PORT}`);
});

// Connect to the daemon
connectToDaemon(); 