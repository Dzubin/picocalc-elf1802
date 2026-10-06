; RANDTONE: three loops make a tone,
; and the 1861 shows where N is.
; Outer: a new random number N, on the
;   displays. Its high nibble is a row
;   and its low nibble a column of a
;   16 x 16 grid of 4 x 8 pixel blocks.
; Middle: toggles Q till the time is up.
; Inner: waits N turns between toggles.
; R7 counts every inner turn, so a note
; lasts the same time for any N; it
; ends when R7 high byte is 0 (MIDDLE).
; The picture is only 16 lines of 8
; bytes at 0400; the interrupt routine
; shows each line 8 times. All code
; that runs with the video on uses
; two-cycle instructions only. The
; blocks stay lit until IN is pressed.
MIDDLE  EQU  #18
PICHI   EQU  04     ; picture at 0400

        ORG  0000
        GHI  0      ; D = 0 (page 0)
        PHI  1
        PHI  2
        PHI  3
        PHI  6
        LDI  ISR
        PLO  1      ; R1: interrupt
        LDI  F0
        PLO  2      ; R2: N, and stack
        LDI  MAIN
        PLO  3      ; R3: main program
        LDI  SHOW
        PLO  6      ; R6: subroutine
        LDI  PICHI
        PHI  5      ; R5: the picture
        SEX  2
        INP  1      ; video on
        LDI  F0
        STR  2      ; the first N is F0
        SEP  3      ; R0 is for the DMA

; Outer loop. The carries make it a
; sequence of all 256 values.
MAIN:
NEXT:   LDN  2
        SMI  00     ; DF = 1 for the ADC
        ADC
        ADD
        ADD
        ADD
        STR  2
        OUT  4      ; N on the displays
        DEC  2
        SEP  6      ; light the block
        LDI  MIDDLE
        PHI  7

; Middle loop: wait, then toggle Q.
PERIOD: LDN  2
WAIT:   SMI  01     ; the inner loop
        DEC  7      ; counts every turn
        BNZ  WAIT
        BQ   QOFF   ; Q on: go to REQ
        SEQ         ; Q off: turn it on
        DB   38     ; and skip the REQ
QOFF:   REQ
        B4   ERASE  ; IN pressed: clear
AGAIN:  GHI  7
        BNZ  PERIOD
        BR   NEXT   ; a new note

; The IN button (the I key) clears the
; whole picture, 128 bytes at 0400.
ERASE:  LDI  00
        PLO  5      ; R5 = 0400
ERASE1: STR  5
        INC  5
        GLO  5
        SHL         ; DF = bit 7: done
        LDI  00
        BNF  ERASE1
        BR   AGAIN

; Subroutine: light the block for N.
; Its byte is at 0400 + N / 2, and the
; nibble depends on N bit 0.
SHOW:   LDN  2
        SHR         ; DF = N bit 0
        PLO  5
        LDI  F0     ; the left nibble
        BNF  LEFT
        LDI  0F     ; or the right one
LEFT:   SEX  5
        OR
        STR  5      ; set its pixels
        SEX  2
        SEP  3      ; back to MAIN
        BR   SHOW   ; for the next call

; The interrupt routine. It saves T and
; D on the stack at R2, sets R0 to the
; picture exactly 29 cycles after the
; interrupt, and then rewinds R0 so
; each picture line is shown 8 times.
; The DMA takes 8 of the 14 cycles of a
; line, leaving three two-cycle
; instructions: PLO 0 puts R0 back to
; the line's start (D) in lines 1 to 7
; of each 8, and GLO 0 saves the next
; start in line 8. The BN1 is in the
; 2nd slot of line 8, where EF1 is low
; only at the end of the picture. The
; RET is just before ISR, which R1
; points to.
LINES:  SEX  2      ; line 8, slot 3
LINE1:  PLO  0      ; line 1: rewind
        SEX  2
        SEX  2
        PLO  0      ; line 2
        SEX  2
        SEX  2
        PLO  0      ; line 3
        SEX  2
        SEX  2
        PLO  0      ; line 4
        SEX  2
        SEX  2
        PLO  0      ; line 5
        SEX  2
        SEX  2
        PLO  0      ; line 6
        SEX  2
        SEX  2
        PLO  0      ; line 7
        SEX  2
        SEX  2
        GLO  0      ; line 8: go on
        BN1  LINES  ; EF1 low: the end
        LDXA
        RET
ISR:    DEC  2
        SAV
        DEC  2
        STR  2
        NOP         ; three cycles
        LDI  PICHI
        PHI  0
        LDI  00
        PLO  0      ; R0 = 0400, D = 0
        SEX  2      ; four fillers,
        SEX  2      ; two cycles each
        SEX  2
        SEX  2
        BR   LINE1  ; the DMA starts
