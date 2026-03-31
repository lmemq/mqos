#include "types.h"
#include "bootboot.h"
#include "io.h"
#include "screen.hpp"

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
    uint16_t limit; 
    uint64_t base;  
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

    outb(0x21, 0xFD); 
    
    outb(0xA1, 0xFF);
}

void enable_sse() {
    uint64_t cr0, cr4;

    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    
    cr0 &= ~(1 << 2); // EM = 0
    cr0 |= (1 << 1);  // MP = 1
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    
    cr4 |= (1 << 9);  // OSFXSR = 1
    cr4 |= (1 << 10); // OSXMMEXCPT = 1
    
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
}


Screen login_screen;

Screen main_screen;

extern "C" {
    void keyboard_handler_asm(); 

    void keyboard_handler_main() {
        uint8_t scancode;
        scancode = inb(0x60);
        uint32_t *pixel = (uint32_t*)&fb;
        *pixel = 0xFF00FF00; 

        main_screen.flip();

        outb(0x20, 0x20);
    }
}

extern "C" void ignore_irq_asm();

uint32_t login_buffer_raw[1024 * 768];
uint32_t main_buffer_raw[1024 * 768];

extern "C" __attribute__((section(".text._start"))) void _start() {

    enable_sse();
    // uint32_t *pixel = (uint32_t*)&fb;
    // for(uint32_t i = 0; i < (bootboot.fb_width * bootboot.fb_height); i++) {
    //     pixel[i] = 0x00223344; 
    // }

    uint32_t w = bootboot.fb_width;
    uint32_t h = bootboot.fb_height;
    uint32_t screen_size = w * h;

    login_screen.init(login_buffer_raw, w, h);
    main_screen.init(main_buffer_raw, w, h);

    uint16_t current_cs;
    __asm__ ("mov %%cs, %0" : "=r"(current_cs));

    for(int i = 0; i < 256; i++) {
        set_idt_gate(i, (uint64_t)ignore_irq_asm, current_cs);
    }
    set_idt_gate(33, (uint64_t)keyboard_handler_asm, current_cs);

    //init_gdt();

    idtr.limit = (uint16_t)(sizeof(IDTEntry) * 256 - 1);
    idtr.base  = (uint64_t)&idt;

    __asm__ __volatile__("lidt %0" : : "m"(idtr));

    pic_init();
    while (inb(0x64) & 1) inb(0x60); 
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
