//
// Created by ntoskrnl on 2024-07-02.
//

#pragma once

#include <esp_err.h>

struct {
	uint8_t r, g, b;
} typedef color_rgb_t;
struct {
	uint8_t r, g, b, a;
} typedef color_rgba_t;

typedef color_rgb_t colour_rgb_t;
typedef color_rgba_t colour_rgba_t;

extern color_rgb_t fg, bg;

esp_err_t nt_disp_init();
void nt_disp_fill(uint8_t r, uint8_t g, uint8_t b);
void nt_disp_write_text(const char* str, size_t len, int loc_x, int loc_y);