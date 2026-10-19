; MCMON: a serial monitor for the 1802
; Membership Card (Rev J and later: Q is
; inverted, so Q low is the idle line).
; It lives in the ROM at 8000, 4800 baud,
; 8 bits, no parity, one stop bit.
;   D hhhh   dump 64 bytes from hhhh
;   M hhhh   put bytes in memory from hhhh:
;            type two hex digits for each
;            byte, ENTER to finish
;   G hhhh   run the program at hhhh (R0
;            is the PC, X = 0; to come back
;            jump to 8000)
;   ?        this list
; Letters may be typed in either case.
; Bit time is 104 machine cycles at 4 MHz;
; the delay loops give it.
; R2 stack, R3 PC, R4 call, R5 return, R6
; the link, R7.0 the character, R8.0 delay,
; R9.0 bit count, RA the address, RB counts,
; RC.0 the bits being sent, RD text, RE.0
; and RF scratch. A subroutine is called
; with SEP 4 and its address as two bytes
; after it, and leaves with SEP 5.
        ORG  8000
        LDI  80
        PHI  4
        PHI  5
        LDI  <CALLR
        PLO  4
        LDI  <RETR
        PLO  5
        LDI  7F
        PHI  2
        LDI  F0
        PLO  2          ; R2 = 7FF0
        LDI  >MAIN
        PHI  3
        LDI  <MAIN
        PLO  3
        SEX  2
        SEP  3

EXITC:  SEP  3          ; into the subroutine
CALLR:  PLO  0E         ; keep D
        GHI  6
        STXD
        GLO  6
        STXD            ; push the old link
        GHI  3
        PHI  6
        GLO  3
        PLO  6          ; link = the address bytes
        LDA  6
        PHI  3
        LDA  6
        PLO  3          ; R3 = the subroutine
        GLO  0E
        BR   EXITC

EXITR:  SEP  3          ; back in the caller
RETR:   PLO  0E
        GHI  6
        PHI  3
        GLO  6
        PLO  3          ; R3 = the return address
        IRX
        LDXA
        PLO  6
        LDX
        PHI  6          ; pop the old link
        GLO  0E
        BR   EXITR

; Send R7.0: a start bit, 8 bits least
; significant first, a stop bit.
TXS:    LDI  09
        PLO  9          ; 9 bits, the first is DF = 0
        GLO  7
        PLO  0C
        ADI  00         ; DF = 0
TB:     BNF  T0
        REQ             ; a 1: Q low
        BR   TD
T0:     SEQ             ; a 0: Q high
        SEX  2
TD:     LDI  0E
        PLO  8
TDL:    DEC  8
        GLO  8
        BNZ  TDL
        GLO  0C
        SHRC            ; the next bit to DF
        PLO  0C
        DEC  9
        GLO  9
        BNZ  TB
        REQ             ; the stop bit
        LDI  10
        PLO  8
TSL:    DEC  8
        GLO  8
        BNZ  TSL
        SEP  5

; Receive into R7.0: wait for the start
; bit (EF3 low), then read each bit in
; the middle of its time.
RXS:    BN3  RXS        ; EF3 high: no start yet
        LDI  08
        PLO  8          ; half a bit
RH:     DEC  8
        GLO  8
        BNZ  RH
        LDI  08
        PLO  9          ; 8 bits
RBIT:   LDI  0E
        PLO  8
RDLY:   DEC  8
        GLO  8
        BNZ  RDLY
        GLO  7
        SHR             ; room at the top
        B3   RZ         ; EF3 low is a 0
        ORI  80         ; high is a 1
RZ:     PLO  7
        DEC  9
        GLO  9
        BNZ  RBIT
RS:     B3   RS         ; wait for the stop bit
        SEP  5

        ORG  8100
; Send the character in D.
PUTC:   PLO  7
        SEP  4
        DB   >TXS, <TXS
        SEP  5

; Send CR and LF.
NEWL:   LDI  0D
        SEP  4
        DB   >PUTC, <PUTC
        LDI  0A
        SEP  4
        DB   >PUTC, <PUTC
        SEP  5

; Send the text at RD, up to its zero.
PSTR:   LDA  0D
        BZ   PSX
        SEP  4
        DB   >PUTC, <PUTC
        BR   PSTR
PSX:    SEP  5

; Send D as a hex digit.
PNIB:   ANI  0F
        ADI  30
        PLO  0F
        SMI  3A
        BNF  PN1
        GLO  0F
        ADI  07
        PLO  0F
PN1:    GLO  0F
        SEP  4
        DB   >PUTC, <PUTC
        SEP  5

; Send R7.0 as two hex digits.
PHEX:   GLO  7
        PHI  7          ; keep the byte
        SHR
        SHR
        SHR
        SHR
        SEP  4
        DB   >PNIB, <PNIB
        GHI  7
        SEP  4
        DB   >PNIB, <PNIB
        GHI  7
        PLO  7
        SEP  5

; Wait for a key, make it a capital, show
; it. R7.0 = the character.
GETC:   SEP  4
        DB   >RXS, <RXS
        GLO  7
        SMI  61
        BNF  GC1        ; below a
        SMI  1A
        BDF  GC1        ; above z
        GLO  7
        SMI  20
        PLO  7
GC1:    GLO  7
        SEP  4
        DB   >PUTC, <PUTC
        SEP  5

