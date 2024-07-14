//
// Created by ntoskrnl on 2024-07-12.
//

#include <esp_err.h>
#include <driver/spi_master.h>
#include <esp_log.h>
#include <esp_check.h>
#include "imu.h"

spi_device_handle_t imu;
int accel_scale = 8;
int gyro_scale = 2000;
float imu_temp = 0.0f;
float imu_accel[3] = {0.0f, 0.0f, 0.0f};
float imu_gyro[3] = {0.0f, 0.0f, 0.0f};  // we start off in outer space


#if CONFIG_IMU_ENABLED

void imu_write_reg(uint8_t reg, uint8_t val) {
	assert(reg <= 0x7f);
	spi_transaction_t txn = {
			.flags = SPI_TRANS_USE_TXDATA,
			.cmd = 0,
			.addr = reg,
			.length = 8,
			.tx_data[0] = val,
			.rx_buffer = NULL,
	};
	ESP_ERROR_CHECK(spi_device_transmit(imu, &txn));
}

uint8_t imu_read_reg(uint8_t reg) {
	assert(reg <= 0x7f);
	spi_transaction_t txn = {
			.flags = SPI_TRANS_USE_RXDATA,
			.cmd = 1,
			.addr = reg,
			.rxlength = 8,
			.tx_buffer = NULL,
	};
	ESP_ERROR_CHECK(spi_device_transmit(imu, &txn));
	return txn.rx_data[0];
}

int16_t imu_read_reg2(uint8_t reg) {
	assert(reg <= 0x7f);
	spi_transaction_t txn = {
			.flags = SPI_TRANS_USE_RXDATA,
			.cmd = 1,
			.addr = reg,
			.rxlength = 16,
			.tx_buffer = NULL,
	};
	ESP_ERROR_CHECK(spi_device_transmit(imu, &txn));
	return ((int16_t*)(txn.rx_data))[0];
}

void imu_reconfigure(imu_accel_range_t accel_range, imu_odr_t accel_odr,
					 imu_gyro_range_t gyro_range, imu_odr_t gyro_odr) {

	imu_write_reg(0x15, imu_read_reg(0x15) & 0b11101111);	// Clear bit 4 (Enable Accel high-perf mode)
	imu_write_reg(0x16, imu_read_reg(0x16) & 0b01111111);	// Clear bit 7 (Enable Gyro high-perf mode)

	uint8_t CTRL1_XL = (accel_odr << 4) | (accel_range << 2);
	imu_write_reg(0x10, CTRL1_XL);

	uint8_t CTRL2_G = (gyro_odr << 4) | (gyro_range << 1);
	imu_write_reg(0x11, CTRL2_G);

	// set the global scale values
	switch (accel_range) {
		case IMU_ACCEL_RANGE_2G:
			accel_scale = 2;
			break;
		case IMU_ACCEL_RANGE_4G:
			accel_scale = 4;
			break;
		case IMU_ACCEL_RANGE_8G:
			accel_scale = 8;
			break;
		case IMU_ACCEL_RANGE_16G:
			accel_scale = 16;
			break;
	}
	switch (gyro_range) {
		case IMU_GYRO_RANGE_125DPS:
			gyro_scale = 125;
			break;
		case IMU_GYRO_RANGE_250DPS:
			gyro_scale = 250;
			break;
		case IMU_GYRO_RANGE_500DPS:
			gyro_scale = 500;
			break;
		case IMU_GYRO_RANGE_1000DPS:
			gyro_scale = 1000;
			break;
		case IMU_GYRO_RANGE_2000DPS:
			gyro_scale = 2000;
			break;
	}
}

void imu_update() {
	int16_t raw;  // we're gonna put it in raw
	float gyro_scale_factor = (float)gyro_scale*0.000035f;  // 2000dps = 70mdps/LSB, same scale w/ others
	float accel_scale_factor = (float)accel_scale/32768.0f;  // 16g = 0.488mg/LSB, aka div by value's width
	// (these values are fine as float because they're binary-round-ish, eg. 16/32768)

	raw = imu_read_reg2(0x20);
	imu_temp = ((float)raw/256.0f) + 25.0f;  // Lower byte is fractions of a degree, upper register centered on +25'C

	raw = imu_read_reg2(0x22);
	imu_gyro[0] = (float)raw*gyro_scale_factor;
	raw = imu_read_reg2(0x24);
	imu_gyro[1] = (float)raw*gyro_scale_factor;
	raw = imu_read_reg2(0x26);
	imu_gyro[2] = (float)raw*gyro_scale_factor;

	raw = imu_read_reg2(0x28);
	imu_accel[0] = (float)raw*accel_scale_factor;
	raw = imu_read_reg2(0x2a);
	imu_accel[1] = (float)raw*accel_scale_factor;
	raw = imu_read_reg2(0x2c);
	imu_accel[2] = (float)raw*accel_scale_factor;
}

void nt_imu_update() { imu_update(); }

esp_err_t nt_imu_init() {
	spi_bus_config_t bus = {
			.mosi_io_num = CONFIG_DISP_PIN_MOSI,
			.miso_io_num = CONFIG_DISP_PIN_MISO,
			.sclk_io_num = CONFIG_DISP_PIN_SCK,
			.quadwp_io_num = -1,
			.quadhd_io_num = -1,
			.max_transfer_sz = 32768,
			.flags = 0
	};
	esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
	if (ret == ESP_ERR_INVALID_STATE) {
		ESP_LOGI("imu", "spi bus already in use, adding device instead");
	} else
		ESP_RETURN_ON_ERROR(ret, "imu", "spi bus failed init");

	spi_device_interface_config_t cfg = {
			.clock_speed_hz = (int)10e6,
			.queue_size = 8,
			.mode = 0,
			.flags = SPI_DEVICE_HALFDUPLEX,
			.command_bits = 1,
			.address_bits = 7,
			.spics_io_num = 13,  // todo: config option. (on protopaw 0.1, this is GPIO13)
	};

	ESP_RETURN_ON_ERROR(
			spi_bus_add_device(SPI2_HOST, &cfg, &imu),
			"imu", "spi device failed init"
	);

	if (imu_read_reg(0x0f) != 0x6c)  // LSM6DSO: DS page 48: register 0x0f always contains 0x6c
		return ESP_ERR_NOT_SUPPORTED;

	// Datasheet: https://www.st.com/resource/en/datasheet/lsm6dso.pdf
	imu_reconfigure(
			IMU_ACCEL_RANGE_8G, IMU_ODR_6660HZ,  // we lose nothing (except like, 50µA) by measuring at max speed
			IMU_GYRO_RANGE_2000DPS, IMU_ODR_6660HZ
	);
	imu_write_reg(0x16, 0b01000000);  // [7] Gyro low-power mode, [6] Gyro HPF en, [5:4] Gyro HPF freq (65mHz)
	imu_write_reg(0x5e, 0b00000100);  // enable orientation detection

	return ESP_OK;
}
