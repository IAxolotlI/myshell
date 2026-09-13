format ELF64

public context_switch
context_switch:
    push rdi
    push rbp    
    push rbx    
    push r12    
    push r13    
    push r14
    push r15

    mov [rdi], rsp

    mov rsp, rsi

    pop r15
    pop r14    
    pop r13    
    pop r12    
    pop rbx    
    pop rbp    
    pop rdi

    ret
