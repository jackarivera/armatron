import React, { useState } from 'react';
import {
  Dialog,
  DialogTitle,
  DialogContent,
  DialogActions,
  Button,
  Slider,
  TextField,
  Typography,
  Paper,
} from '@mui/material';

interface SettingsDialogProps {
  open: boolean;
  onClose: () => void;
  onSetModifier: (value: number) => void;
}

const SettingsDialog: React.FC<SettingsDialogProps> = ({ open, onClose, onSetModifier }) => {
  const [modifier, setModifier] = useState<number>(1);
  const [textValue, setTextValue] = useState<string>('1');

  const handleSliderChange = (_event: Event, newValue: number | number[]) => {
    const value = newValue as number;
    setModifier(value);
    setTextValue(value.toString());
  };

  const handleTextChange = (event: React.ChangeEvent<HTMLInputElement>) => {
    const value = event.target.value;
    setTextValue(value);
    const numValue = parseFloat(value);
    if (!isNaN(numValue) && numValue >= 0 && numValue <= 1) {
      setModifier(numValue);
    }
  };

  const handleSetModifier = () => {
    onSetModifier(modifier);
  };

  return (
    <Dialog open={open} onClose={onClose} maxWidth="sm" fullWidth>
      <DialogTitle>Settings</DialogTitle>
      <DialogContent>
        <Paper sx={{ mt: 2 }}>
          <Typography gutterBottom>Max Speed Modifier</Typography>
          <Paper sx={{ display: 'flex', alignItems: 'center', gap: 2 }}>
            <Slider
              value={modifier}
              onChange={handleSliderChange}
              min={0}
              max={1}
              step={0.01}
              sx={{ flex: 1 }}
            />
            <TextField
              size="small"
              value={textValue}
              onChange={handleTextChange}
              inputProps={{
                min: 0,
                max: 1,
                step: 0.01,
                type: 'number',
              }}
              sx={{ width: '100px' }}
            />
          </Paper>
        </Paper>
      </DialogContent>
      <DialogActions>
        <Button onClick={onClose}>Cancel</Button>
        <Button onClick={handleSetModifier} variant="contained">
          Set Modifier
        </Button>
      </DialogActions>
    </Dialog>
  );
};

export default SettingsDialog; 