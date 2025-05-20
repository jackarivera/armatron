import React from 'react';
import { AppBar, Toolbar, IconButton, Button, Menu, MenuItem, Paper, Divider } from '@mui/material';
import { Brightness4, Brightness7, PlayArrow, Stop, PrecisionManufacturing, PrecisionManufacturingOutlined, Settings } from '@mui/icons-material';
import { useTheme } from '../ThemeProvider';
import { Link } from 'react-router-dom';
import SettingsDialog from '../SettingsDialog';
import { socketService } from '../../services/socket';

const Layout: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const { mode, toggleTheme } = useTheme();
  const [anchorEl, setAnchorEl] = React.useState<null | HTMLElement>(null);
  const [settingsOpen, setSettingsOpen] = React.useState(false);

  const handleMenuClick = (event: React.MouseEvent<HTMLElement>) => {
    setAnchorEl(event.currentTarget);
  };

  const handleMenuClose = () => {
    setAnchorEl(null);
  };

  const handleSettingsClick = () => {
    setSettingsOpen(true);
  };

  const handleSettingsClose = () => {
    setSettingsOpen(false);
  };

  const handleSetModifier = (value: number) => {
    socketService.sendCommand({
      cmd: 'setMaxSpeedModifier',
      modifier: value
    });
    setSettingsOpen(false);
  };

  const handleESTOP = () => {
    socketService.sendCommand({
      cmd: 'setESTOP'
    });
  };

  const handleHoldPosition = () => {
    socketService.sendCommand({
      cmd: 'setHoldPosition'
    });
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', minHeight: '100vh' }}>
      <AppBar position="static" component={Paper} elevation={0}>
        <Toolbar>
          {/* Left side - Logo and Navigation */}
          <div style={{ display: 'flex', alignItems: 'center', flexGrow: 1 }}>
            <Button 
              component={Link} 
              to="/"
              startIcon={<PrecisionManufacturing />}
              sx={{ 
                color: 'inherit',
                textTransform: 'none',
                fontSize: '1.2rem',
                fontWeight: 'bold',
                mr: 2
              }}
            >
              Armatron
            </Button>
            
            <Button 
              color="inherit" 
              component={Link} 
              to="/"
              sx={{ mr: 2 }}
            >
              Dashboard
            </Button>
            <Button 
              color="inherit" 
              component={Link} 
              to="/joint-control"
              sx={{ mr: 2 }}
            >
              Joint Control
            </Button>
          </div>

          {/* Right side - Controls and Theme */}
          <div style={{ display: 'flex', alignItems: 'center' }}>
            <Button
              color="inherit"
              startIcon={<PrecisionManufacturingOutlined />}
              onClick={handleMenuClick}
              sx={{ mr: 1 }}
            >
              Positions
            </Button>
            <Menu
              anchorEl={anchorEl}
              open={Boolean(anchorEl)}
              onClose={handleMenuClose}
            >
              <MenuItem onClick={handleMenuClose}>Virtual Home</MenuItem>
              <MenuItem onClick={handleMenuClose}>Gravity Safe #1</MenuItem>
              <MenuItem onClick={handleMenuClose}>Example #3</MenuItem>
            </Menu>
            <Button
              color="inherit"
              startIcon={<PlayArrow />}
              onClick={handleHoldPosition}
              sx={{ mr: 1 }}
            >
              HOLD
            </Button>
            <Button
              color="error"
              startIcon={<Stop />}
              onClick={handleESTOP}
              sx={{ mr: 1 }}
            >
              ESTOP
            </Button>
            <Button
              color="inherit"
              startIcon={<Settings />}
              onClick={handleSettingsClick}
              sx={{ mr: 1 }}
            >
              Settings
            </Button>
            <Divider orientation="vertical" flexItem sx={{ mx: 1 }} />
            <IconButton onClick={toggleTheme} color="inherit">
              {mode === 'dark' ? <Brightness7 /> : <Brightness4 />}
            </IconButton>
          </div>
        </Toolbar>
      </AppBar>
      <main style={{ flexGrow: 1, padding: '24px' }}>
        {children}
      </main>
      <SettingsDialog 
        open={settingsOpen} 
        onClose={handleSettingsClose}
        onSetModifier={handleSetModifier}
      />
    </div>
  );
};

export default Layout;
