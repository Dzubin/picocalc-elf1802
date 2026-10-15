; MCMON: a tiny serial monitor for the 1802
; Membership Card (Rev J and later: Q is
; inverted, so Q low is the idle line).
; It lives in the ROM at 8000, says who it
; is and then echoes what is typed, 4800
; baud, 8 bits, no parity, one stop bit.
; The bit time is 104 machine cycles at
; 4 MHz; the delay loops give it.
; R2 stack, R3 main, R4 send, R5 receive,
; R6 text, R7.0 the character, R8.0 delay,
; R9.0 bit count. Subroutines are called
; with SEP and leave with SEP 3.
        ORG  8000
        LDI  80
        PHI  3
        PHI  4
        PHI  5
        PHI  6
        LDI  7F
        PHI  2
        LDI  F0
        PLO  2          ; R2 = 7FF0
        LDI  <MAIN
        PLO  3
        LDI  <TX
        PLO  4
        LDI  <RX
        PLO  5
        SEX  2
        SEP  3

MAIN:   REQ             ; the line is idle
        LDI  <BANNER
        PLO  6
SAY:    LDA  6
        BZ   ECHO
        PLO  7
        SEP  4          ; send R7.0
        BR   SAY

ECHO:   SEP  5          ; R7.0 = a character
        SEP  4          ; echo it
        GLO  7
        XRI  0D
        BNZ  ECHO
        LDI  0A         ; CR gets a line feed
        PLO  7
        SEP  4
        BR   ECHO

; Send R7.0: a start bit, 8 bits least
; significant first, a stop bit.
TXRET:  SEP  3
TX:     LDI  09
        PLO  9          ; 9 bits, the first is DF = 0
        GLO  7
        ADI  00         ; D = the byte, DF = 0
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
        GLO  7
        SHRC            ; the next bit to DF
        PLO  7
        DEC  9
        GLO  9
        BNZ  TB
        REQ             ; the stop bit
        LDI  10
        PLO  8
TSL:    DEC  8
        GLO  8
        BNZ  TSL
        BR   TXRET

; Receive into R7.0: wait for the start
; bit (EF3 low), then read each bit in
; the middle of its time.
RXRET:  SEP  3
RX:     BN3  RX         ; EF3 high: no start yet
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
        BR   RXRET

BANNER: DB   "MCMON 1802 MEMBERSHIP CARD"
        DB   0D, 0A, 00
