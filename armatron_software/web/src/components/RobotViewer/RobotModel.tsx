import React, { useEffect, useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import URDFLoader from 'urdf-loader';
import { robotStateService } from '../../services/robotState';
import * as THREE from 'three';

interface RobotModelProps {
  // Add any props needed for the model
}

const RobotModel: React.FC<RobotModelProps> = () => {
  const modelRef = useRef<THREE.Group>(null);
  const robot = useRef<THREE.Object3D | null>(null);

  useEffect(() => {
    // Initialize the URDF loader
    const loader = new URDFLoader();
    
    // Set the package path to the directory containing the URDF file
    loader.packages = {
      '': '/models/urdf/' // This tells the loader where to find the meshes relative to the URDF file
    };
    
    // Load the URDF file
    loader.load(
      '/models/urdf/armatron.urdf',
      (loadedRobot: THREE.Object3D) => {
        if (modelRef.current) {
          robot.current = loadedRobot;
          modelRef.current.add(loadedRobot);
          
          // Set initial position and scale
          loadedRobot.position.set(0, 0, 0);
          loadedRobot.scale.set(1, 1, 1);
          
          // Log the available joints for debugging
          console.log('Available joints:', Object.keys((loadedRobot as any).joints));
        }
      },
      (progress: ProgressEvent) => {
        console.log('Loading progress:', progress);
      },
      (error: Error) => {
        console.error('Error loading URDF:', error);
      }
    );

    const updateModel = () => {
      if (!robot.current) return;
      
        // Get the current robot state
        const state = robotStateService.getState();
        
        // Update joint angles based on the state
        Object.entries(state.motors).forEach(([motorName, motorState]) => {
          // Grab angle in radians
          const angle = motorState.multiTurnRad_Mapped ?? 0;
          
          // Map motor names to joint names (J1 through J7)
          const jointName = `J${motorName}`;
          
          // Use the URDFRobot's setJointValue method
          const urdfRobot = robot.current as any;
          if (urdfRobot.setJointValue) {
            // Handle special cases for J6 and J7
            if (jointName === 'J6') {
              // J6 is pitch
              const success = urdfRobot.setJointValue(jointName, state.diff_pitch_rad ?? 0);
              if (!success) {
                console.warn(`Failed to set joint ${jointName} to pitch angle ${state.diff_pitch_rad}!`);
              }
            } else if (jointName === 'J7') {
              // J7 is roll
              const success = urdfRobot.setJointValue(jointName, state.diff_roll_rad ?? 0);
              if (!success) {
                console.warn(`Failed to set joint ${jointName} to roll angle ${state.diff_roll_rad}!`);
              }
            } else {
              // Handle all other joints normally
              const success = urdfRobot.setJointValue(jointName, angle);
              if (!success) {
                console.warn(`Failed to set joint ${jointName} to angle ${angle}!`);
              }
            }
          } else {
            console.warn('URDFRobot does not have setJointValue method');
          }
        });
    };

    robotStateService.on('update', updateModel);
    return () => {
      robotStateService.off('update', updateModel);
    };
  }, []);

  useFrame(() => {
    // Smooth animations or continuous updates can go here
  });

  return (
    <group ref={modelRef}>
      {/* The URDF loader will handle all the robot parts */}
    </group>
  );
};

export default RobotModel; 