; --- MOS 6502 Commodore 64 Raster Interrupt ---
; This routine changes the border color exactly mid-screen.

* = $0800           ; Origin: Load program into memory at $0800

        SEI         ; Disable CPU interrupts so we don't get interrupted while setting up

        ; 1. Point the hardware interrupt vector to our custom routine
        LDA #<irq_handler   ; Load lower 8 bits of our hardler address
        STA $0314           ; Store in hardware vector low byte
        LDA #>irq_handler   ; Load upper 8 bits
        STA $0315           ; Store in hardware vector high byte

        ; 2. Congigure the VIC-II Video Chip
        LDA #$7F    ; Clear highest bit
        STA $DC0D   ; Turn off complex CIA timers
        LDA #$01    ; Enable raster interrupts
        STA $D01A   

        ; 3. Set the trigger scanline (Line 128)
        LDA #$80    ; Hex $80 = Decimal 128
        STA $D012   ; Tell video chip to trigger here

        ; 4. Clear the highest bit of the raster line (for lines < 255)
        LDA $D011
        AND #$7F
        STA $D011

        CLI         ; Re-enable interrupts

infinite_loop:
        JMP infinite_loop   ; Main game loop does nothing, waiting for the hardware interrupt

; --- The Interrupt Handler (Must be blistering fast) ---
irq_handler:
        ; Note: Hardware automatically pushed Processor Status and Program Counter to the stack
        PHA         ; Push Accumulator (A) to stack (3 cycles)
        TXA
        PHA         ; Push X register to stack (3 cycles)
        TYA     
        PHA         ; Push Y register to stack (3 cycles)

        ; 5. The Payload: Change the border color to Red ($02)
        LDA #$02
        STA $D020   ; Write directly to the Video Interface Chip border color register

        ; 6. Acknowledge the interrupt so it can happen again
        LSR $D019   ; Clear the raster interrupt flag by shifting right

        ; 7. Restore registers in reverse order
        PLA         ; Pull Y from stack
        TAY
        PLA         ; Pull X from stack
        TAX 
        PLA         ; Pull A from stack

        RIT         ; Return From Interrupt (Restores PC and Status, resumes infinite_loop)
