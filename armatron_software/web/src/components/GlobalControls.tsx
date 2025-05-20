import React, { useState, useEffect } from 'react';
import { motorControlService } from '../services/motorControl';
import { robotStateService } from '../services/robotState';
import { styled } from '@mui/material/styles';

const CurrentGainsContainer = styled('div')(({ theme }) => ({
  marginBottom: theme.spacing(2),
  padding: theme.spacing(2),
  backgroundColor: theme.palette.background.paper,
  borderRadius: theme.shape.borderRadius,
  boxShadow: theme.shadows[1],
}));

const GainsGrid = styled('div')(({ theme }) => ({
  display: 'grid',
  gridTemplateColumns: 'repeat(2, 1fr)',
  gap: theme.spacing(1),
  marginTop: theme.spacing(1),
}));

const GainsRow = styled('div')(({ theme }) => ({
  display: 'flex',
  justifyContent: 'space-between',
  alignItems: 'center',
  padding: theme.spacing(0.5),
  borderBottom: `1px solid ${theme.palette.divider}`,
}));

const GainValue = styled('span')(({ theme }) => ({
  fontWeight: 'bold',
  color: theme.palette.primary.main,
}));

const GlobalControls: React.FC = () => {
  const [selectedJoint, setSelectedJoint] = useState<number>(1);
  const [pidGains, setPidGains] = useState({
    angKp: 100,
    angKi: 50,
    spdKp: 50,
    spdKi: 20,
    iqKp: 50,
    iqKi: 50
  });
  const [encoderValue, setEncoderValue] = useState<number>(0);
  const [accelVal, setAccelVal] = useState<number>(1000);
  const [currentGains, setCurrentGains] = useState<{
    angKp: number;
    angKi: number;
    spdKp: number;
    spdKi: number;
    iqKp: number;
    iqKi: number;
  } | null>(null);

  // Subscribe to state updates to get current gains
  useEffect(() => {
    const handleStateUpdate = () => {
      const state = robotStateService.getState();
      const motorState = state.motors[selectedJoint.toString()];
      console.log('[GlobalControls] State update received:', motorState);
      if (motorState?.gains) {
        console.log('[GlobalControls] Updating gains:', motorState.gains);
        setCurrentGains(motorState.gains);
        // Also update the input fields to match current gains
        setPidGains({
          angKp: motorState.gains.angKp,
          angKi: motorState.gains.angKi,
          spdKp: motorState.gains.spdKp,
          spdKi: motorState.gains.spdKi,
          iqKp: motorState.gains.iqKp,
          iqKi: motorState.gains.iqKi
        });
      }
    };

    // Initial state check
    handleStateUpdate();

    // Subscribe to updates
    robotStateService.on('update', handleStateUpdate);
    return () => {
      robotStateService.off('update', handleStateUpdate);
    };
  }, [selectedJoint]);

  const handlePidChange = (field: string, value: string) => {
    setPidGains(prev => ({
      ...prev,
      [field]: parseInt(value) || 0
    }));
  };

  const handleReadPid = () => {
    console.log('[GlobalControls] Reading PID for joint:', selectedJoint);
    motorControlService.readPID(selectedJoint);
  };

  const handleWritePidRAM = () => {
    motorControlService.writePID_RAM(
      selectedJoint,
      pidGains.angKp,
      pidGains.angKi,
      pidGains.spdKp,
      pidGains.spdKi,
      pidGains.iqKp,
      pidGains.iqKi
    );
  };

  const handleWritePidROM = () => {
    motorControlService.writePID_ROM(
      selectedJoint,
      pidGains.angKp,
      pidGains.angKi,
      pidGains.spdKp,
      pidGains.spdKi,
      pidGains.iqKp,
      pidGains.iqKi
    );
  };

  const handleReadEncoder = () => {
    motorControlService.readEncoder(selectedJoint);
  };

  const handleWriteEncoderZero = () => {
    motorControlService.writeEncoderZero(selectedJoint, encoderValue);
  };

  const handleWriteCurrentPosZero = () => {
    motorControlService.writeCurrentPosAsZero(selectedJoint);
  };

  const handleWriteAcceleration = () => {
    motorControlService.writeAcceleration(selectedJoint, accelVal);
  };

  const handleReadAcceleration = () => {
    motorControlService.readAcceleration(selectedJoint);
  };

  return (
    <div id="globalControls" className="container global-controls">
      <h2>Global Controls</h2>
      
      <div className="joint-selector">
        <label htmlFor="jointSelect">Select Joint:</label>
        <select
          id="jointSelect"
          value={selectedJoint}
          onChange={(e) => setSelectedJoint(parseInt(e.target.value))}
        >
          {[1, 2, 3, 4, 5, 6, 7].map((joint) => (
            <option key={joint} value={joint}>Joint {joint}</option>
          ))}
        </select>
      </div>

      <div className="pid-controls">
        <h3>PID Control</h3>
        
        {/* Current Gains Display */}
        <CurrentGainsContainer>
          <h4>Current Gains</h4>
          {currentGains ? (
            <GainsGrid>
              <GainsRow>
                <span>Angle Kp:</span>
                <GainValue>{currentGains.angKp}</GainValue>
              </GainsRow>
              <GainsRow>
                <span>Angle Ki:</span>
                <GainValue>{currentGains.angKi}</GainValue>
              </GainsRow>
              <GainsRow>
                <span>Speed Kp:</span>
                <GainValue>{currentGains.spdKp}</GainValue>
              </GainsRow>
              <GainsRow>
                <span>Speed Ki:</span>
                <GainValue>{currentGains.spdKi}</GainValue>
              </GainsRow>
              <GainsRow>
                <span>IQ Kp:</span>
                <GainValue>{currentGains.iqKp}</GainValue>
              </GainsRow>
              <GainsRow>
                <span>IQ Ki:</span>
                <GainValue>{currentGains.iqKi}</GainValue>
              </GainsRow>
            </GainsGrid>
          ) : (
            <p>No gains data available. Click "Read PID Gains" to fetch current values.</p>
          )}
        </CurrentGainsContainer>

        <div className="pid-inputs">
          <div>
            <label htmlFor="angKp">Angle Kp:</label>
            <input
              type="number"
              id="angKp"
              value={pidGains.angKp}
              onChange={(e) => handlePidChange('angKp', e.target.value)}
            />
          </div>
          <div>
            <label htmlFor="angKi">Angle Ki:</label>
            <input
              type="number"
              id="angKi"
              value={pidGains.angKi}
              onChange={(e) => handlePidChange('angKi', e.target.value)}
            />
          </div>
          <div>
            <label htmlFor="spdKp">Speed Kp:</label>
            <input
              type="number"
              id="spdKp"
              value={pidGains.spdKp}
              onChange={(e) => handlePidChange('spdKp', e.target.value)}
            />
          </div>
          <div>
            <label htmlFor="spdKi">Speed Ki:</label>
            <input
              type="number"
              id="spdKi"
              value={pidGains.spdKi}
              onChange={(e) => handlePidChange('spdKi', e.target.value)}
            />
          </div>
          <div>
            <label htmlFor="iqKp">IQ Kp:</label>
            <input
              type="number"
              id="iqKp"
              value={pidGains.iqKp}
              onChange={(e) => handlePidChange('iqKp', e.target.value)}
            />
          </div>
          <div>
            <label htmlFor="iqKi">IQ Ki:</label>
            <input
              type="number"
              id="iqKi"
              value={pidGains.iqKi}
              onChange={(e) => handlePidChange('iqKi', e.target.value)}
            />
          </div>
        </div>
        <div className="pid-actions">
          <button onClick={handleReadPid}>Read PID Gains</button>
          <button onClick={handleWritePidRAM}>Write Gains to RAM</button>
          <button onClick={handleWritePidROM}>Write Gains to ROM</button>
        </div>
      </div>

      <div className="encoder-controls">
        <h3>Encoder</h3>
        <div className="encoder-input">
          <label htmlFor="encoderValue">Encoder Value:</label>
          <input
            type="number"
            id="encoderValue"
            value={encoderValue}
            onChange={(e) => setEncoderValue(parseInt(e.target.value) || 0)}
          />
        </div>
        <div className="encoder-actions">
          <button onClick={handleReadEncoder}>Read Encoder</button>
          <button onClick={handleWriteEncoderZero}>Set Encoder Zero</button>
          <button onClick={handleWriteCurrentPosZero}>Write Current Pos as Zero</button>
        </div>
      </div>

      <div className="accel-controls">
        <h3>Acceleration</h3>
        <div className="accel-input">
          <label htmlFor="accelVal">Acceleration Value (1 dps^2):</label>
          <input
            type="number"
            id="accelVal"
            value={accelVal}
            onChange={(e) => setAccelVal(parseInt(e.target.value) || 0)}
          />
        </div>
        <div className="accel-actions">
          <button onClick={handleWriteAcceleration}>Write Acceleration</button>
          <button onClick={handleReadAcceleration}>Read Acceleration</button>
        </div>
      </div>
    </div>
  );
};

export default GlobalControls; 