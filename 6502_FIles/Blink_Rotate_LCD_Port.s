bitsToRotate = $0200
save_accum = $0201
save_wait = $0202
PORTB = $9000
PORTA = $9001
DDRB = $9002
DDRA = $9003
T1_LC  = $9004	; Read T1 low-order-counter only
T1_HC  = $9005	; Read/Write T1 high-order-counter
T1_LL  = $9006	; Read/Write T1 low-order-latch
T1_HL  = $9007	; Read/Write T1 high-order-latch
ACR    = $900b  ; Auxiliary Control Register
IFR    = $900D  ; Interrupt Flag Register
T1_MODE0    = %00000000 	; One shot mode.
E_NOR = %11110111
E  = %00001000
RW = %00000100
RS = %00000010

msToWait = 0xF0

  .org $8000
  .fill $2000, $EA
  .org $a000
reset:
  ldx #$ff
  txs

  lda #%11111111 ; Set all pins on port B to output
  sta DDRB
  
  lda #%00101010
  sta bitsToRotate
cycle_again:
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