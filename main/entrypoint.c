#include <driver/gpio.h>
#include <stdio.h>
#include <esp_random.h>
#include <string.h>
#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <portmacro.h>
#include <math.h>
#include <esp_task_wdt.h>
#include "esp_err.h"
#include "display.h"
#include "imu.h"
#include "sdcard.h"
#include "init.h"


void nt_init_gpio() {
	// Required pins: Set direction
	gpio_set_direction(CONFIG_DISP_PIN_CS, GPIO_MODE_OUTPUT);
	gpio_set_direction(CONFIG_DISP_PIN_MOSI, GPIO_MODE_OUTPUT);
	gpio_set_direction(CONFIG_DISP_PIN_MISO, GPIO_MODE_INPUT);
	gpio_set_direction(CONFIG_DISP_PIN_SCK, GPIO_MODE_OUTPUT);
	gpio_set_direction(CONFIG_DISP_PIN_RST, GPIO_MODE_OUTPUT);
	gpio_set_direction(CONFIG_DISP_PIN_DC, GPIO_MODE_OUTPUT);

	// If needed: Set pull
	gpio_set_pull_mode(CONFIG_DISP_PIN_MISO, GPIO_PULLDOWN_ONLY);
	gpio_pulldown_en(CONFIG_DISP_PIN_MISO);

	// Optional pins
#if CONFIG_DISP_PIN_BKLT != -1
	gpio_set_direction(CONFIG_DISP_PIN_BKLT, GPIO_MODE_OUTPUT);
#endif
#if CONFIG_DISP_PIN_SD_CS != -1
	gpio_set_direction(CONFIG_DISP_PIN_SD_CS, GPIO_MODE_OUTPUT);
#endif
}


void app_main(void) {
	nt_init_post();
	nt_init_gpio();


//	// app_main TWDT
//	esp_task_wdt_add(NULL);


	ESP_ERROR_CHECK(nt_disp_init());

	char string[256];

//	nt_disp_write_text(string, 128, 0,0);
//	nt_disp_write_text("This is a test string to see if the render engine is working even remotely at all correctly.\n\nblah blah", 103, 0, 0);

	nt_imu_init();
	ESP_LOGI("app_main", "stack high water mark: -%d", uxTaskGetStackHighWaterMark(NULL));


	nt_disp_fill(0,0,0);
	fg = (color_rgb_t){0xff,0, 0};
	bg = (color_rgb_t){0,0,0xff};

	for(;;) {
		int len = sprintf(string, "IMU Debug:\n"
					  "AX = %+07.3f g\n"
					  "AY = %+07.3f g\n"
					  "AZ = %+07.3f g\n"
					  "Az = %+07.3f deg\n"
					  "El = %+07.3f deg\n",
					  imu_accel[0], imu_accel[1], imu_accel[2], orientation.az, orientation.el);
		nt_disp_write_text(string, len, 0, 0);
//		printf("Avg clocks lost to waiting on bus lock: %1.1f\n", (float)sum_bus_delay/acquire_count);
//		esp_task_wdt_reset();
		vTaskDelay(250/portTICK_PERIOD_MS);

	}
}

// Todo: Serial/command-line terminal to interface with the system

// Todo: Ability to read out files on the SD card, and load images

// Todo: Interrupt on debug button. dump sysinfo? direct to debug screen? Whar do

// Todo (!): switch imu to internal fifos so disp uninterrupted. imu rework to not depend on immediate realtime data
