.global xadd
.global xchg
.global cmpxchg
.global cmpxchg2
    
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

xchg:
    push %rbp
    mov %rsp, %rbp
    
    lock xchg %rdi, (%rsi)
    mov %rdi, %rax

    pop %rbp    
    ret
    
cmpxchg:
    push %rbp
    mov %rsp, %rbp
    
    mov %rdx, %rax
    lock cmpxchg %rdi, (%rsi)

    pop %rbp    
    ret
    
cmpxchg2:
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
    
