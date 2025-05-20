import { socketService } from './socket';

interface MotorCommand {
  cmd: string;
  motorID?: number;
  value?: number;
  angle?: number;
  maxSpeed?: number;
  angles?: number[];
  speeds?: number[];
  angKp?: number;
  angKi?: number;
  spdKp?: number;
  spdKi?: number;
  iqKp?: number;
  iqKi?: number;
  encoderValue?: number;
  accelVal?: number;
}

class MotorControlService {
  private static instance: MotorControlService;

  private constructor() {}

  public static getInstance(): MotorControlService {
    if (!MotorControlService.instance) {
      MotorControlService.instance = new MotorControlService();
    }
    return MotorControlService.instance;
  }

  // Individual joint commands
  public motorOn(motorID: number) {
    this.sendCommand({ cmd: 'motorOn', motorID });
  }

  public motorOff(motorID: number) {
    this.sendCommand({ cmd: 'motorOff', motorID });
  }

  public motorStop(motorID: number) {
    this.sendCommand({ cmd: 'motorStop', motorID });
  }

  public clearMotorError(motorID: number) {
    this.sendCommand({ cmd: 'clearMotorError', motorID });
  }

  public syncSingleAndMulti(motorID: number) {
    this.sendCommand({ cmd: 'syncSingleAndMulti', motorID });
  }

  public setTorque(motorID: number, value: number) {
    this.sendCommand({ cmd: 'setTorque', motorID, value });
  }

  public setPosition(motorID: number, angle: number, maxSpeed: number) {
    this.sendCommand({ cmd: 'setMultiAngleWithSpeed', motorID, angle, maxSpeed });
  }

  // Multi-joint commands
  public setMultiJointAngles(angles: number[], speeds: number[]) {
    this.sendCommand({ cmd: 'setMultiJointAngles', angles, speeds });
  }

  public emergencyStop() {
    for (let i = 1; i <= 7; i++) {
      this.motorStop(i);
    }
  }

  // Global control commands
  public writePID_RAM(motorID: number, angKp: number, angKi: number, spdKp: number, spdKi: number, iqKp: number, iqKi: number) {
    this.sendCommand({ cmd: 'writePID_RAM', motorID, angKp, angKi, spdKp, spdKi, iqKp, iqKi });
  }

  public readPID(motorID: number) {
    this.sendCommand({ cmd: 'readPID', motorID });
  }

  public writePID_ROM(motorID: number, angKp: number, angKi: number, spdKp: number, spdKi: number, iqKp: number, iqKi: number) {
    this.sendCommand({ cmd: 'writePID_ROM', motorID, angKp, angKi, spdKp, spdKi, iqKp, iqKi });
  }

  public readEncoder(motorID: number) {
    this.sendCommand({ cmd: 'readEncoder', motorID });
  }

  public writeEncoderZero(motorID: number, encoderValue: number) {
    this.sendCommand({ cmd: 'writeEncoderOffset', motorID, encoderValue });
  }

  public writeCurrentPosAsZero(motorID: number) {
    this.sendCommand({ cmd: 'writeCurrentPosAsZero', motorID });
  }

  public writeAcceleration(motorID: number, accelVal: number) {
    this.sendCommand({ cmd: 'writeAcceleration', motorID, accelVal });
  }

  public readAcceleration(motorID: number) {
    this.sendCommand({ cmd: 'readAcceleration', motorID });
  }

  private sendCommand(command: MotorCommand) {
    socketService.sendCommand(command);
  }
}

export const motorControlService = MotorControlService.getInstance(); 