import { socketService } from './socket';
import { EventEmitter } from 'events';

interface TwinState {
  active: boolean;
  joint_angles_deg: number[]; // First 5 elements are regular joints, last 2 are diff_pitch_rad and diff_roll_rad
  diff_roll_rad?: number;
  diff_pitch_rad?: number;
}

interface MotorGains {
  angKp: number;
  angKi: number;
  spdKp: number;
  spdKi: number;
  iqKp: number;
  iqKi: number;
}

interface MotorState {
  temp: number;
  torqueA: number;
  speedDeg_s: number;
  posDeg: number;
  encoder_val?: number;
  error?: number;
  positionRad_Mapped?: number;
  positionDeg_Mapped?: number;
  multiTurnRaw: number;
  multiTurnRad_Mapped?: number;
  multiTurnDeg_Mapped?: number;
  gains?: MotorGains;
}

interface RobotState {
  motors: {
    [key: string]: MotorState;
  };
  diff_roll_rad?: number;
  diff_pitch_rad?: number;
  diff_roll_deg?: number;
  diff_pitch_deg?: number;
  twin?: TwinState;
  connected: boolean;
  lastUpdate: number;
}

class RobotStateService extends EventEmitter {
  private static instance: RobotStateService;
  private state: RobotState = {
    motors: {},
    diff_roll_rad: 0,
    diff_pitch_rad: 0,
    diff_roll_deg: 0,
    diff_pitch_deg: 0,
    connected: false,
    lastUpdate: 0
  };
  private connectionCheckInterval: NodeJS.Timeout | null = null;
  private readonly CONNECTION_TIMEOUT_MS = 1000;

  private constructor() {
    super();
    this.setupSocketListeners();
    this.startConnectionCheck();
    
    // Also register with the specific motor states callback system
    socketService.onMotorStates((data) => {
      console.log('[RobotStateService] Received data via onMotorStates callback', data);
      this.handleMotorStates(data);
    });
  }

  public static getInstance(): RobotStateService {
    if (!RobotStateService.instance) {
      RobotStateService.instance = new RobotStateService();
    }
    return RobotStateService.instance;
  }

  private startConnectionCheck() {
    this.connectionCheckInterval = setInterval(() => {
      const timeSinceLastUpdate = Date.now() - this.state.lastUpdate;
      const shouldBeConnected = timeSinceLastUpdate < this.CONNECTION_TIMEOUT_MS;
      
      if (this.state.connected !== shouldBeConnected) {
        this.state.connected = shouldBeConnected;
        this.emit('update', this.state);
      }
    }, 100); // Check every 100ms
  }

  private setupSocketListeners() {
    socketService.on('connect', () => {
      this.state.lastUpdate = Date.now();
      this.emit('update', this.state);
    });

    socketService.on('disconnect', () => {
      this.state.connected = false;
      this.state.lastUpdate = Date.now();
      this.emit('update', this.state);
    });

    socketService.on('connect_error', () => {
      this.state.connected = false;
      this.state.lastUpdate = Date.now();
      this.emit('update', this.state);
    });
  }

