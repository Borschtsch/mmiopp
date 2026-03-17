.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector, "a", %progbits
.global g_pfnVectors
g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word Default_Handler
  .word Default_Handler
  .word Default_Handler
  .word Default_Handler
  .word Default_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word Default_Handler
  .word Default_Handler
  .word 0
  .word Default_Handler
  .word Default_Handler

.text
.thumb_func
.global Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  ldr r0, =_sidata
  ldr r1, =_sdata
  ldr r2, =_edata
1:
  cmp r1, r2
  bcs 2f
  ldr r3, [r0], #4
  str r3, [r1], #4
  b 1b
2:
  ldr r1, =_sbss
  ldr r2, =_ebss
  movs r3, #0
3:
  cmp r1, r2
  bcs 4f
  str r3, [r1], #4
  b 3b
4:
  bl mmiocpp_target_main
  bl mmiocpp_target_exit
5:
  b 5b
.size Reset_Handler, . - Reset_Handler