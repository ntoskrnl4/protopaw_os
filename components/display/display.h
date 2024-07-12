//
// Created by ntoskrnl on 2024-07-02.
//

#pragma once

#include <esp_err.h>

typedef uint16_t disp_color_t;
typedef disp_color_t disp_colour_t;

#define COLOR(r, g, b) (disp_color_t)((r & 0b11111000) << 8 | (g & 0b11111100) << 3 | (b & 0b11111000) >> 3)
#define FROM565R(x) (uint8_t)(x >> 11)
#define FROM565G(x) (uint8_t)((x >> 3) & 0b11111100)
#define FROM565B(x) (uint8_t)((x & 0b11111) << 3)

esp_err_t nt_disp_init();
void nt_disp_start();
void nt_disp_redraw();
void nt_disp_fill(uint8_t r, uint8_t g, uint8_t b);
void nt_disp_set_fg(uint8_t r, uint8_t g, uint8_t b);
void nt_disp_set_bg(uint8_t r, uint8_t g, uint8_t b);
void nt_disp_write_text(const char* str, size_t len, int loc_x, int loc_y);
