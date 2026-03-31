#include "screen.hpp"
extern "C" void fast_memcpy(void* dest, const void* src, uint64_t count);

void Screen::init(uint32_t* buffer, uint32_t w, uint32_t h) {
    this->fb_ptr = buffer;
    this->width = w;
    this->height = h;
}

void Screen::print_char(char c, int x, int y, uint32_t color, int scale) {
    uint8_t *font_ptr = (uint8_t*)&_binary_font_psf_start;
    uint8_t *glyph = font_ptr + 4 + (uint32_t)c * 16;
    
    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 8; cx++) {
            if (glyph[cy] & (0x80 >> cx)) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        int canvas_x = x + (cx * scale) + sx;
                        int canvas_y = y + (cy * scale) + sy;
                        this->fb_ptr[canvas_y * this->width + canvas_x] = color;
                    }
                }
            }
        }
    }
}

void Screen::print_str(const char *str, int x, int y, uint32_t color, int scale) {
    while (*str) {
        print_char(*str, x, y, color, scale);
        str++;
        x += (8 * scale);
    }
}

void Screen::draw_wallpaper() {
    uint32_t *img_ptr = (uint32_t*)&_binary_wallpaper_bin_start;
    for (uint32_t y = 0; y < 768; y++) {
        for (uint32_t x = 0; x < 1024; x++) {
            this->fb_ptr[y * this->width + x] = img_ptr[y * 1024 + x];
        }
    }
}

void Screen::fill(int x, int y, int w, int h, uint32_t color) {
    for (int cy = 0; cy < h; cy++) {
        for (int cx = 0; cx < w; cx++) {
            this->fb_ptr[(y + cy) * this->width + (x + cx)] = color;
        }
    }
}

void Screen::flip() {
    // uint32_t *dest = (uint32_t*)&fb;
    // uint32_t real_pitch = bootboot.fb_scanline / 4;

    // for (uint32_t y = 0; y < height; y++) {
    //     for (uint32_t x = 0; x < width; x++) {
    //         dest[y * real_pitch + x] = fb_ptr[y * width + x];
    //     }
    // }

    fast_memcpy((void*)bootboot.fb_ptr, this->fb_ptr, 1024 * 768 * 4);
}
