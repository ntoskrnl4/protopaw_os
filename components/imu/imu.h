//
// Created by ntoskrnl on 2024-07-12.
//

#pragma once

#include <esp_err.h>

extern float imu_temp;  // [degC]
extern float imu_accel[3];  // [g] x, y, z
extern float imu_gyro[3];  // [deg/s] x, y, z

void nt_imu_update();
esp_err_t nt_imu_init();
