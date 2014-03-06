.global xadd
.global xchg64b    
.global cmpxchg64b    
.global cmpxchg128b
    
    
## xadd:
##     lock xadd %edi, (%rsi)
##     mov %edi, %eax
##     ret

xadd:
    push %rbp
    mov %rsp, %rbp
    
    lock xadd %rdi, (%rsi)
    mov %rdi, %rax

    pop %rbp
    ret

xchg64b:
    push %rbp
    mov %rsp, %rbp
    
    lock xchg %rdi, (%rsi)
    mov %rdi, %rax

    pop %rbp    
    ret
    
cmpxchg64b:
    push %rbp
    mov %rsp, %rbp
    
    mov %rdx, %rax
    lock cmpxchg %rdi, (%rsi)

    pop %rbp    
    ret
    
cmpxchg128b:
    push %rbp
    mov %rsp, %rbp
    
    push %rbx
    
    mov %rdi, %rbx
    mov %rcx, %rax
    mov %rsi, %rcx
    mov %rdx, %rsi
    mov %r8, %rdx

    lock cmpxchg16b  (%rsi)

    pop %rbx
    
    pop %rbp    
    ret
    
