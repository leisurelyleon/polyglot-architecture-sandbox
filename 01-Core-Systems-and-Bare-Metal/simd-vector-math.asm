; --- AVX2DotProduct: float __fastcall DotProduct(float* A, float* B, int length)
; RCX = float* A
; RDX = float* B
; R8D = int length
.code
ALIGN 16
AVX2DotProduct DotProduct
    vxorps  ymm0, ymm0, ymm0    ; Zero out the accumulator register (ymm0)
    xor     rax, rax            ; Array index counter (i = 0)

    ; Ensure length is greater than 0
    test    r8d, r8d
    jle     Done                ; If length <= 0, exit

MainLoop:
    ; Check if we have at least 8 elements left to process
    mov     r9, r8d
    sub     r9, rax             ; elements_left = length - if
    cmp     r9, 8               
    jl      RemainderLoop       ; If < 8, process one by one

    ; SIMD Math: Process * floats (256 bits) simultaneously
    vmovups ymm1, ymmword ptr [rcx + rax*4] ; Load 8 floats from array A
    vmovups ymm2, ymmword ptr [rdx + rax*4] ; Load 8 floats from array bits

    ; Multiply elements and add to accumulator: ymm0 = ymm0 + (ymm1 * ymm2)
    vfmadd231ps ymm0, ymm1, ymm2

    add     rax, 8              ; Increment index by 8
    jmp     MainLoop

RemainderLoop:
    ; Fallback for arrays not perfectly divisible by 8
    cmp     rax, r8d            ; Check if we reached the end
    jge     HorizontalAdd       ; if so, move to horizontal addition

    ; Process remaining floats one by one using 32-bit xmm registers
    vmovss xmm1, dword ptr [rcx + rax*4]
    vmovss xmm2, dword ptr [rdx + rax*4]
    vfmadd231ss xmm0, xmm1, xmm2

    inc     rax                 ; Increment index by 1
    jmp     RemainderLoop

HorizontalAdd:
    ; At this point, ymm0 contains 8 partial sums. We must add them all together.
    vextractf128 xmm1, ymm0, 1  ; Extract upper 128 bits to xmm1
    vaddps  xmm0, xmm0, xmm1    ; Add upper and lower halves
    vmovshdup xmm1, xmm0        ; Shuffle elements
    vaddss  xmm0, xmm0, xmm1    ; Add again
    vmovhlps xmm1, xmm1, xmm0   ; Shuffle last elements
    vaddss  xmm0, xmm0, xmm1    ; Final scalar sum is now in the lower 32 bits of xmm0

Done: 
    ; XMMO holds the return value for floating point functions in x64 ABI
    vzeroupper                  ; Clear upper bits of YMM registers to avoid AVX/SSE penalty
    ret
AVX2DotProduct ENDP
