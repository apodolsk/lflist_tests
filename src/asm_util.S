/**
* @file   asm_util.S
* @author Alex Podolsky <apodolsk@andrew.cmu.edu>
* 
* @brief  Assorted ASM utility code.
* 
*/

.global xadd
.global xchg64b    
.global cmpxchg64b    
.global cmpxchg128b
    
## xadd:
##     lock xadd %edi, (%rsi)
##     mov %edi, %eax
##     ret

xchg64b:
    lock xchg %rdi, (%rsi)
    mov %rdi, %rax
    ret
    
cmpxchg64b:
    mov %rdx, %rax
    lock cmpxchg %rdi, (%rsi)
    ret
    
cmpxchg128b:
    push %rbx
    
    mov %rdi, %rbx
    mov %rcx, %rax
    mov %rsi, %rcx
    mov %rdx, %rsi
    mov %r8, %rdx

    lock cmpxchg16b  (%rsi)

    pop %rbx
    ret
    
