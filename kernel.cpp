typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef short              int16_t;
typedef unsigned int       uint32_t;
typedef unsigned long int  uint64_t;
typedef unsigned long int  uintptr_t; 

#include "bootboot.h"
#include "io.h"

extern "C" {
    extern BOOTBOOT bootboot;
    extern uint8_t fb;
    extern uint8_t _binary_font_psf_start;
    extern uint8_t _binary_wallpaper_bin_start;
    extern uint8_t _binary_wallpaper_bin_end;
}

// gdt
struct GDTDescriptor {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

uint64_t gdt[3] = {
    0,                  // Null-селектор
    0x00AF9A000000FFFF, // Code: 64-bit, Exec/Read, Ring 0
    0x00CF92000000FFFF  // Data: Read/Write, Ring 0
};

void init_gdt() {
    GDTDescriptor gdtr = { sizeof(gdt) - 1, (uint64_t)&gdt };
    __asm__ __volatile__("lgdt %0" : : "m"(gdtr));
}

// idt
struct IDTEntry {
    uint16_t offset_low;    // биты 0..15 адреса
    uint16_t selector;      // сегмент кода (0x08 из GDT)
    uint8_t  ist;           // 0
    uint8_t  type_attr;     // 0x8E (Interrupt Gate, Ring 0)
    uint16_t offset_mid;    // биты 16..31
    uint32_t offset_high;   // биты 32..63
    uint32_t zero;          // всегда 0
} __attribute__((packed));

IDTEntry idt[256];

struct IDTR {
    uint16_t limit; // Размер таблицы в байтах минус 1
    uint64_t base;  // Прямой адрес начала массива IDT в памяти
} __attribute__((packed)); 

IDTR idtr;

void set_idt_gate(int n, uint64_t handler, uint16_t current_cs) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].selector    = current_cs;
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E;
    idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].zero        = 0;
}

void pic_init() {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);

    // МАСКА: 0 - включено, 1 - выключено.
    // Выключаем всё кроме клавиатуры (биты: 11111101)
    outb(0x21, 0xFD); 
    
    // Выключаем ВООБЩЕ ВСЁ на втором контроллере (включая мышь)
    // 0xFF это 11111111
    outb(0xA1, 0xFF);
}

class Screen {
private:
    uint32_t* fb_ptr; // Указатель на буфер в памяти
    uint32_t width;
    uint32_t height;
public:
    void init(uint32_t* buffer, uint32_t w, uint32_t h) {
        this->fb_ptr = buffer;
        this->width = w;
        this->height = h;
    }

    void print_char(char c, int x, int y, uint32_t color, int scale) {
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

    void print_str(const char *str, int x, int y, uint32_t color, int scale) {
        while (*str) {
            this->print_char(*str, x, y, color, scale);
            str++;
            x += (8 * scale);
        }
    }

    void draw_wallpaper() {
        uint32_t *img_ptr = (uint32_t*)&_binary_wallpaper_bin_start;

        // bootboot.fb_scanline — это количество БАЙТ в одной строке экрана.
        // Делим на 4, чтобы получить количество ПИКСЕЛЕЙ (uint32_t).
        for (uint32_t y = 0; y < 768; y++) {
            for (uint32_t x = 0; x < 1024; x++) {
                // Пиксель в буфере экрана: y * ширина_строки + x
                // Пиксель в массиве картинки: y * ширина_картинки + x
                this->fb_ptr[y * this->width + x] = img_ptr[y * 1024 + x];
            }
        }
    }

    void fill(int x, int y, int w, int h, uint32_t color) {
        for (int cy = 0; cy < h; cy++) {
            for (int cx = 0; cx < w; cx++) {
                int canvas_x = x + cx;
                int canvas_y = y + cy;
                this->fb_ptr[canvas_y * this->width + canvas_x] = color;
            }
        }
    }

    void flip() {
        uint32_t *dest = (uint32_t*)&fb;
        // Pitch — это сколько ПИКСЕЛЕЙ в одной строке реального FB (с учетом отступов)
        uint32_t real_pitch = bootboot.fb_scanline / 4;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                dest[y * real_pitch + x] = fb_ptr[y * width + x];
            }
        }
    }

};

Screen login_screen;

Screen main_screen;

extern "C" {
    void keyboard_handler_asm(); 

    void keyboard_handler_main() {
        // Читаем скан-код из порта клавиатуры
        uint8_t scancode;
        scancode = inb(0x60);
        uint32_t *pixel = (uint32_t*)&fb;
        *pixel = 0xFF00FF00; // Ярко-зеленый

        main_screen.flip();

        // Говорим контроллеру прерываний, что мы закончили (EOI)
        outb(0x20, 0x20);
    }
}

extern "C" void ignore_irq_asm();

uint32_t login_buffer_raw[1024 * 768];
uint32_t main_buffer_raw[1024 * 768];

extern "C" __attribute__((section(".text._start"))) void _start() {
    // uint32_t *pixel = (uint32_t*)&fb;
    // for(uint32_t i = 0; i < (bootboot.fb_width * bootboot.fb_height); i++) {
    //     pixel[i] = 0x00223344; 
    // }

    uint32_t w = bootboot.fb_width;
    uint32_t h = bootboot.fb_height;
    uint32_t screen_size = w * h;
    
    // Опционально: третья область для шрифтов или иконок
    // uint32_t* next_buffer = main_buf + screen_size;

    login_screen.init(login_buffer_raw, w, h);
    main_screen.init(main_buffer_raw, w, h);

    uint16_t current_cs;
    __asm__ ("mov %%cs, %0" : "=r"(current_cs));

    for(int i = 0; i < 256; i++) {
        set_idt_gate(i, (uint64_t)ignore_irq_asm, current_cs);
    }
    set_idt_gate(33, (uint64_t)keyboard_handler_asm, current_cs);

    //init_gdt();

    // 3. Заполняем описатель IDTR
    idtr.limit = (uint16_t)(sizeof(IDTEntry) * 256 - 1);
    idtr.base  = (uint64_t)&idt;

    // 4. Загружаем таблицу в процессор
    __asm__ __volatile__("lidt %0" : : "m"(idtr));

    pic_init();

    // 5. РАЗРЕШАЕМ прерывания (команда STI)
    // До этого момента процессор их игнорировал
    while (inb(0x64) & 1) inb(0x60); // Читать, пока есть что давать
    __asm__ __volatile__("sti");

    login_screen.draw_wallpaper();

    login_screen.print_str("mqos x64", 100, 100, 0xFFFFFF00, 1);
    login_screen.print_str("by lmemq and Velikolepniy_Ro", 100, 120, 0xFFFFFFFF, 1);

    login_screen.print_str("Press any key to continue", 300, 360, 0xFFFF8C00, 2);

    main_screen.fill(0, 0, 1024, 768, 0xFFFFFFFF);

    login_screen.flip();

    while(1) {
        __asm__ __volatile__("hlt");
    }
}
