import React, { useState, useEffect } from 'react';
import { 
  Paper, 
  Typography, 
  TextField, 
  Button, 
  List, 
  ListItem, 
  ListItemText, 
  ListItemSecondaryAction,
  IconButton,
  Grid,
  Dialog,
  DialogTitle,
  DialogContent,
  DialogActions
} from '@mui/material';
import DeleteIcon from '@mui/icons-material/Delete';
import PlayArrowIcon from '@mui/icons-material/PlayArrow';
import EditIcon from '@mui/icons-material/Edit';
import ContentCopyIcon from '@mui/icons-material/ContentCopy';
import { socketService } from '../services/socket';

interface SavedPosition {
  id: string;
  name: string;
  angles: number[];
}

interface JointPositionControlProps {
  // Add any props if needed in the future
}

const JointPositionControl: React.FC<JointPositionControlProps> = () => {
  const [jointAngles, setJointAngles] = useState<number[]>([0, 0, 0, 0, 0, 0, 0]);
  const [positionName, setPositionName] = useState<string>('');
  const [savedPositions, setSavedPositions] = useState<SavedPosition[]>([]);
  const [editingPosition, setEditingPosition] = useState<SavedPosition | null>(null);
  const [editDialogOpen, setEditDialogOpen] = useState(false);

  useEffect(() => {
    // Load saved positions from server
    fetch('/api/positions')
      .then(res => res.json())
      .then((data: SavedPosition[]) => setSavedPositions(data))
      .catch(err => console.error('Error loading positions:', err));
  }, []);

  const handleAngleChange = (index: number, value: string): void => {
    const newAngles = [...jointAngles];
    newAngles[index] = parseFloat(value) || 0;
    setJointAngles(newAngles);
  };

  const generateUniqueName = (baseName: string): string => {
    let newName = baseName;
    let counter = 1;
    while (savedPositions.some(pos => pos.name === newName)) {
      newName = `${baseName}_${counter}`;
      counter++;
    }
    return newName;
  };

  const handleSavePosition = (): void => {
    if (!positionName.trim()) return;

    const newPosition: SavedPosition = {
      id: Date.now().toString(),
      name: positionName,
      angles: [...jointAngles]
    };

    // Save to server
    fetch('/api/positions', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(newPosition),
    })
      .then(res => res.json())
      .then((savedPosition: SavedPosition) => {
        setSavedPositions(prev => [...prev, savedPosition]);
        setPositionName('');
      })
      .catch(err => console.error('Error saving position:', err));
  };

  const handleDeletePosition = (id: string): void => {
    fetch(`/api/positions/${id}`, {
      method: 'DELETE',
    })
      .then(() => {
        setSavedPositions(prev => prev.filter(pos => pos.id !== id));
      })
      .catch(err => console.error('Error deleting position:', err));
  };

  const handleRunPosition = (angles: number[]): void => {
    socketService.sendCommand({
      cmd: 'moveToJointPositionRuckig',
      angles: angles
    });
  };

  const handleEditPosition = (position: SavedPosition): void => {
    setEditingPosition(position);
    setEditDialogOpen(true);
  };

  const handleSaveEdit = (): void => {
    if (!editingPosition) return;

    fetch(`/api/positions/${editingPosition.id}`, {
      method: 'PUT',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(editingPosition),
    })
      .then(res => res.json())
      .then((updatedPosition: SavedPosition) => {
        setSavedPositions(prev => 
          prev.map(pos => pos.id === updatedPosition.id ? updatedPosition : pos)
        );
        setEditDialogOpen(false);
        setEditingPosition(null);
      })
      .catch(err => console.error('Error updating position:', err));
  };

  const handleDuplicatePosition = (position: SavedPosition): void => {
    const newPosition: SavedPosition = {
      id: Date.now().toString(),
      name: generateUniqueName(position.name),
      angles: [...position.angles]
    };

    fetch('/api/positions', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(newPosition),
    })
      .then(res => res.json())
      .then((savedPosition: SavedPosition) => {
        setSavedPositions(prev => [...prev, savedPosition]);
      })
      .catch(err => console.error('Error duplicating position:', err));
  };

  const jointLabels: string[] = [
    'Joint 1',
    'Joint 2',
    'Joint 3',
    'Joint 4',
    'Joint 5',
    'Wrist Roll',
    'Wrist Pitch'
  ];

  return (
    <Paper sx={{ p: 2, height: '100%', display: 'flex', flexDirection: 'column' }}>
      <Typography variant="h6" gutterBottom>
        Joint Position Control
      </Typography>

      <Grid container spacing={1} sx={{ mb: 2 }}>
        {jointLabels.map((label, index) => (
          <Grid item xs={12} sm={6} md={4} lg={3} key={index}>
            <TextField
              fullWidth
              label={label}
              type="number"
              value={jointAngles[index]}
              onChange={(e: React.ChangeEvent<HTMLInputElement>) => handleAngleChange(index, e.target.value)}
              size="small"
              inputProps={{ step: "0.1" }}
            />
          </Grid>
        ))}
      </Grid>

      <Grid container spacing={2} sx={{ mb: 2 }}>
        <Grid item xs={8}>
          <TextField
            fullWidth
            label="Position Name"
            value={positionName}
            onChange={(e: React.ChangeEvent<HTMLInputElement>) => setPositionName(e.target.value)}
            size="small"
          />
        </Grid>
        <Grid item xs={4}>
          <Button
            fullWidth
            variant="contained"
            onClick={handleSavePosition}
            disabled={!positionName.trim()}
          >
            Save Position
          </Button>
        </Grid>
      </Grid>

      <Typography variant="subtitle1" gutterBottom>
        Saved Positions
      </Typography>
      <List dense sx={{ flexGrow: 1, overflow: 'auto' }}>
        {savedPositions.map((position) => (
          <ListItem key={position.id}>
            <ListItemText
              primary={position.name}
              secondary={`Angles: ${position.angles.map(a => a.toFixed(1)).join(', ')}`}
            />
            <ListItemSecondaryAction>
              <IconButton
                edge="end"
                aria-label="run"
                onClick={() => handleRunPosition(position.angles)}
                sx={{ mr: 1 }}
              >
                <PlayArrowIcon />
              </IconButton>
              <IconButton
                edge="end"
                aria-label="edit"
                onClick={() => handleEditPosition(position)}
                sx={{ mr: 1 }}
              >
                <EditIcon />
              </IconButton>
              <IconButton
                edge="end"
                aria-label="duplicate"
                onClick={() => handleDuplicatePosition(position)}
                sx={{ mr: 1 }}
              >
                <ContentCopyIcon />
              </IconButton>
              <IconButton
                edge="end"
                aria-label="delete"
                onClick={() => handleDeletePosition(position.id)}
              >
                <DeleteIcon />
              </IconButton>
            </ListItemSecondaryAction>
          </ListItem>
        ))}
      </List>

      <Dialog open={editDialogOpen} onClose={() => setEditDialogOpen(false)}>
        <DialogTitle>Edit Position</DialogTitle>
        <DialogContent>
          <TextField
            autoFocus
            margin="dense"
            label="Position Name"
            fullWidth
            value={editingPosition?.name || ''}
            onChange={(e: React.ChangeEvent<HTMLInputElement>) => 
              setEditingPosition(prev => prev ? {...prev, name: e.target.value} : null)
            }
          />
          <Grid container spacing={1} sx={{ mt: 2 }}>
            {jointLabels.map((label, index) => (
              <Grid item xs={12} sm={6} md={4} lg={3} key={index}>
                <TextField
                  fullWidth
                  label={label}
                  type="number"
                  value={editingPosition?.angles[index] || 0}
                  onChange={(e: React.ChangeEvent<HTMLInputElement>) => {
                    const newAngles = [...(editingPosition?.angles || [])];
                    newAngles[index] = parseFloat(e.target.value) || 0;
                    setEditingPosition(prev => prev ? {...prev, angles: newAngles} : null);
                  }}
                  size="small"
                  inputProps={{ step: "0.1" }}
                />
              </Grid>
            ))}
          </Grid>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setEditDialogOpen(false)}>Cancel</Button>
          <Button onClick={handleSaveEdit} variant="contained">Save</Button>
        </DialogActions>
      </Dialog>
    </Paper>
  );
};

export default JointPositionControl; 