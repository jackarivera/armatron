import { io, Socket } from 'socket.io-client';

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
  gains?: {
    angKp: number;
    angKi: number;
    spdKp: number;
    spdKi: number;
    iqKp: number;
    iqKi: number;
  };
}

interface MotorStates {
  motors: {
    [key: string]: MotorState;
  };
  diff_roll_rad?: number;
  diff_pitch_rad?: number;
  diff_roll_deg?: number;
  diff_pitch_deg?: number;
}

class SocketService {
  private static instance: SocketService;
  private socket: Socket | null = null;
  private motorStateCallbacks: ((data: MotorStates) => void)[] = [];
  private isConnecting: boolean = false;
  private reconnectAttempts: number = 0;
  private readonly MAX_RECONNECT_ATTEMPTS = 5;

  private constructor() {}

  public static getInstance(): SocketService {
    if (!SocketService.instance) {
      SocketService.instance = new SocketService();
    }
    return SocketService.instance;
  }

  public connect() {
    if (this.socket?.connected || this.isConnecting) {
      return;
    }

    this.isConnecting = true;
    const host = window.location.hostname;
    this.socket = io(`http://${host}:8888`, {
      reconnection: true,
      reconnectionAttempts: this.MAX_RECONNECT_ATTEMPTS,
      reconnectionDelay: 1000,
      reconnectionDelayMax: 5000,
      timeout: 20000
    });

    this.setupListeners();
  }

  private setupListeners() {
    if (!this.socket) return;

    this.socket.on('motorStates', (data: MotorStates) => {
      if (data && data.motors) {
        this.motorStateCallbacks.forEach(callback => callback(data));
      }
    });

    this.socket.on('connect', () => {
      console.log('Connected to server');
      this.isConnecting = false;
      this.reconnectAttempts = 0;
    });

    this.socket.on('disconnect', () => {
      console.log('Disconnected from server');
      this.isConnecting = false;
    });

    this.socket.on('connect_error', (error) => {
      console.error('Connection error:', error);
      this.isConnecting = false;
      this.reconnectAttempts++;
      if (this.reconnectAttempts >= this.MAX_RECONNECT_ATTEMPTS) {
        console.error('Max reconnection attempts reached');
        this.socket?.disconnect();
      }
    });
  }

  public sendCommand(command: any) {
    if (this.socket?.connected) {
      this.socket.emit('sendCommand', command);
    } else {
      console.warn('Cannot send command: Socket not connected');
    }
  }

  public onMotorStates(callback: (data: MotorStates) => void) {
    this.motorStateCallbacks.push(callback);
    return () => {
      this.motorStateCallbacks = this.motorStateCallbacks.filter(cb => cb !== callback);
    };
  }

  public on(event: string, callback: (...args: any[]) => void) {
    if (this.socket) {
      this.socket.on(event, callback);
    }
  }

  public off(event: string, callback?: (...args: any[]) => void) {
    this.socket?.off(event, callback);
  }

  public emit(event: string, ...args: any[]) {
    this.socket?.emit(event, ...args);
  }

  public isConnected(): boolean {
    return this.socket?.connected || false;
  }
}

export const socketService = SocketService.getInstance(); 