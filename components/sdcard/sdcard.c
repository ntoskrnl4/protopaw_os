//
// Created by ntoskrnl on 2024-10-02.
//

#include <esp_err.h>
#include <sdmmc_cmd.h>
#include "sdcard.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"

sdspi_dev_handle_t dev;
sdmmc_host_t host;
sdmmc_card_t card;

bool nt_sdcard_enabled = false;


void nt_sdcard_init() {
#if CONFIG_DISP_PIN_SD_CS == -1
	ESP_LOGI("sdcard", "SD card explicitly force-disabled in build configuration");
	return;
#endif

	host = (sdmmc_host_t)SDSPI_HOST_DEFAULT();
	host.max_freq_khz = 5000*1;

	sdspi_device_config_t dvc_cfg = {
			.host_id = SPI2_HOST,
			.gpio_cs = CONFIG_DISP_PIN_SD_CS,
			.gpio_cd = SDSPI_SLOT_NO_CD,
			.gpio_wp = SDSPI_SLOT_NO_WP,
			.gpio_int = SDSPI_SLOT_NO_INT,
	};

	esp_vfs_fat_mount_config_t mnt_cfg = {
			.format_if_mount_failed = false,
			.allocation_unit_size = 512,
			.max_files = 32,
			.disk_status_check_enable = true,
	};
//	VFS_FAT_MOUNT_DEFAULT_CONFIG();  // why doesn't this exist??

	esp_err_t ret = esp_vfs_fat_sdspi_mount(
			// here goes nothing
		"/sdcard",
		&host,
		&dvc_cfg,
		&mnt_cfg,
		NULL
	);

	if (ret == ESP_ERR_INVALID_RESPONSE) {
		ESP_LOGE("sdcard", "ignore this error ^ it's fine to not have an SD card, but IDF logs errors even though it's okay");
	} else {
		ESP_ERROR_CHECK(ret);  // Todo: handle errors
		nt_sdcard_enabled = true;
	}

	ESP_LOGI("sdcard", "SD card? %d", nt_sdcard_enabled);

	if (nt_sdcard_enabled) {
		uint64_t free, total;
		ESP_ERROR_CHECK(esp_vfs_fat_info("/sdcard", &total, &free));
		ESP_LOGI("sdcard", "    [size] %llu", total);
		ESP_LOGI("sdcard", "    [free] %llu", free);
	}

}
