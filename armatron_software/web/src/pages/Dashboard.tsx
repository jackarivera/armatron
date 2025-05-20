import React, { useEffect, useState } from 'react';
import { Grid, Paper, Typography, List, ListItem, ListItemText, Chip, TextField, Button } from '@mui/material';
import RobotViewer from '../components/RobotViewer';
import JointPositionControl from '../components/JointPositionControl';
import ServiceLogsViewer from '../components/ServiceLogsViewer';
import { robotStateService } from '../services/robotState';
import { socketService } from '../services/socket';
import { motorControlService } from '../services/motorControl';
import { styled } from '@mui/material/styles';

// Styled emergency stop button
const EmergencyButton = styled(Button)(({ theme }) => ({
  backgroundColor: theme.palette.error.main,
  color: theme.palette.error.contrastText,
  '&:hover': {
    backgroundColor: theme.palette.error.dark,
  },
  marginTop: theme.spacing(2),
}));

const Dashboard: React.FC = () => {
  const [state, setState] = useState(robotStateService.getState());
  const [targetRoll, setTargetRoll] = useState(0);
  const [targetPitch, setTargetPitch] = useState(0);
  const [maxDiffMotorSpeed, setMaxDiffMotorSpeed] = useState(60); // Default to 100 deg/s

  useEffect(() => {
    const updateState = () => {
      setState(robotStateService.getState());
    };

    robotStateService.on('update', updateState);
    return () => {
      robotStateService.off('update', updateState);
    };
  }, []);

  const handleDifferentialMove = () => {
    // Convert degrees to radians for the command
    const rollRad = targetRoll * (Math.PI / 180);
    const pitchRad = targetPitch * (Math.PI / 180);

    socketService.sendCommand({
      cmd: 'setDifferentialAngles',
      roll: rollRad,
      pitch: pitchRad,
      maxSpeed: maxDiffMotorSpeed
    });
  };

  const handleEmergencyStop = () => {
    motorControlService.emergencyStop();
  };

  const formatMotorState = (motorId: string) => {
    const motor = state.motors[motorId];
    if (!motor) return 'Unknown';
    const motorNum = parseInt(motorId);
    const position = (motorNum >= 6) ? 
      (motor.multiTurnRaw?.toFixed(2) ?? '-1') :
      (motor.multiTurnDeg_Mapped?.toFixed(2) ?? '-1');
    return `Position: ${position}° | Speed: ${motor.speedDeg_s.toFixed(2)}°/s | Temp: ${motor.temp}°C`;
  };

  return (
    <div style={{ width: '100%' }}>
      <Grid container spacing={3}>
        {/* Left Column - Robot Status */}
        <Grid item xs={12} md={4}>
          <Paper sx={{ p: 2, height: '100%' }}>
            <Paper sx={{ p: 2, mb: 2, backgroundColor: 'transparent', boxShadow: 'none' }}>
              <Typography variant="h6" gutterBottom>
                Robot Status
              </Typography>
              <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                {state.connected ? (
                  <Chip 
                    label="Connected"
                    color="success"
                    size="small"
                  />
                ) : (
                  <Chip 
                    label="Disconnected"
                    color="error"
                    size="small"
                  />
                )}
                <Typography variant="caption" color="text.secondary">
                  Last Update: {new Date(state.lastUpdate).toLocaleTimeString()}
                </Typography>
              </div>
            </Paper>

            <Paper sx={{ p: 2, mb: 2 }}>
              <Typography variant="h6" gutterBottom>
                Wrist Control
              </Typography>
              <Paper sx={{ mb: 2 }}>
                <Typography variant="body2">
                  Current Roll: {state.diff_roll_deg?.toFixed(2)}°
                </Typography>
                <Typography variant="body2">
                  Current Pitch: {state.diff_pitch_deg?.toFixed(2)}°
                </Typography>
              </Paper>
              <Paper sx={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
                <TextField
                  label="Target Roll (deg)"
                  type="number"
                  value={targetRoll}
                  onChange={(e) => setTargetRoll(parseFloat(e.target.value) || 0)}
                  size="small"
                  inputProps={{ step: "any" }}
                />
                <TextField
                  label="Target Pitch (deg)"
                  type="number"
                  value={targetPitch}
                  onChange={(e) => setTargetPitch(parseFloat(e.target.value) || 0)}
                  size="small"
                  inputProps={{ step: "any" }}
                />
                <TextField
                  label="Max Motor Speed (deg/s)"
                  type="number"
                  value={maxDiffMotorSpeed}
                  onChange={(e) => setMaxDiffMotorSpeed(parseFloat(e.target.value) || 0)}
                  size="small"
                  inputProps={{ step: "any" }}
                />
                <Button 
                  variant="contained" 
                  onClick={handleDifferentialMove}
                  disabled={!state.connected}
                >
                  Move Wrist
                </Button>
              </Paper>
            </Paper>

            <Paper sx={{ p: 2, mb: 2 }}>
              <Typography variant="h6" gutterBottom>
                Emergency Stop
              </Typography>
              <EmergencyButton
                variant="contained"
                onClick={handleEmergencyStop}
                disabled={!state.connected}
                fullWidth
              >
                EMERGENCY STOP
              </EmergencyButton>
            </Paper>

            <Typography variant="h6" gutterBottom>
              Motor States
            </Typography>
            <List dense>
              {Object.keys(state.motors).map((motorId) => (
                <ListItem key={motorId}>
                  <ListItemText
                    primary={`Motor ${motorId}`}
                    secondary={formatMotorState(motorId)}
                    primaryTypographyProps={{ variant: 'body2' }}
                    secondaryTypographyProps={{ variant: 'caption' }}
                  />
                </ListItem>
              ))}
            </List>
          </Paper>
        </Grid>

        {/* Middle Column - Joint Position Control */}
        <Grid item xs={12} md={4}>
          <JointPositionControl />
        </Grid>

        {/* Right Column - 3D Viewer */}
        <Grid item xs={12} md={4}>
          <Paper sx={{ p: 2, height: '600px' }}>
            <RobotViewer />
          </Paper>
          <ServiceLogsViewer />
        </Grid>
      </Grid>
    </div>
  );
};

export default Dashboard; 