import React, { useState, useEffect } from 'react';
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
} from 'chart.js';
import { Line } from 'react-chartjs-2';
import { motorControlService } from '../services/motorControl';
import { socketService } from '../services/socket';

// Register Chart.js components
ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
);

interface MotorState {
  temp: number;
  torqueA: number;
  speedDeg_s: number;
  posDeg: number;
  encoder_val?: number;
  error?: number;
  positionRad_Mapped?: number;
  positionDeg_Mapped?: number;
  multiTurnRaw: number;
  multiTurnRad_Mapped?: number;
  multiTurnDeg_Mapped?: number;
}

interface Props {
  jointId: number;
}

const JointCard: React.FC<Props> = ({ jointId }) => {
  const [motorState, setMotorState] = useState<MotorState | null>(null);
  const [torqueValue, setTorqueValue] = useState<number>(50);
  const [positionValue, setPositionValue] = useState<number>(90);
  const [speedValue, setSpeedValue] = useState<number>(360);
  const [chartData, setChartData] = useState({
    labels: [] as string[],
    datasets: [
      { label: 'Position (deg)', data: [] as number[], borderColor: 'blue', fill: false },
      { label: 'Speed (deg/s)', data: [] as number[], borderColor: 'red', fill: false },
      { label: 'Torque', data: [] as number[], borderColor: 'purple', fill: false }
    ]
  });

  const updateChart = (newData: MotorState) => {
    const now = new Date().toLocaleTimeString();
    setChartData(prev => {
      const newLabels = [...prev.labels, now];
      const newPositionData = [...prev.datasets[0].data, newData.multiTurnDeg_Mapped??0];
      const newSpeedData = [...prev.datasets[1].data, newData.speedDeg_s];
      const newTorqueData = [...prev.datasets[2].data, newData.torqueA];

      if (newLabels.length > 200) {
        newLabels.shift();
        newPositionData.shift();
        newSpeedData.shift();
        newTorqueData.shift();
      }

      return {
        labels: newLabels,
        datasets: [
          { ...prev.datasets[0], data: newPositionData },
          { ...prev.datasets[1], data: newSpeedData },
          { ...prev.datasets[2], data: newTorqueData }
        ]
      };
    });
  };

  useEffect(() => {
    const unsubscribe = socketService.onMotorStates(data => {
      const motor = data.motors[String(jointId)];
      if (motor) {
        setMotorState(motor);
        updateChart(motor);
      }
    });

    return () => {
      unsubscribe();
    };
  }, [jointId]);

  return (
    <div className="card" id={`card-${jointId}`}>
      <h3>Joint {jointId}</h3>
      <div className="card-content">
        <div className="card-left">
          <div className="data-section">
            <div>Temperature: <span className="temp">{motorState?.temp.toFixed(2) || '--'}</span></div>
            <div>Torque: <span className="torque">{motorState?.torqueA.toFixed(2) || '--'}</span></div>
            <div>Speed: <span className="speed">{motorState?.speedDeg_s.toFixed(2) || '--'}</span></div>
            <div>Encoder: <span className="enc">{motorState?.encoder_val?.toFixed(6) || '--'}</span></div>
            <div>Error: <span className="error">{motorState?.error ? 'YES' : 'NO'}</span></div>
            <br />
            <div>[Single Turn] Raw Angle: <span className="position">{motorState?.posDeg.toFixed(6) || '--'}</span></div>
            <div>[Single Turn] Remapped Angle [rad]: <span className="pos_remapped_rad">{motorState?.positionRad_Mapped?.toFixed(6) || '--'}</span></div>
            <div>[Single Turn] Remapped Angle [deg]: <span className="pos_remapped_deg">{motorState?.positionDeg_Mapped?.toFixed(6) || '--'}</span></div>
            <br />
            <div>[Multi Turn] Raw Angle: <span className="position_multi">{motorState?.multiTurnRaw.toFixed(6) || '--'}</span></div>
            <div>[Multi Turn] Remapped Angle [rad]: <span className="pos_remapped_rad_multi">{motorState?.multiTurnRad_Mapped?.toFixed(6) || '--'}</span></div>
            <div>[Multi Turn] Remapped Angle [deg]: <span className="pos_remapped_deg_multi">{motorState?.multiTurnDeg_Mapped?.toFixed(6) || '--'}</span></div>
          </div>
          <div className="local-controls">
            <button onClick={() => motorControlService.motorOn(jointId)}>Motor ON</button>
            <button onClick={() => motorControlService.motorOff(jointId)}>Motor OFF</button>
            <button onClick={() => motorControlService.motorStop(jointId)}>Motor STOP</button>
            <button onClick={() => motorControlService.clearMotorError(jointId)}>Clear Error</button>
            <button onClick={() => motorControlService.syncSingleAndMulti(jointId)}>Sync Angles</button>
            <div>
              <label>Torque (raw):</label>
              <input type="number" value={torqueValue} onChange={(e) => setTorqueValue(Number(e.target.value))} />
              <button onClick={() => motorControlService.setTorque(jointId, torqueValue)}>Set Torque</button>
            </div>
            <div>
              <label>Position (deg):</label>
              <input type="number" value={positionValue} onChange={(e) => setPositionValue(Number(e.target.value))} />
              <label>MaxSpeed (dps):</label>
              <input type="number" value={speedValue} onChange={(e) => setSpeedValue(Number(e.target.value))} />
              <button onClick={() => motorControlService.setPosition(jointId, positionValue, speedValue)}>Set Position</button>
            </div>
          </div>
        </div>
        <div className="card-right">
          <Line data={chartData} options={{ animation: false, scales: { x: { display: false } } }} />
        </div>
      </div>
    </div>
  );
};

export default JointCard; 