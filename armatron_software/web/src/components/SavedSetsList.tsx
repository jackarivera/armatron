import React, { useState } from 'react';
import { motorControlService } from '../services/motorControl';

interface SavedSet {
  angles: number[];
  speeds: number[];
}

interface SavedSetsListProps {
  savedSets: Record<string, SavedSet>;
  onDeleteSet: (name: string) => void;
  onUpdateSet: (name: string, set: SavedSet) => void;
}

const SavedSetsList: React.FC<SavedSetsListProps> = ({ savedSets, onDeleteSet, onUpdateSet }) => {
  const [editingSet, setEditingSet] = useState<string | null>(null);
  const [editedSet, setEditedSet] = useState<SavedSet | null>(null);
  const [hasChanges, setHasChanges] = useState(false);

  const handleEditStart = (name: string) => {
    setEditingSet(name);
    setEditedSet({ ...savedSets[name] });
    setHasChanges(false);
  };

  const handleEditCancel = () => {
    setEditingSet(null);
    setEditedSet(null);
    setHasChanges(false);
  };

  const handleEditSave = () => {
    if (editingSet && editedSet) {
      onUpdateSet(editingSet, editedSet);
      setEditingSet(null);
      setEditedSet(null);
      setHasChanges(false);
    }
  };

  const handleValueChange = (jointIndex: number, field: 'angle' | 'speed', value: string) => {
    if (editedSet) {
      const newSet = { ...editedSet };
      if (field === 'angle') {
        newSet.angles[jointIndex] = parseFloat(value) || 0;
      } else {
        newSet.speeds[jointIndex] = parseFloat(value) || 0;
      }
      setEditedSet(newSet);
      setHasChanges(true);
    }
  };

  const handleSetAngles = (set: SavedSet) => {
    motorControlService.setMultiJointAngles(set.angles, set.speeds);
  };

  const handleDelete = (name: string) => {
    if (window.confirm(`Are you sure you want to delete the set "${name}"?`)) {
      onDeleteSet(name);
    }
  };

  return (
    <div className="saved-sets-list">
      {Object.entries(savedSets).map(([name, set]) => (
        <div key={name} className="saved-set-item">
          <div className="set-header">
            <h4>{name}</h4>
            <div className="set-actions">
              <button onClick={() => handleSetAngles(set)}>Set Angles</button>
              <button onClick={() => handleDelete(name)}>🗑️</button>
            </div>
          </div>
          <div className="set-values">
            {[1, 2, 3, 4, 5, 6, 7].map((jointId) => (
              <div key={jointId} className="joint-value">
                <span className="joint-label">Joint {jointId}:</span>
                {editingSet === name ? (
                  <>
                    <input
                      type="number"
                      value={editedSet?.angles[jointId - 1]}
                      onChange={(e) => handleValueChange(jointId - 1, 'angle', e.target.value)}
                      className="editable-input"
                    />
                    <input
                      type="number"
                      value={editedSet?.speeds[jointId - 1]}
                      onChange={(e) => handleValueChange(jointId - 1, 'speed', e.target.value)}
                      className="editable-input"
                    />
                  </>
                ) : (
                  <>
                    <span className="value-display">{set.angles[jointId - 1]}°</span>
                    <span className="value-display">{set.speeds[jointId - 1]} dps</span>
                  </>
                )}
              </div>
            ))}
          </div>
          {editingSet === name ? (
            <div className="edit-actions">
              <button onClick={handleEditSave} disabled={!hasChanges}>Save Changes</button>
              <button onClick={handleEditCancel}>Cancel</button>
            </div>
          ) : (
            <button onClick={() => handleEditStart(name)}>Edit</button>
          )}
        </div>
      ))}
    </div>
  );
};

export default SavedSetsList; 