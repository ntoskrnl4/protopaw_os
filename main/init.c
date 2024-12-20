//
// Created by ntoskrnl on 2024-12-16.
//
#include <driver/gpio.h>
#include <esp_app_desc.h>
#include <esp_log.h>
#include <esp_cpu.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <esp_heap_caps.h>
#include <driver/spi_master.h>

#include "init.h"
#include "sdcard.h"

// Code to test the system and initialize stuff at power-on


void init_dump_info() {

	// Print application info to console
	const esp_app_desc_t* app_info = esp_app_get_description();
	ESP_LOGI("post", "Loaded image binary:");
	ESP_LOGI("post", "    [project] %s", app_info->project_name);
	ESP_LOGI("post", "    [app_ver] %s", app_info->version);
	ESP_LOGI("post", "    [compile] %s", app_info->date);
	ESP_LOGI("post", "    [compile] %s", app_info->time);
	ESP_LOGI("post", "    [idf_ver] %s", app_info->idf_ver);
	ESP_LOGI("post", "    [sha_256] %s...", esp_app_get_elf_sha256_str());

	// Print system debug information to console
	esp_chip_info_t hwinfo;
	uint8_t mac[8];  // MACs are 6, but 802.15.4 support is 8, so just in case
	esp_efuse_mac_get_default(mac);
	ESP_LOGI("post", "System hardware:");
	esp_chip_info(&hwinfo);

	switch (hwinfo.model) {
		case CHIP_ESP32:
			ESP_LOGI("post", "    [chip_hw] ESP32 Original");
			break;
		case CHIP_ESP32S2:
			ESP_LOGI("post", "    [chip_hw] ESP32-S2");
			break;
		case CHIP_ESP32S3:
			ESP_LOGI("post", "    [chip_hw] ESP32-S3");
			break;
		case CHIP_ESP32C3:
			ESP_LOGI("post", "    [chip_hw] ESP32-C3");
			break;
		case CHIP_ESP32C2:
			ESP_LOGI("post", "    [chip_hw] ESP32-C2");
			break;
		case CHIP_ESP32C6:
			ESP_LOGI("post", "    [chip_hw] ESP32-C6");
			break;
		case CHIP_ESP32H2:
			ESP_LOGI("post", "    [chip_hw] ESP32-H2");
			break;
		case CHIP_ESP32P4:
			ESP_LOGI("post", "    [chip_hw] ESP32-P4");
			break;
		case CHIP_POSIX_LINUX:
			ESP_LOGI("post", "    [chip_hw] Simulator");
	}
	ESP_LOGI("post", "    [num_cpu] %dx", hwinfo.cores);
	ESP_LOGI("post", "    [cpu_rev] %d", hwinfo.revision);
	ESP_LOGI("post", "    [address] %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	if (hwinfo.features & CHIP_FEATURE_EMB_FLASH)
		ESP_LOGI("post", "    [feature] ESP-embedded Flash");
	if (hwinfo.features & CHIP_FEATURE_EMB_PSRAM)
		ESP_LOGI("post", "    [feature] ESP-embedded PSRAM");
	if (hwinfo.features & CHIP_FEATURE_WIFI_BGN)
		ESP_LOGI("post", "    [feature] 2.4GHz WiFi (802.11 b/g/n)");
	if (hwinfo.features & CHIP_FEATURE_BT)
		ESP_LOGI("post", "    [feature] Bluetooth");
	if (hwinfo.features & CHIP_FEATURE_BLE)
		ESP_LOGI("post", "    [feature] BLE");
	if (hwinfo.features & CHIP_FEATURE_IEEE802154)
		ESP_LOGI("post", "    [feature] IEEE 802.15.4");

	ESP_LOGI("post", "Current memory free, by malloc_heap_cap:");
	ESP_LOGI("post", "    [Default] %07d", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
	ESP_LOGI("post", "    [DMAable] %07d", heap_caps_get_free_size(MALLOC_CAP_DMA));
	ESP_LOGI("post", "    [Int.RAM] %07d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
	ESP_LOGI("post", "    [Ext.RAM] %07d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
	ESP_LOGI("post", "    [InstRAM] %07d", heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT));
}


void init_detect_features() {
	/* Features to detect:
	 * - SD card (first)
	 * - IMU
	 * - Display
	 * - Todo: PSRAM
	 * */
	nt_sdcard_init();
}


void nt_init_post() {
	// If everything is good, function will return
	// If there is a critical error (eg., PSRAM not detected), we will display error and infinite loop (or panic)

	/* Steps:
	 * 0. Turn on blue LED
	 * 1. Print application information to console
	 * 2. Print system debug information
	 * 3. Try to detect system features: PSRAM? SD card? LCD RO/RW? IMU? (SD card should be first)
	 * 	- Any sane SPI device should tristate its pins, so we can try detecting the display by enabling the
	 * 	  input pull-down/pull-up and see if the observed value changes. If the display is connected but
	 * 	  readonly (unmodded), it should return the same value for both (whether that's low or high).
	 * 	  If it *does* change, then we can try asking the ST7789 for its chip ID. If *that* doesn't work,
	 * 	  then we know we don't have anything on the bus.
	 * 4. Initialize screen, if present
	 * 5. POST status / error on boot
	 */


	// Start off by turning on the blue LED

#if CONFIG_HW_GPIO_STATUSLED != -1
	gpio_set_direction(CONFIG_HW_GPIO_STATUSLED, GPIO_MODE_OUTPUT);
	gpio_set_level(CONFIG_HW_GPIO_STATUSLED, 1);
#endif


	ESP_LOGI("post", "Welcome to ESP32 Protogen Paw OS");
	ESP_LOGI("post", "booting on CPU %d", esp_cpu_get_core_id());
	ESP_LOGI("post", "Status LED is GPIO %d. If not unset, it should have turned on.", CONFIG_HW_GPIO_STATUSLED);


	init_dump_info();


	// Set up the SPI bus here in init, so we know it's valid everywhere
	// todo: SPI bus initiation procedure: https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32s3/api-reference/peripherals/sdspi_share.html#initialization-sequence
	spi_bus_config_t bus = {
			.mosi_io_num = CONFIG_DISP_PIN_MOSI,
			.miso_io_num = CONFIG_DISP_PIN_MISO,
			.sclk_io_num = CONFIG_DISP_PIN_SCK,
			.quadwp_io_num = -1,
			.quadhd_io_num = -1,
			.max_transfer_sz = 32768,
			.flags = 0
	};
	ESP_ERROR_CHECK(
			spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO)
	);
	init_detect_features();

	gpio_set_level(CONFIG_HW_GPIO_STATUSLED, 0);
}
