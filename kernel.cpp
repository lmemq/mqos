typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef short              int16_t;
typedef unsigned int       uint32_t;
typedef unsigned long int  uint64_t;

#include "bootboot.h"

extern "C" {
    extern BOOTBOOT bootboot;
    extern uint8_t fb;
    extern uint8_t _binary_font_psf_start;
}

void put_char(char c, int x, int y, uint32_t color) {
    uint8_t *font_ptr = (uint8_t*)&_binary_font_psf_start;
    uint8_t *glyph = font_ptr + 4 + (uint32_t)c * 16;
    uint32_t *screen = (uint32_t*)&fb;

    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 8; cx++) {
            if (glyph[cy] & (0x80 >> cx)) {
                screen[(y + cy) * bootboot.fb_width + (x + cx)] = color;
            }
        }
    }
}

void put_string(const char *str, int x, int y, uint32_t color) {
    while (*str) {
        put_char(*str++, x, y, color);
        x += 8;
    }
}

extern "C" __attribute__((section(".text._start"))) void _start() {
    uint32_t *pixel = (uint32_t*)&fb;
    for(uint32_t i = 0; i < (bootboot.fb_width * bootboot.fb_height); i++) {
        pixel[i] = 0x00223344; 
    }

    put_string("mqos x64", 100, 100, 0xFFFFFF00);
    put_string("by lmemq and Velikolepniy_Ro", 100, 120, 0xFFFFFFFF);

    while(1) __asm__ __volatile__("hlt");
}
