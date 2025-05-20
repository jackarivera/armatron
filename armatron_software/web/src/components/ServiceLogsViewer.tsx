import React, { useEffect, useState, useRef } from 'react';
import { Paper, Typography } from '@mui/material';
import { styled } from '@mui/material/styles';
import { socketService } from '../services/socket';

const LogContainer = styled(Paper)(({ theme }) => ({
  padding: theme.spacing(2),
  height: '300px',
  overflow: 'auto',
  backgroundColor: theme.palette.background.default,
  fontFamily: 'monospace',
  whiteSpace: 'pre-wrap',
  wordBreak: 'break-word',
}));

const LogLine = styled('div')(({ theme }) => ({
  marginBottom: theme.spacing(0.5),
  fontSize: '0.875rem',
  lineHeight: 1.5,
}));

const ServiceLogsViewer: React.FC = () => {
  const [logs, setLogs] = useState<string[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [connected, setConnected] = useState(false);
  const logContainerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    // Ensure socket is connected
    if (!socketService.isConnected()) {
      socketService.connect();
    }

    const handleConnect = () => {
      console.log('Socket connected, subscribing to logs');
      setConnected(true);
      socketService.emit('subscribeToLogs');
    };

    const handleDisconnect = () => {
      console.log('Socket disconnected');
      setConnected(false);
    };

    const handleLogs = (data: { logs: string[] }) => {
      console.log('Received logs:', data.logs);
      setLogs(prevLogs => [...prevLogs, ...data.logs].slice(-1000)); // Keep last 1000 lines
      setError(null);
    };

    const handleError = (err: Error) => {
      console.error('Log error:', err);
      setError(err.message);
    };

    socketService.on('connect', handleConnect);
    socketService.on('disconnect', handleDisconnect);
    socketService.on('serviceLogs', handleLogs);
    socketService.on('error', handleError);

    // Initial subscription if already connected
    if (socketService.isConnected()) {
      handleConnect();
    }

    return () => {
      socketService.off('connect', handleConnect);
      socketService.off('disconnect', handleDisconnect);
      socketService.off('serviceLogs', handleLogs);
      socketService.off('error', handleError);
      socketService.emit('unsubscribeFromLogs');
    };
  }, []);

  // Auto-scroll to bottom when new logs arrive
  useEffect(() => {
    if (logContainerRef.current) {
      logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;
    }
  }, [logs]);

  return (
    <Paper sx={{ p: 2, mt: 2 }}>
      <Typography variant="h6" gutterBottom>
        Service Logs {!connected && '(Connecting...)'}
      </Typography>
      {error ? (
        <Typography color="error">{error}</Typography>
      ) : (
        <LogContainer ref={logContainerRef}>
          {logs.length === 0 && connected ? (
            <LogLine>Waiting for logs...</LogLine>
          ) : (
            logs.map((line, index) => (
              <LogLine key={`${index}-${line}`}>{line}</LogLine>
            ))
          )}
        </LogContainer>
      )}
    </Paper>
  );
};

export default ServiceLogsViewer; 