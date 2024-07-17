//
// Created by ntoskrnl on 2024-07-02.
//

#include <esp_check.h>
#include <hal/spi_types.h>
#include <driver/spi_master.h>
#include <string.h>
#include <driver/gpio.h>
#include "driver/spi_common.h"
#include "display.h"
#include "sdkconfig.h"
#include "cascadia_12x24/font.h"

typedef enum {
	ST_DAT = 1,
	ST_CMD = 0,
} st_dc_t;

spi_device_handle_t display;
DRAM_ATTR disp_color_t framebuffer[CONFIG_DISP_SIZE_Y][CONFIG_DISP_SIZE_X];

uint8_t fg_r = 255, fg_g = 255, fg_b = 255;
uint8_t bg_r = 0, bg_g = 0, bg_b = 0;


void st7789_write(st_dc_t dat_cmd, const uint8_t* data, size_t len) {
	static spi_transaction_t txn = {};
	gpio_set_level(CONFIG_DISP_PIN_DC, dat_cmd);
	txn.length = len*8;
	txn.tx_buffer = data;
	ESP_ERROR_CHECK(spi_device_transmit(display, &txn));
}

void st7789_write_cmd(uint8_t cmd) {
	st7789_write(ST_CMD, &cmd, 1);
}

void st7789_write_byte(uint8_t val) {
	st7789_write(ST_DAT, &val, 1);
}

void nt_disp_fill(uint8_t r, uint8_t g, uint8_t b) {
	memset(&framebuffer, COLOR(r, g, b), sizeof(framebuffer));
}

void nt_disp_redraw() {
	// Redraw from framebuffer
	st7789_write_cmd(0x2a);
	st7789_write(ST_DAT, (uint8_t[]){0, 0}, 2);
	st7789_write_cmd(0x2b);
	st7789_write(ST_DAT, (uint8_t[]){0, 0}, 2);
	st7789_write_cmd(0x2c);

	uint8_t* fb_pointer = (uint8_t*)framebuffer;

	spi_device_acquire_bus(display, portMAX_DELAY);
	uint32_t start = esp_cpu_get_cycle_count();

	if (sizeof(framebuffer) > 32768) {  // If the framebuffer is too large to fit in one DMA transaction, split it up
		int32_t remaining = sizeof(framebuffer);
		do {
			st7789_write(ST_DAT, fb_pointer, (remaining > 32768 ? 32768 : remaining));
			remaining -= 32768;
			fb_pointer += 32768;
		} while (remaining > 0);
	} else {
		st7789_write(ST_DAT, (uint8_t *) framebuffer, sizeof(framebuffer));
	}

	uint32_t end = esp_cpu_get_cycle_count();
	spi_device_release_bus(display);

	printf("redraw: took %lu clock cycles to write framebuffer\n", end-start);
}

void nt_disp_start() {
	st7789_write_cmd(0x01);  // reset
	vTaskDelay(100/portTICK_PERIOD_MS);

	st7789_write_cmd(0x11);
	vTaskDelay(250/portTICK_PERIOD_MS);

	st7789_write_cmd(0x3a);
	st7789_write_byte(0x55);

	st7789_write_cmd(0x36);	//Memory Data Access Control
	st7789_write_byte(0b01000001);

	st7789_write_cmd(0x2A);	// screen width
	st7789_write_byte(0x00);	// from 0x0000...
	st7789_write_byte(0x00);
	st7789_write_byte(0x00);	// .. to 0x00F0 (240)
	st7789_write_byte(0xF0);

	st7789_write_cmd(0x2B);	// screen height
	st7789_write_byte(0x00);	// from 0x0000...
	st7789_write_byte(0x00);
	st7789_write_byte(0x01);	// .. to 0x0140 (320)
	st7789_write_byte(0x40);

	st7789_write_cmd(0x21);	// Invert display (so it's actually *normal*)
	vTaskDelay(10/portTICK_PERIOD_MS);

	st7789_write_cmd(0x13);	// Use full display with
	vTaskDelay(10/portTICK_PERIOD_MS);

	st7789_write_cmd(0x29);	//Display ON
	vTaskDelay(255/portTICK_PERIOD_MS);

}

