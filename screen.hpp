#ifndef SCREEN_HPP
#define SCREEN_HPP

#include "types.h"
#include "bootboot.h"

extern "C" {
    extern BOOTBOOT bootboot;
    extern uint8_t fb;
    extern uint8_t _binary_font_psf_start;
    extern uint8_t _binary_wallpaper_bin_start;
    extern uint8_t _binary_wallpaper_bin_end;
}

class Screen {
private:
    uint32_t* fb_ptr;
    uint32_t width;
    uint32_t height;

public:
    void init(uint32_t* buffer, uint32_t w, uint32_t h);
    void print_char(char c, int x, int y, uint32_t color, int scale);
    void print_str(const char *str, int x, int y, uint32_t color, int scale);
    void draw_wallpaper();
    void fill(int x, int y, int w, int h, uint32_t color);
    void flip();
};

#endif
