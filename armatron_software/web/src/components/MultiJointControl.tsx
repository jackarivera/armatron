import React, { useState, useEffect } from 'react';
import { motorControlService } from '../services/motorControl';
import SavedSetsList from './SavedSetsList';

interface SavedSet {
  angles: number[];
  speeds: number[];
}

const MultiJointControl: React.FC = () => {
  const [angles, setAngles] = useState<number[]>([-1, -1, -1, -1, -1, -1, -1]);
  const [speeds, setSpeeds] = useState<number[]>([30, 30, 30, 30, 30, 30, 30]);
  const [setName, setSetName] = useState<string>('');
  const [savedSets, setSavedSets] = useState<Record<string, SavedSet>>({});

  // Load saved sets from localStorage on component mount
  useEffect(() => {
    const saved = localStorage.getItem('savedAngleSets');
    if (saved) {
      setSavedSets(JSON.parse(saved));
    }
  }, []);

  // Save sets to localStorage whenever they change
  useEffect(() => {
    localStorage.setItem('savedAngleSets', JSON.stringify(savedSets));
  }, [savedSets]);

  const handleAngleChange = (index: number, value: string) => {
    const newAngles = [...angles];
    newAngles[index] = parseFloat(value) || 0;
    setAngles(newAngles);
  };

  const handleSpeedChange = (index: number, value: string) => {
    const newSpeeds = [...speeds];
    newSpeeds[index] = parseFloat(value) || 0;
    setSpeeds(newSpeeds);
  };

  const handleSendCommand = () => {
    motorControlService.setMultiJointAngles(angles, speeds);
  };

  const handleEmergencyStop = () => {
    motorControlService.emergencyStop();
  };

  const handleSaveSet = () => {
    if (!setName.trim()) {
      alert('Please provide a name for the set');
      return;
    }
    setSavedSets(prev => ({
      ...prev,
      [setName]: { angles: [...angles], speeds: [...speeds] }
    }));
    setSetName('');
    alert(`Set saved as '${setName}'`);
  };

  const handleDeleteSet = (name: string) => {
    setSavedSets(prev => {
      const newSets = { ...prev };
      delete newSets[name];
      return newSets;
    });
  };

  const handleUpdateSet = (name: string, set: SavedSet) => {
    setSavedSets(prev => ({
      ...prev,
      [name]: set
    }));
  };

  return (
    <div id="multiJointControl" className="container">
      <h2>Multi Joint Setpoints</h2>
      <div className="multi-joint-grid">
        {[1, 2, 3, 4, 5, 6, 7].map((jointId) => (
          <div key={jointId} className="multi-joint-column" data-joint={jointId}>
            <h3>Joint {jointId}</h3>
            <label>Angle (deg):</label>
            <input
              type="number"
              id={`multiAngle-${jointId}`}
              value={angles[jointId - 1]}
              onChange={(e) => handleAngleChange(jointId - 1, e.target.value)}
            />
            <label>Max Speed (dps):</label>
            <input
              type="number"
              id={`multiSpeed-${jointId}`}
              value={speeds[jointId - 1]}
              onChange={(e) => handleSpeedChange(jointId - 1, e.target.value)}
            />
          </div>
        ))}
      </div>
      <div className="multi-joint-actions">
        <button id="sendMultiJoint" onClick={handleSendCommand}>Send Multi Joint Command</button>
        <button id="emergencyStop" className="estop" onClick={handleEmergencyStop}>ESTOP</button>
      </div>
      <div className="saved-sets">
        <h3>Saved Angle Sets</h3>
        <div className="save-set-inputs">
          <input
            type="text"
            value={setName}
            onChange={(e) => setSetName(e.target.value)}
            placeholder="Name this set"
          />
          <button onClick={handleSaveSet}>Save Set</button>
        </div>
        <SavedSetsList
          savedSets={savedSets}
          onDeleteSet={handleDeleteSet}
          onUpdateSet={handleUpdateSet}
        />
      </div>
    </div>
  );
};

export default MultiJointControl; 