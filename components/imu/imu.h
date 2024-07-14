//
// Created by ntoskrnl on 2024-07-12.
//

#pragma once

#include <esp_err.h>


typedef struct {
	union {  // alias these names
		float azimuth;
		float az;
	};
	union {
		float elevation;
		float el;
	};
} gravity_vec_t;


typedef enum {
	IMU_OFF = 0,
	IMU_SENSOR_OFF = 0,
	IMU_ODR_OFF = 0,
	IMU_ODR_0HZ = 0,
	IMU_ODR_ZERO = 0,

	IMU_ODR_12HZ = 0b0001,
	IMU_ODR_26HZ = 0b0010,
	IMU_ODR_52HZ = 0b0011,
	IMU_ODR_104HZ = 0b0100,
	IMU_ODR_208HZ = 0b0101,
	IMU_ODR_416HZ = 0b0110,
	IMU_ODR_833HZ = 0b0111,
	IMU_ODR_1660HZ = 0b1000,
	IMU_ODR_3330HZ = 0b1001,
	IMU_ODR_6660HZ = 0b1010,
} imu_odr_t;


typedef enum {
	IMU_ACCEL_RANGE_2G = 0b00,
	IMU_ACCEL_RANGE_4G = 0b10,
	IMU_ACCEL_RANGE_8G = 0b11,
	IMU_ACCEL_RANGE_16G = 0b01,
} imu_accel_range_t;

typedef enum {
	IMU_GYRO_RANGE_125DPS = 0b001,
	IMU_GYRO_RANGE_250DPS = 0b000,
	IMU_GYRO_RANGE_500DPS = 0b010,
	IMU_GYRO_RANGE_1000DPS = 0b100,
	IMU_GYRO_RANGE_2000DPS = 0b110,
} imu_gyro_range_t;


extern float imu_temp;  // [degC]
extern float imu_accel[3];  // [g] x, y, z
extern float imu_gyro[3];  // [deg/s] x, y, z

/* We're going to reference the device's orientation based on the gravity
 * vector's location relative to a static device (basis x/y/z vectors).
 * The azimuth will be defined with standard 3d-coordinate system rules:
 * Az= 0: +X <+1.0, 0.0, 0.0>
 * Az=90: +Y <0.0, +1.0, 0.0>
 * At extreme values close to the Z axis (eg. normally upright or upside-down),
 * the azimuth is not well defined and should not be relied upon (your code
 * should check the elevation first, and handle logic based on that).*/
extern gravity_vec_t orientation;


void nt_imu_update();
esp_err_t nt_imu_init();
esp_err_t nt_imu_start_task();
