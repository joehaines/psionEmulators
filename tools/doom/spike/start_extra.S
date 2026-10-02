    .syntax unified
    .arm
    .text
    .globl ScreenInfo
ScreenInfo:
    svc     #0x80007c
    mov     pc, lr

    .globl TickCountProbe
TickCountProbe:
    svc     #0x800070
    mov     pc, lr
