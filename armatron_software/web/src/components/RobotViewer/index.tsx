import React, { Suspense } from 'react';
import { Canvas } from '@react-three/fiber';
import { OrbitControls, Grid } from '@react-three/drei';
import { Paper } from '@mui/material';
import { useTheme } from '@mui/material/styles';
import * as THREE from 'three';
import RobotModel from './RobotModel';
import DigitalTwinModel from './DigitalTwinModel';

interface RobotViewerProps {
  style?: React.CSSProperties;
}

const RobotViewer: React.FC<RobotViewerProps> = ({ style }) => {
  const theme = useTheme();

  return (
    <Paper 
      elevation={3} 
      sx={{ 
        width: '100%', 
        height: '100%', 
        minHeight: '400px',
        backgroundColor: theme.palette.background.paper,
        ...style 
      }}
    >
      <Canvas
        orthographic
        camera={{
          position: [5, 0, 5],
          zoom: 400,
          near: 0.1,
          far: 1000,
          up: [0, 0, 1]
        }}
        style={{ width: '100%', height: '100%' }}
        gl={{ antialias: true }}
      >
        <color attach="background" args={['#263238']} />
        <Suspense fallback={null}>
          <ambientLight intensity={0.3} />
          <directionalLight
            position={[5, 30, 5]}
            intensity={1}
            castShadow
            shadow-mapSize={[1024, 1024]}
          />
          <pointLight 
            position={[1, 0, 2.2]} 
            intensity={0.8} 
            distance={100}
            decay={2}
          />
          <OrbitControls 
            makeDefault
            enableDamping
            dampingFactor={0.05}
            rotateSpeed={0.5}
            target={[0, 0, 0]}
            minPolarAngle={0}
            maxPolarAngle={Math.PI / 2}
            up={[0, 0, 1]}
          />
          <RobotModel />
          <DigitalTwinModel opacity={0.5} />
          <Grid
            args={[0.6096, 0.9144]}
            position={[0.2032, 0, 0]}
            rotation={[Math.PI / 2, 0, 0]}
            cellSize={0.0254} // 1 inch
            cellThickness={0.5}
            cellColor="#6f6f6f"
            sectionSize={0.1524} // 6 inches
            sectionThickness={1}
            sectionColor="#9d4b4b"
            fadeDistance={30}
            fadeStrength={1}
            followCamera={false}
            infiniteGrid={false}
          />
          <primitive object={new THREE.AxesHelper(0.33)} />
        </Suspense>
      </Canvas>
    </Paper>
  );
};

export default RobotViewer; 