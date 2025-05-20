import React, { useEffect } from 'react';
import { BrowserRouter as Router, Routes, Route } from 'react-router-dom';
import { socketService } from './services/socket';
import JointControl from './pages/JointControl';
import Dashboard from './pages/Dashboard';
import Layout from './components/Layout';
import { ThemeProvider as CustomThemeProvider } from './components/ThemeProvider';

const AppContent: React.FC = () => {
  useEffect(() => {
    // Initialize socket connection when app starts
    socketService.connect();

    // Cleanup on unmount
    return () => {
      // Note: We don't disconnect here to maintain the connection across route changes
      // The socket service will handle reconnection if needed
    };
  }, []);

  return (
    <Layout>
      <Routes>
        <Route path="/" element={<Dashboard />} />
        <Route path="/joint-control" element={<JointControl />} />
      </Routes>
    </Layout>
  );
};

const App: React.FC = () => {
  return (
    <CustomThemeProvider>
      <Router>
        <AppContent />
      </Router>
    </CustomThemeProvider>
  );
};

export default App;
