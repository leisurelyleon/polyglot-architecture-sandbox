; --- CheckHypervisorPresence: Detects if OS is running inside Hyper-V ---
.code
ALIGN 16
CheckHypervisorPresence PROC
    ; Prologue
    push    rbx
    push    rcx
    push    rdx

    ; Setup CPUID leaf 1 (Standard Feature Flags)
    mov     eax, 1
    cpuid                       ; Executes CPUID, populates EAX, EBX, ECX, EDX

    ; Check bit 31 of ECX (Hypervisor Present bit)
    bt      ecx, 31             ; Bit Test instruction
    jnc     NotVirtual          ; If Carry Flag not set, jump out

    ; Setup CPUID leaf 0x40000000 (Hypervisor Signature)
    mov     eax, 4000000h
    cpuid
    
    ; Check if the sugnature in EBX, ECX, EDX matches "Microsof" "t Hv"
    cmp     ebx, 7263694Dh      ; 'Micr'
    jne     NotVirtual
    cmp     ecx, 666F736Fh      ; 'osof'
    jne     NotVirtual
    cmp     edx, 76482075h      ; 't Hv'
    jne     NotVirtual

    ; Hyper-V is present
    mov     rax, 1              ; Return TRUE
    jmp     Cleanup

NotVirtual:
    xor     rax, rax            ; Return FALSE (0)

Cleanup:
    ; Epilogue
    pop     rdx
    pop     rcx
    pop     rbx
    Ret
CheckHypervisorPresence ENDP
