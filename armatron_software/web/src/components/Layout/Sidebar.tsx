import { List, ListItem, ListItemIcon, ListItemText, Divider } from '@mui/material';
import { useNavigate } from 'react-router-dom';
import DashboardIcon from '@mui/icons-material/Dashboard';
import SettingsIcon from '@mui/icons-material/Settings';
import TimelineIcon from '@mui/icons-material/Timeline';
import BugReportIcon from '@mui/icons-material/BugReport';

const menuItems = [
  { text: 'Dashboard', icon: <DashboardIcon />, path: '/' },
  { text: 'Joint Control', icon: <SettingsIcon />, path: '/joints' },
  { text: 'Motion Planning', icon: <TimelineIcon />, path: '/motion' },
  { text: 'Debug', icon: <BugReportIcon />, path: '/debug' },
];

export default function Sidebar() {
  const navigate = useNavigate();

  return (
    <List>
      {menuItems.map((item) => (
        <ListItem 
          button 
          key={item.text} 
          onClick={() => navigate(item.path)}
        >
          <ListItemIcon>{item.icon}</ListItemIcon>
          <ListItemText primary={item.text} />
        </ListItem>
      ))}
      <Divider />
    </List>
  );
} 