esp_err_t nt_disp_init() {
	gpio_reset_pin(CONFIG_DISP_PIN_RST);
	gpio_set_direction(CONFIG_DISP_PIN_RST, GPIO_MODE_OUTPUT);
	gpio_set_level(CONFIG_DISP_PIN_RST, 1);

	gpio_reset_pin(CONFIG_DISP_PIN_DC);
	gpio_set_direction(CONFIG_DISP_PIN_DC, GPIO_MODE_OUTPUT);
	gpio_set_level(CONFIG_DISP_PIN_DC, 0);

	gpio_set_level(CONFIG_DISP_PIN_RST, 0);  // start reset. We'll get back to this later

#if CONFIG_DISP_PIN_BKLT != -1
	gpio_reset_pin(CONFIG_DISP_PIN_BKLT);
	gpio_set_direction(CONFIG_DISP_PIN_BKLT, GPIO_MODE_OUTPUT);
	gpio_set_level(CONFIG_DISP_PIN_BKLT, 0);
#endif

	spi_bus_config_t bus = {
			.mosi_io_num = CONFIG_DISP_PIN_MOSI,
			.miso_io_num = CONFIG_DISP_PIN_MISO,
			.sclk_io_num = CONFIG_DISP_PIN_SCK,
			.quadwp_io_num = -1,
			.quadhd_io_num = -1,
			.max_transfer_sz = 32768,
			.flags = 0
	};

	ESP_RETURN_ON_ERROR(
			spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO),
			"display", "spi bus failed init"
	);

	spi_device_interface_config_t cfg = {
			.clock_speed_hz = CONFIG_DISP_SPI_FREQ*1000,
			.queue_size = 8,
			.mode = 3,  // mode 0 DOES NOT work
			.flags = 0,
			.spics_io_num = CONFIG_DISP_PIN_CS,
	};
	ESP_RETURN_ON_ERROR(
			spi_bus_add_device(SPI2_HOST, &cfg, &display),
			"display", "spi device failed init"
	);

	memset(&framebuffer, COLOR(bg_r, bg_g, bg_b), sizeof(framebuffer));

	gpio_set_level(CONFIG_DISP_PIN_RST, 1);

	return ESP_OK;
}

void fb_render_char(uint8_t chr, int loc_x, int loc_y) {
	// This function simply places the character into the framebuffer at the specified location
	// Location should be specified in character-grid coordinates, not pixel coords
	// Also need to scale linearly from fg color to bg color: 255=>fg, 0=>bg
	int corner_x = loc_x * font_size_x;
	int corner_y = loc_y * font_size_y;

	for (int x = 0; x < font_size_x; x++) {
		for (int y = 0; y < font_size_y; y++) {
			uint8_t pxl = font_sheet[chr][(y * font_size_x) + x];
			framebuffer[corner_x + x][corner_y + y] = COLOR(pxl,pxl,pxl);
		}
	}
}

void nt_disp_write_text(const char* str, size_t len, int loc_x, int loc_y) {
	for (int i = 0; i < len; ++i) {
		loc_x = loc_x % (CONFIG_DISP_SIZE_Y / font_size_x);
		loc_y = loc_y % (CONFIG_DISP_SIZE_X / font_size_y);
		if (str[i] == '\r') {
			loc_x = 0;
		} else if (str[i] == '\n') {
			loc_x = 0;
			loc_y += 1;
		} else {
			fb_render_char(str[i], loc_x, loc_y);
			loc_x += 1;
		}
		if (loc_x >= (CONFIG_DISP_SIZE_Y / font_size_x)) {
			loc_x = 0;
			loc_y += 1;
		}
	}
}