; R7.0 as a hex digit: DF = 1 and D = the
; value, or DF = 0 if it is not one.
HVAL:   GLO  7
        SMI  30
        BNF  HBAD       ; below 0
        PLO  0F
        SMI  0A
        BNF  HOK        ; 0 to 9
        GLO  0F
        SMI  11
        BNF  HBAD       ; between 9 and A
        PLO  0F
        SMI  06
        BDF  HBAD       ; above F
        GLO  0F
        ADI  0A
        SMI  00         ; DF = 1
        SEP  5
HOK:    GLO  0F
        SMI  00
        SEP  5
HBAD:   ADI  00         ; DF = 0
        SEP  5

; Read four hex digits into RA. DF = 1 if
; they were all good.
GETADR: LDI  20
        SEP  4
        DB   >PUTC, <PUTC
        LDI  00
        PHI  0A
        PLO  0A
        LDI  04
        PLO  0B
GA1:    SEP  4
        DB   >GETC, <GETC
        SEP  4
        DB   >HVAL, <HVAL
        BNF  GAERR
        STR  2          ; the digit, in the free slot
        LDI  04
        PHI  0F
GA2:    GLO  0A
        SHL
        PLO  0A
        GHI  0A
        SHLC
        PHI  0A         ; RA times 2, four times
        GHI  0F
        SMI  01
        PHI  0F
        BNZ  GA2
        GLO  0A
        OR
        PLO  0A
        GLO  0B
        SMI  01
        PLO  0B
        BNZ  GA1
        SEP  5          ; DF = 1 from the last SMI
GAERR:  ADI  00
        SEP  5

; Send RA as four hex digits.
PADR:   GHI  0A
        PLO  7
        SEP  4
        DB   >PHEX, <PHEX
        GLO  0A
        PLO  7
        SEP  4
        DB   >PHEX, <PHEX
        SEP  5

        ORG  8200
BANNER: DB   "MCMON 1802 MEMBERSHIP CARD"
        DB   0D, 0A, 00
HELPMSG: DB  "D hhhh  M hhhh  G hhhh  ?"
        DB   00
MAIN:   REQ             ; the line is idle
        LDI  >BANNER
        PHI  0D
        LDI  <BANNER
        PLO  0D
        SEP  4
        DB   >PSTR, <PSTR
PROMPT: SEP  4
        DB   >NEWL, <NEWL
        LDI  3E
        SEP  4
        DB   >PUTC, <PUTC
        SEP  4
        DB   >GETC, <GETC
        GLO  7
        XRI  44         ; D
        LBZ  DUMP
        GLO  7
        XRI  4D         ; M
        LBZ  MODIFY
        GLO  7
        XRI  47         ; G
        LBZ  GO
        GLO  7
        XRI  3F         ; ?
        LBZ  HELP
        GLO  7
        XRI  0D         ; ENTER: just a new prompt
        LBZ  PROMPT
BADIN:  LDI  3F
        SEP  4
        DB   >PUTC, <PUTC
        LBR  PROMPT

HELP:   SEP  4
        DB   >NEWL, <NEWL
        LDI  >HELPMSG
        PHI  0D
        LDI  <HELPMSG
        PLO  0D
        SEP  4
        DB   >PSTR, <PSTR
        LBR  PROMPT

DUMP:   SEP  4
        DB   >GETADR, <GETADR
        LBNF BADIN
        LDI  04
        PHI  0B         ; four lines
DL1:    SEP  4
        DB   >NEWL, <NEWL
        SEP  4
        DB   >PADR, <PADR
        LDI  3A
        SEP  4
        DB   >PUTC, <PUTC
        LDI  10
        PLO  0B         ; sixteen bytes
DL2:    LDI  20
        SEP  4
        DB   >PUTC, <PUTC
        LDA  0A
        PLO  7
        SEP  4
        DB   >PHEX, <PHEX
        GLO  0B
        SMI  01
        PLO  0B
        LBNZ DL2
        GHI  0B
        SMI  01
        PHI  0B
        LBNZ DL1
        LBR  PROMPT

MODIFY: SEP  4
        DB   >GETADR, <GETADR
        LBNF BADIN
        SEP  4
        DB   >NEWL, <NEWL
        SEP  4
        DB   >PADR, <PADR
        LDI  3A
        SEP  4
        DB   >PUTC, <PUTC
MD1:    SEP  4
        DB   >GETC, <GETC
        GLO  7
        XRI  0D
        LBZ  PROMPT     ; ENTER ends it
        GLO  7
        XRI  20
        LBZ  MD1        ; spaces are skipped
        SEP  4
        DB   >HVAL, <HVAL
        LBNF BADIN
        SHL
        SHL
        SHL
        SHL
        PHI  0B         ; the high digit
        SEP  4
        DB   >GETC, <GETC
        SEP  4
        DB   >HVAL, <HVAL
        LBNF BADIN
        STR  2
        GHI  0B
        OR              ; the byte
        STR  0A
        INC  0A
        LBR  MD1

GO:     SEP  4
        DB   >GETADR, <GETADR
        LBNF BADIN
        SEP  4
        DB   >NEWL, <NEWL
        GHI  0A
        PHI  0
        GLO  0A
        PLO  0
        SEX  0
        SEP  0
