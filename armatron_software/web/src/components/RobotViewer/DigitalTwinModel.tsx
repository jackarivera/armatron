import React, { useEffect, useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import URDFLoader from 'urdf-loader';
import { robotStateService } from '../../services/robotState';
import * as THREE from 'three';

interface DigitalTwinModelProps {
  opacity?: number;
}

const DigitalTwinModel: React.FC<DigitalTwinModelProps> = ({ opacity = 0.5 }) => {
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
          
          // Apply transparency to all materials
          loadedRobot.traverse((child) => {
            if (child instanceof THREE.Mesh) {
              if (child.material instanceof THREE.Material) {
                child.material.transparent = true;
                child.material.opacity = opacity;
              }
            }
          });
          
          // Log the available joints for debugging
          console.log('Digital Twin - Available joints:', Object.keys((loadedRobot as any).joints));
        }
      },
      (progress: ProgressEvent) => {
        console.log('Digital Twin - Loading progress:', progress);
      },
      (error: Error) => {
        console.error('Digital Twin - Error loading URDF:', error);
      }
    );

    const updateModel = () => {
      if (!robot.current) return;
      
      // Get the current robot state
      const state = robotStateService.getState();
      
      // Only update if twin is active and exists
      if (!state.twin?.active) return;
      
      // Update joint angles based on the twin state
      state.twin.joint_angles_deg.forEach((angle, index) => {
        // Map motor names to joint names (J1 through J7)
        const jointName = `J${index + 1}`;
        
        // Use the URDFRobot's setJointValue method
        const urdfRobot = robot.current as any;
        if (urdfRobot.setJointValue) {
          // Handle special cases for J6 and J7
          if (jointName === 'J6') {
            
            // J6 is pitch - use the second to last element of the array
            const success = urdfRobot.setJointValue(jointName, state.twin?.joint_angles_deg[5] ?? 0);
            if (!success) {
              console.warn(`Digital Twin - Failed to set joint ${jointName} to pitch angle ${state.twin?.joint_angles_deg[5] ?? 0}!`);
            }
          } else if (jointName === 'J7') {
            // J7 is roll - use the last element of the array
            const success = urdfRobot.setJointValue(jointName, state.twin?.joint_angles_deg[6] ?? 0);
            if (!success) {
              console.warn(`Digital Twin - Failed to set joint ${jointName} to roll angle ${state.twin?.joint_angles_deg[6] ?? 0}!`);
            }
          } else if (index < 5) {
            // Handle all other joints normally (only first 5 joints)
            const success = urdfRobot.setJointValue(jointName, angle * (Math.PI / 180)); // Convert degrees to radians
            if (!success) {
              console.warn(`Digital Twin - Failed to set joint ${jointName} to angle ${angle}!`);
            }
          }
        } else {
          console.warn('Digital Twin - URDFRobot does not have setJointValue method');
        }
      });
    };

    robotStateService.on('update', updateModel);
    return () => {
      robotStateService.off('update', updateModel);
    };
  }, [opacity]);

  useFrame(() => {
    // Smooth animations or continuous updates can go here
  });

  return (
    <group ref={modelRef}>
      {/* The URDF loader will handle all the robot parts */}
    </group>
  );
};

export default DigitalTwinModel; 