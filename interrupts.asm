[bits 64]
extern keyboard_handler_main
global keyboard_handler_asm

keyboard_handler_asm:
    ; Сохраняем абсолютно всё
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Магия выравнивания стека
    mov rbp, rsp          ; Сохраняем текущий RSP в RBP
    and rsp, -16          ; Обнуляем нижние 4 бита (выравнивание по 16 байт)
    sub rsp, 32           ; Резервируем "Shadow Space" (нужно для x64 вызовов)

    call keyboard_handler_main

    mov rsp, rbp          ; Возвращаем оригинальный RSP

    ; Восстанавливаем в обратном порядке
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    iretq

global ignore_irq_asm
ignore_irq_asm:
    push rax
    mov al, 0x20
    out 0x20, al  ; Говорим первому контроллеру (Master), что закончили
    out 0xA0, al  ; Говорим второму контроллеру (Slave), что закончили
    pop rax
    iretq

global fast_memcpy

fast_memcpy:
    ; rdi = куда (dest - адрес видеопамяти)
    ; rsi = откуда (src - ваш буфер в RAM)
    ; rdx = сколько байт (count)
    
    shr rdx, 4          ; делим количество байт на 16 (так как копируем по 16)
.loop:
    movdqu xmm0, [rsi]  ; загружаем 16 байт из RAM в регистр XMM0
    movdqu [rdi], xmm0  ; выгружаем 16 байт из XMM0 в видеопамять
    add rsi, 16
    add rdi, 16
    dec rdx
    jnz .loop
    ret