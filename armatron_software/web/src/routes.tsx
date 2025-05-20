import { RouteObject } from 'react-router-dom';
import Dashboard from './pages/Dashboard';
import JointControl from './pages/JointControl';
// import MotionPlanning from './pages/MotionPlanning';
// import Debug from './pages/Debug';

export const routes: RouteObject[] = [
  {
    path: '/',
    element: <Dashboard />
  },
  {
    path: '/joint-control',
    element: <JointControl />
  }
  // {
  //   path: '/motion-planning',
  //   element: <MotionPlanning />
  // },
  // {
  //   path: '/debug',
  //   element: <Debug />
  // }
]; 