; --- KiSystemCall64: The entry point for all system calls in Windows x64 ---
.code
ALIGN 16
KiSystemCall64 PROC
    ; CPU automatically loaded RIP and CS from MSR_LSTAR and MSR_STAR
    ; Swap GS base to access the Kernel Processor Control Region (KPCR)
    swapgs

    ; Save the user's stack pointer and load the kernel stack pointer
    mov      gs:[10h], rsp      ; Save user RSP in KPCR
    mov      rsp, gs:[1A8h]     ; Load Kernel RSP from KPCR->TSS

    ; Build the Trap Frame (saving the volatile registers)
    sub     rsp, 200h           ; Allocate Trap Frame on kernel stack
    mov     [rsp+180h], rcx     ; Save user-mode RIP (passed in RCX by syscall)
    mov     [rsp+190h], r11     ; Save user-mode RFLAGS (passed in R11)

    ; Save general reigsters to prevent corruption
    mov     [rsp+120h], rax
    mov     [rsp+128h], rbx
    mov     [rsp+130h], rsi
    mov     [rsp+138h], rdi
    mov     [rsp+140h], rdp
    mov     [rsp+148h], r8
    mov     [rsp+150h], r9
    mov     [rsp+158h], r10

    ; ... System call routing logic happens here based on RAX ...

    ; Restore state and return to user mode
    mov     rcx, [rsp+180h]     ; Restore RIP
    mov     r11, [rsp+190h]     ; Restore RFLAGS
    mov     rsp, gs:[10h]       ; Restore User RSP
    swapgs                      ; Swap back to user GS base
    sysretq                     ; Execute hardware return to Ring 3 (User Mode)
KiSystemCall64 ENDP
