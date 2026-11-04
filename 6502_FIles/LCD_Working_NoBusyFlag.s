use_LCD_bsy = $0200
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

  .org $8000
  .fill $2000, $EA
  .org $a000
reset:
  ldx #$ff
  txs
  
  lda #0			; We can't use LCDBusy Flag yet
  sta use_LCD_bsy

  lda #%11111111 ; Set all pins on port B to output
  sta DDRB
  
  lda #15		 
  sta save_wait	;store back
  jsr delay_ms

  lda #%00110000
  jsr lcd_instruction_single  ;still in 8 bit mode
  lda #%00110000
  jsr lcd_instruction_single  ;still in 8 bit mode
  lda #%00110000
  jsr lcd_instruction_single  ;still in 8 bit mode
  
  lda #%00100000 ; Set 4-bit mode; 2-line display; 5x8 font
  jsr lcd_instruction_single
  ; now in 4 bit mode
  lda #%00100000 ; Set 4-bit mode; 2-line display; 5x8 font
  jsr lcd_instruction
  
  ;BUSY FLAG NOT WORKING YET
  ;lda #1			; We can now use LCDBusy Flag
  ;sta use_LCD_bsy
  
  lda #%00001110 ; display on
  jsr lcd_instruction
  
  lda #%00000110 ; entry mode
  jsr lcd_instruction
  
  lda #$00000001 ; Clear display
  jsr lcd_instruction

  ldx #0
print:
  lda message,x
  beq loop
  jsr print_char
  inx
  jmp print

loop:
  jmp loop

message: .asciiz "BR DR!"

lcd_wait:
  pha
  lda use_LCD_bsy
  bne lcdusebusy
  
  lda #5		  ;Delay 5 ms
  sta save_wait	;store back
  jsr delay_ms
  pla
  rts
  
lcdusebusy:
  pha
  txa
  pha ;really storing x
  lda #%00001111  ; Port B data is input
  sta DDRB
lcdbusy:
  lda #RW
  sta PORTB
  nop  ;delay 1us
  lda #(RW | E)
  sta PORTB
  nop  ;delay 1us
  lda PORTB
  and #%10000000
  tax
  lda #RW
  sta PORTB
  nop  ;delay 1us
  lda #(RW | E)
  sta PORTB
  nop  ;delay 1us
  lda PORTB
  txa
  bne lcdbusy

  lda #RW
  sta PORTB
  lda #%11111111  ; Port B is output
  sta DDRB
  pla ;really pulling x value
  tax ;restoring x
  pla
  rts

lcd_instruction:
  sta save_accum
  jsr lcd_wait
  and #$F0		  ; Clear RS/RW/E
  sta PORTB
  jsr toggle_e
  lda save_accum
  asl A			  ;shift left a nibble
  asl A
  asl A
  asl A
lcd_instruction_single:
  and #$F0		  ; Clear RS/RW/E
  sta PORTB
  jsr toggle_e  
  rts

print_char:
  jsr lcd_wait
  sta save_accum
  and #$F0		  ; Clear RS/RW/E
  ora #RS         ; Set RS
  sta PORTB
  jsr toggle_e
  lda save_accum
  asl A			  ;shift left a nibble
  asl A
  asl A
  asl A
  and #$F0		  ; Clear RS/RW/E
  ora #RS         ; Set RS
  sta PORTB
  jsr toggle_e  
  rts
  
toggle_e:
  ora #E		  ; Set E
  sta PORTB
  nop			  ;wait 1 us
  and #E_NOR	  ; clear E
  sta PORTB
  nop			  ;wait 1 us
  rts
  

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