  private handleMotorStates(data: any) {
    try {
      // Create a new state object that preserves existing state
      const newState: RobotState = {
        connected: true,  // Set connected to true when we receive data
        lastUpdate: Date.now(),
        motors: {},  // Initialize motors object
        diff_roll_rad: 0,
        diff_pitch_rad: 0,
        diff_roll_deg: 0,
        diff_pitch_deg: 0,
        twin: {
          active: false,
          joint_angles_deg: [0, 0, 0, 0, 0, 0, 0],
          diff_roll_rad: 0,
          diff_pitch_rad: 0,
        },
      };

      // Process motor states
      if (data.motors) {
        Object.entries(data.motors).forEach(([key, value]: [string, any]) => {
          if (!value) {
            console.warn(`[RobotStateService] Missing data for motor ${key}`);
            return;
          }
          
          try {
            //console.log(`[RobotStateService] Processing motor ${key} data:`, value);
            newState.motors[key] = {
              temp: value.temp || 0,
              torqueA: value.torqueA || 0,
              speedDeg_s: value.speedDeg_s || 0,
              posDeg: value.posDeg || 0,
              multiTurnRaw: value.multiTurnRaw || 0,
              multiTurnRad_Mapped: value.multiTurnRad_Mapped,
              multiTurnDeg_Mapped: value.multiTurnDeg_Mapped,
              error: value.error || 0,
              encoder_val: value.encoder_val,
              positionRad_Mapped: value.positionRad_Mapped,
              positionDeg_Mapped: value.positionDeg_Mapped,
              gains: value.gains ? {
                angKp: value.gains.angKp || 0,
                angKi: value.gains.angKi || 0,
                spdKp: value.gains.spdKp || 0,
                spdKi: value.gains.spdKi || 0,
                iqKp: value.gains.iqKp || 0,
                iqKi: value.gains.iqKi || 0
              } : undefined
            };
            //console.log(`[RobotStateService] Processed motor ${key} gains:`, newState.motors[key].gains);
          } catch (err) {
            console.error(`[RobotStateService] Error processing motor ${key} data:`, err);
          }
        });
      } else {
        console.warn('[RobotStateService] Received data without motors property');
      }

      // Add differential data
      if (data.diff_roll_rad !== undefined) newState.diff_roll_rad = data.diff_roll_rad;
      if (data.diff_pitch_rad !== undefined) newState.diff_pitch_rad = data.diff_pitch_rad;
      if (data.diff_roll_deg !== undefined) newState.diff_roll_deg = data.diff_roll_deg;
      if (data.diff_pitch_deg !== undefined) newState.diff_pitch_deg = data.diff_pitch_deg;

      // Process twin state if present
      if (data.twin) {
        try {
          newState.twin = {
            active: !!data.twin.active,
            joint_angles_deg: Array.isArray(data.twin.joint_angles_deg) ? 
              data.twin.joint_angles_deg : [0, 0, 0, 0, 0, 0, 0],
            diff_roll_rad: Array.isArray(data.twin.joint_angles_deg) && data.twin.joint_angles_deg.length > 6 ?
              data.twin.joint_angles_deg[6] : 0,
            diff_pitch_rad: Array.isArray(data.twin.joint_angles_deg) && data.twin.joint_angles_deg.length > 5 ?
              data.twin.joint_angles_deg[5] : 0,
          };
        } catch (err) {
          console.error('[RobotStateService] Error processing twin data:', err);
        }
      }

      console.log('[RobotStateService] Updating state with new data');
      this.updateState(newState);
    } catch (err) {
      console.error('[RobotStateService] Error handling motor states:', err);
    }
  }

  public getState(): RobotState {
    return { ...this.state }; // Return a copy to prevent direct state mutation
  }

  public getMotorState(motorId: string): MotorState | undefined {
    return this.state.motors[motorId];
  }

  public isConnected(): boolean {
    return this.state.connected;
  }

  public getLastUpdate(): number {
    return this.state.lastUpdate;
  }

  public cleanup() {
    if (this.connectionCheckInterval) {
      clearInterval(this.connectionCheckInterval);
    }
  }

  public updateState(newState: Partial<RobotState>): void {
    this.state = {
      ...this.state,
      ...newState,
      motors: {
        ...this.state.motors,
        ...newState.motors
      },
      twin: {
        ...(this.state.twin || { active: false, joint_angles_deg: [] }),
        ...(newState.twin || {})
      },
      connected: this.state.connected,
      lastUpdate: Date.now()
    };
    this.emit('update', this.state);
  }

  public updateTwinState(twinState: Partial<TwinState>): void {
    this.state = {
      ...this.state,
      twin: {
        ...(this.state.twin || { active: false, joint_angles_deg: [] }),
        ...twinState,
      }
    };
    this.emit('update', this.state);
  }
}

export const robotStateService = RobotStateService.getInstance(); 