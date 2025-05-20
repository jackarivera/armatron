import React from 'react';
import { Paper } from '@mui/material';
import JointCard from '../components/JointCard';
import MultiJointControl from '../components/MultiJointControl';
import GlobalControls from '../components/GlobalControls';

const JointControl: React.FC = () => {
  return (
    <Paper sx={{ p: 3 }}>
      <h1>Armatron Motor Tuner</h1>

      <MultiJointControl />

      <div id="cardsGrid" className="cards-grid">
        {[1, 2, 3, 4, 5, 6, 7].map((jointId) => (
          <JointCard key={jointId} jointId={jointId} />
        ))}
      </div>

      <GlobalControls />
    </Paper>
  );
};

export default JointControl; 