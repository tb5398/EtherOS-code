bits 32

section .multiboot
        align 4
        dd 0x1BADB002              ; magic number
        dd 0x00                    ; flags
        dd -(0x1BADB002 + 0x00)    ; checksum (REQUIRED)

section .text
global start
extern main

start:
        cli
        mov esp, stack_space
        call main
        hlt

section .bss
resb 16000
stack_space:
