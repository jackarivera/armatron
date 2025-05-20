// // // // // // // // // // // // // // // 
//                                        //
//       Motor Parameter Definitions      //
//                                        //
// // // // // // // // // // // // // // //

// MG8015
#define MG8015_MAX_TORQUE_NM 28
#define MG8015_RATED_TORQUE_NM 14
#define MG8015_REDUCTION_RATIO 9
#define MG8015_SINGLE_TURN_DEG_SCALE_MAX 324

// MG8008
#define MG8008_MAX_TORQUE_NM 20
#define MG8008_RATED_TORQUE_NM 10
#define MG8008_REDUCTION_RATIO 9
#define MG8008_SINGLE_TURN_DEG_SCALE_MAX 324

// MG4010
#define MG4010_MAX_TORQUE_NM 4.5
#define MG4010_RATED_TORQUE_NM 2.5
#define MG4010_REDUCTION_RATIO 10
#define MG4010_SINGLE_TURN_DEG_SCALE_MAX 360

// MG4005
#define MG4005_MAX_TORQUE_NM 2.5
#define MG4005_RATED_TORQUE_NM 1
#define MG4005_REDUCTION_RATIO 1
#define MG4005_SINGLE_TURN_DEG_SCALE_MAX 360

// Per Joint Parameters
#define JOINT_1_ANGLE_LIMIT_LOW 0 // Deg Units
#define JOINT_1_ANGLE_LIMIT_HIGH 210 // Deg Units
#define JOINT_1_NM_TO_IQ_M 0 // [iq/Nm]
#define JOINT_1_NM_TO_IQ_B 0 // [iq]
#define JOINT_1_MAX_SPEED 180 // [deg/s]
#define JOINT_1_MAX_ACCEL 360 // [deg/s^2]
#define JOINT_1_MAX_JERK 1800 // [deg/s^3]

#define JOINT_2_ANGLE_LIMIT_LOW 5
#define JOINT_2_ANGLE_LIMIT_HIGH 215
#define JOINT_2_NM_TO_IQ_M 0 // [iq/Nm]
#define JOINT_2_NM_TO_IQ_B 0 // [iq]
#define JOINT_2_MAX_SPEED 180 // [deg/s]
#define JOINT_2_MAX_ACCEL 360 // [deg/s^2]
#define JOINT_2_MAX_JERK 1800 // [deg/s^3]

#define JOINT_3_ANGLE_LIMIT_LOW -30
#define JOINT_3_ANGLE_LIMIT_HIGH 225
#define JOINT_3_NM_TO_IQ_M 0 // [iq/Nm]
#define JOINT_3_NM_TO_IQ_B 0 // [iq]
#define JOINT_3_MAX_SPEED 180 // [deg/s]
#define JOINT_3_MAX_ACCEL 360 // [deg/s^2]
#define JOINT_3_MAX_JERK 1800 // [deg/s^3]

#define JOINT_4_ANGLE_LIMIT_LOW 2.5
#define JOINT_4_ANGLE_LIMIT_HIGH 245
#define JOINT_4_NM_TO_IQ_M 0 // [iq/Nm]
#define JOINT_4_NM_TO_IQ_B 0 // [iq]
#define JOINT_4_MAX_SPEED 180 // [deg/s]
#define JOINT_4_MAX_ACCEL 360 // [deg/s^2]
#define JOINT_4_MAX_JERK 1800 // [deg/s^3]

#define JOINT_5_ANGLE_LIMIT_LOW -30
#define JOINT_5_ANGLE_LIMIT_HIGH 210
#define JOINT_5_NM_TO_IQ_M 0 // [iq/Nm]
#define JOINT_5_NM_TO_IQ_B 0 // [iq]
#define JOINT_5_MAX_SPEED 180 // [deg/s]
#define JOINT_5_MAX_ACCEL 360 // [deg/s^2]  
#define JOINT_5_MAX_JERK 1800 // [deg/s^3]

#define JOINT_6_ANGLE_LIMIT_LOW -36000
#define JOINT_6_ANGLE_LIMIT_HIGH 36000
#define JOINT_6_NM_TO_IQ_M 0 // [iq/Nm]
#define JOINT_6_NM_TO_IQ_B 0 // [iq]

#define JOINT_7_ANGLE_LIMIT_LOW -36000
#define JOINT_7_ANGLE_LIMIT_HIGH 36000
#define JOINT_7_NM_TO_IQ_M 0 // [iq/Nm]
#define JOINT_7_NM_TO_IQ_B 0 // [iq]    

#define DIFF_MAX_SPEED 360 // [deg/s]
#define DIFF_MAX_ACCEL 720  // [deg/s^2]
#define DIFF_MAX_JERK 3600 // [deg/s^3]

#define DIFF_PITCH_ANGLE_LIMIT_LOW -100
#define DIFF_PITCH_ANGLE_LIMIT_HIGH 90
