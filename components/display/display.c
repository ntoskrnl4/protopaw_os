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

color_rgb_t fg = {0xff, 0xff, 0xff};
color_rgb_t bg = {0};
bool display_transpose_axes;


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


void st7789_set_window(int x1, int y1, int x2, int y2) {
	x2 -= 1;
	y2 -= 1;

	st7789_write_cmd(0x2A);	// set window width
	st7789_write_byte(x1 >> 8);	// from start (upper, lower)
	st7789_write_byte(x1 & 0xFF);
	st7789_write_byte(x2 >> 8);	// to end (upper, lower)
	st7789_write_byte(x2 & 0xFF);

	st7789_write_cmd(0x2B);	// set window height
	st7789_write_byte(y1 >> 8);	// from start
	st7789_write_byte(y1 & 0xFF);
	st7789_write_byte(y2 >> 8);	// to end
	st7789_write_byte(y2 & 0xFF);
}


void st7789_set_transforms(bool flip_x, bool flip_y, bool transpose) {
/* A Concrete Example, using Adafruit 240x320 display module
 * 		/--- 240x320 2.0" TFT ST7789 ---\
 * 		|								|
 * 		|	O --> x dim					|
 * 		|	|							|
 * 		|	|							|
 * 		|	y							|
 * 		|	dim							|
 * 		|								|
 * 		|								|
 * 		|								|
 * 		|								|
 * 		|								|
 * 		|								|
 * 		|								|
 * 		|								|
 * 		|								|
 * 		|-------------------------------|
 * 		|.  .  .  .  .  .  .  .  .  .  .|
 * 		\_______________________________/
 *
 * */
	display_transpose_axes = transpose;  // we need this to keep track of it when drawing
	st7789_write_cmd(0x36);  // Memory Data Access Control (Scanning order)
	st7789_write_byte((flip_y << 7) | (flip_x << 6) | (transpose << 5));  // Controls fb rotation/mirroring
	// See DS p.125 for detail values. [MV/MX/MY] <=> bits [5/6/7]
}


void st7789_draw_fragment(color_rgb_t* fragment, int x, int y, int xlen, int ylen) {
	st7789_set_window(x, y, x+xlen, y+ylen);

	// Todo: Implement handling for fragments larger than 32768 (32766) bytes (10922px area)
	if (xlen*ylen >= 10923)
		ESP_LOGE("st7789", "Attempt to draw a fragment with more than 10922 pixels (larger than a single transaction). "
						   "This functionality is not implemented and you will now crash and burn. Enjoy.");
	st7789_write_cmd(0x2c);
	st7789_write(ST_DAT, (uint8_t*)fragment, xlen*ylen*sizeof(color_rgb_t));
}


void nt_disp_fill(uint8_t r, uint8_t g, uint8_t b) {
	color_rgb_t* fragment = heap_caps_malloc(32766, MALLOC_CAP_DMA);
	for (int i=0; i < 32766/3; i+=1) {
		fragment[i].r = r;
		fragment[i].g = g;
		fragment[i].b = b;
	}

	st7789_set_window(0,0,320,240);

	st7789_write_cmd(0x2c);  // bulk pixel write
	st7789_write(ST_DAT, (uint8_t*)fragment, 32766);
	st7789_write(ST_DAT, (uint8_t*)fragment, 32766);
	st7789_write(ST_DAT, (uint8_t*)fragment, 32766);
	st7789_write(ST_DAT, (uint8_t*)fragment, 32766);
	st7789_write(ST_DAT, (uint8_t*)fragment, 32766);
	st7789_write(ST_DAT, (uint8_t*)fragment, 32766);
	st7789_write(ST_DAT, (uint8_t*)fragment, 32766);
	st7789_write(ST_DAT, (uint8_t*)fragment, 32766);
	heap_caps_free(fragment);
}


esp_err_t nt_disp_init() {
	gpio_set_level(CONFIG_DISP_PIN_RST, 0);  // start reset. We'll get back to this later

	// Todo: Handle this properly instead
//	spi_bus_config_t bus = {
//			.mosi_io_num = CONFIG_DISP_PIN_MOSI,
//			.miso_io_num = CONFIG_DISP_PIN_MISO,
//			.sclk_io_num = CONFIG_DISP_PIN_SCK,
//			.quadwp_io_num = -1,
//			.quadhd_io_num = -1,
//			.max_transfer_sz = 32768,
//			.flags = 0
//	};
//
//	ESP_RETURN_ON_ERROR(
//			spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO),
//			"display", "spi bus failed init"
//	);

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

	gpio_set_level(CONFIG_DISP_PIN_RST, 1);

	st7789_write_cmd(0x11);  // Exit sleep
	vTaskDelay(1);

	// https://www.rhydolabz.com/documents/33/ST7789.pdf
	st7789_write_cmd(0x3a);
	st7789_write_byte(0x66);  // 0x55 for RGB565, 0x66 for full color

	st7789_set_transforms(true, false, true);

	st7789_write_cmd(0x21);	// Invert display (so it's actually *normal*)
	st7789_write_cmd(0x29);	// Enable output

	nt_disp_fill(0xc3, 0x65, 0x1e);

	return ESP_OK;
}


void fb_render_char(uint8_t chr, int loc_x, int loc_y) {
	// This function simply places the character into the framebuffer at the specified location
	// Location should be specified in character-grid coordinates, not pixel coords
	// Also need to scale linearly from fg color to bg color: 255=>fg, 0=>bg
	static color_rgb_t pxbuf[font_size_x*font_size_y];
	int corner_x = loc_x * font_size_x;
	int corner_y = loc_y * font_size_y;

	for (int x = 0; x < font_size_x; x++) {
		for (int y = 0; y < font_size_y; y++) {
			uint8_t pxl = font_sheet[chr][(y * font_size_x) + x];
			pxbuf[x + (font_size_x*y)] = (color_rgb_t){pxl, pxl, pxl};
		}
	}
	st7789_draw_fragment(pxbuf, corner_x, corner_y, font_size_x, font_size_y);
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
