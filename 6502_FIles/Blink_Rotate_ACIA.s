bitsToRotate = $0200
save_accum = $0201
save_wait = $0202
PORTB = $8800
PORTA = $8801
DDRB = $8802
DDRA = $8803
T1_LC  = $8804	; Read T1 low-order-counter only
T1_HC  = $8805	; Read/Write T1 high-order-counter
T1_LL  = $8806	; Read/Write T1 low-order-latch
T1_HL  = $8807	; Read/Write T1 high-order-latch
ACR    = $880b  ; Auxiliary Control Register
IFR    = $880D  ; Interrupt Flag Register
T1_MODE0    = %00000000 	; One shot mode.
E_NOR = %11110111
E  = %00001000
RW = %00000100
RS = %00000010
ACIA_DATA = $8400
ACIA_STATUS = $8401
ACIA_COMMAND = $8402
ACIA_CONTROL = $8403

msToWait = 0xF0

  .org $8000
  .fill $2000, $EA
  .org $a000
reset:
  ldx #$ff
  txs

  lda #%11111111 ; Set all pins on port B to output
  sta DDRB
  
  jsr INIT_ACIA
  
  lda #%00101010
  sta bitsToRotate
cycle_again:
  jsr RX_CHAR
  jsr TX_CHAR
  lda bitsToRotate
  sta PORTB
  lda #msToWait		 
  sta save_wait	;store back
  jsr delay_ms
  lda bitsToRotate
  rol
  sta bitsToRotate
  lda bitsToRotate
  jmp cycle_again
  
INIT_ACIA:
    sta ACIA_STATUS    ; 1. Programmed Reset (any value clears internal registers)
    
    lda #%00001011     ; 2. Command Register config:
                       ;    Bits 7-5: 000 = No parity
                       ;    Bit 4:    0   = Normal receiver mode
                       ;    Bits 3-2: 10  = Transmit interrupt disabled, RTS low
                       ;    Bit 1:    1   = Receive interrupt disabled (polling mode)
                       ;    Bit 0:    1   = Data Terminal Ready (DTR) low / active
    sta ACIA_COMMAND

    lda #%00011111     ; 3. Control Register config:
                       ;    Bit 7:    0   = 1 stop bit
                       ;    Bits 6-5: 00  = 8 bits word length
                       ;    Bit 4:    1   = Use internal baud generator
                       ;    Bits 3-0: 1110= 19200 Baud (with 1.8432 MHz crystal)
                       ;    (For 9600 Baud, change bits 3-0 to 1111 -> #%00011111)
    sta ACIA_CONTROL
    rts
  
TX_CHAR:
    sta ACIA_DATA      ; Send the character stored in Accumulator (A)
    
    ; --- Delay Loop Workaround for W65C51 Bug ---
    ; Adjust cycles based on your 65C02 clock frequency.
    ; At 19200 baud, 1 character takes ~0.52ms to transmit.
    ; This loop takes ~0.525ms at a 1 MHz system clock.
    ldx #87           ; (2 cycles)
tx_delay:
    dex                ; (2 cycles)
    bne tx_delay       ; (3 cycles when branching, 2 when done)
    
    rts

RX_CHAR:
    lda ACIA_STATUS    ; Read ACIA status
    and #%00001000     ; Mask out Bit 3 (Receive Data Register Full)
    beq RX_CHAR        ; If 0, no data received yet. Keep polling.
    
    lda ACIA_DATA      ; Data is ready! Read it into Accumulator
    rts                ; Return with character in A	

delay_ms:			;Load X reg with delay time in ms before calling
	pha
	lda #T1_MODE0	;One shot
	sta ACR
loop_ms:
	lda #$E8		;0x03E8 is 1 ms when clock = 1MHZ
	sta T1_LL
	lda #$03
	sta T1_HC		;A write to H counter starts the clock 
wait_ms:
	lda save_wait
	lda IFR
	and #%01000000	;T1 has timed out when set
	beq wait_ms
	
	lda T1_LC		;clear interrupt flag by reading counter
	
	dec save_wait
	bne loop_ms		;repeat if x isn't 0	
	pla
	rts

  .org $fffc
  .word reset
  .word $0000