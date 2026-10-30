; Read-only N: transactions. ROM configures/clocks and transmits the command
; with its existing VSEROR/VSEROC handlers; one private ISR then receives ACK,
; C/E, payload and checksum without a foreground buffer handoff.
.include "atari.inc"
.export _receive_sio
.segment "ZEROPAGE"
rx_ptr: .res 2
.bss
rx_vector: .res 2
rx_mask: .res 1
rx_critic: .res 1
rx_skctl: .res 1
rx_start: .res 2
rx_now: .res 2
rx_limit: .res 2
rx_command: .res 4
rx_left: .res 2
rx_sum: .res 1
rx_byte: .res 1
rx_result: .res 1
rx_done: .res 1
rx_phase: .res 1
.segment "TRANSPORT"
_receive_sio:
    php
    sei
    lda POKMSK
    sta rx_mask
    and #$c7
    sta POKMSK
    sta IRQEN
    lda VSERIN
    sta rx_vector
    lda VSERIN+1
    sta rx_vector+1
    lda #<receive_irq
    sta VSERIN
    lda #>receive_irq
    sta VSERIN+1
    lda CRITIC
    sta rx_critic
    lda SSKCTL
    sta rx_skctl
    lda #1
    sta CRITIC
    sta rx_result
    lda #0
    sta rx_done
    sta rx_phase
    sta rx_sum
    sta XMTDON
    sta CHKSNT
    sta CASFLG
    lda DBYTLO
    sta rx_left
    lda DBYTHI
    sta rx_left+1
    ; Only 1..256-byte read frames are supported.
    cmp #2
    bcs invalid_length
    cmp #1
    bne low_length
    lda rx_left
    bne invalid_length
    beq length_ok
low_length:
    lda rx_left
    bne length_ok
invalid_length:
    lda #139
    jmp finish_error
length_ok:
    lda DBUFLO
    sta rx_ptr
    lda DBUFHI
    sta rx_ptr+1
    lda DDEVIC
    clc
    adc DUNIT
    sec
    sbc #1
    sta rx_command
    lda DCOMND
    sta rx_command+1
    lda DAUX1
    sta rx_command+2
    lda DAUX2
    sta rx_command+3
    lda #0
    sta rx_limit+1
    lda DTIMLO
    ldx #6
limit_shift:
    asl
    rol rx_limit+1
    dex
    bne limit_shift
    sta rx_limit
sample_start:
    lda RTCLOK+1
    sta rx_start+1
    lda RTCLOK+2
    sta rx_start
    lda RTCLOK+1
    cmp rx_start+1
    bne sample_start
    lda #$34
    sta PBCTL
    cli
    ldx #8
    lda VCOUNT
command_delay:
    cmp VCOUNT
    beq command_delay
    lda VCOUNT
    dex
    bne command_delay
    sei
    lda #<rx_command
    sta BUFRLO
    clc
    adc #4
    sta BFENLO
    lda #>rx_command
    sta BUFRHI
    adc #0
    sta BFENHI
    lda #$28
    sta AUDF3
    lda #0
    sta AUDF4
    ; ROM enable-SEND vector and existing transmit interrupt handlers.
    ; Interrupts stay masked until checksum/counters and first byte are ready.
    jsr SENDEV
    lda rx_command
    sta CHKSUM
    sta SEROUT
    cli
wait_transmit:
    lda XMTDON
    bne arm_receive
    jsr deadline
    bcc wait_transmit
    jmp timeout
arm_receive:
    sei
    lda SSKCTL
    and #7
    ora #$10
    sta SSKCTL
    sta SKCTL
    sta SKRES
    lda POKMSK
    and #$c7
    ora #$20
    sta POKMSK
    sta IRQEN
    ; FujiNet waits for COMMAND release: receive is armed first.
    lda #$3c
    sta PBCTL
    cli
wait_reply:
    lda rx_done
    bne finish
    jsr deadline
    bcc wait_reply
timeout:
    lda #138
finish_error:
    sta rx_result
finish:
    sei
    lda POKMSK
    and #$c7
    sta POKMSK
    sta IRQEN
    lda rx_vector
    sta VSERIN
    lda rx_vector+1
    sta VSERIN+1
    lda #$3c
    sta PBCTL
    lda rx_skctl
    sta SSKCTL
    sta SKCTL
    lda #0
    sta AUDC1
    sta AUDC2
    sta AUDC3
    sta AUDC4
    lda rx_critic
    sta CRITIC
    lda rx_mask
    sta POKMSK
    sta IRQEN
    lda rx_result
    sta DSTATS
    plp
    ldx #0
    rts
; Carry set at the unchanged DTIMLO*64 total transaction budget.
deadline:
sample_now:
    lda RTCLOK+1
    sta rx_now+1
    lda RTCLOK+2
    sta rx_now
    lda RTCLOK+1
    cmp rx_now+1
    bne sample_now
    sec
    lda rx_now
    sbc rx_start
    tax
    lda rx_now+1
    sbc rx_start+1
    cmp rx_limit+1
    bne deadline_done
    cpx rx_limit
deadline_done:
    rts

receive_irq:
    tya
    pha
    lda SERIN
    sta rx_byte
    lda SKSTAT
    and #$a0
    cmp #$a0
    beq byte_ok
    lda #140
    sta rx_result
    sta SKRES
byte_ok:
    lda rx_phase
    beq ack
    cmp #1
    beq completion
    cmp #2
    beq payload
    cmp #3
    beq checksum
    jmp irq_exit
ack:
    lda rx_byte
    cmp #'A'
    bne bad_ack
    lda #1
    bne set_phase
bad_ack:
    lda #139
    bne failed
completion:
    lda rx_byte
    cmp #'C'
    beq completion_ok
    cmp #'E'
    bne bad_complete
    lda rx_result
    cmp #1
    bne completion_ok
    lda #144
    sta rx_result
completion_ok:
    lda #2
set_phase:
    sta rx_phase
    jmp irq_exit
bad_complete:
    lda #144
    bne failed
payload:
    ldy #0
    lda rx_byte
    sta (rx_ptr),y
    clc
    adc rx_sum
    adc #0
    sta rx_sum
    inc rx_ptr
    bne pointer_ok
    inc rx_ptr+1
pointer_ok:
    lda rx_left
    bne low_count
    dec rx_left+1
low_count:
    dec rx_left
    lda rx_left
    ora rx_left+1
    bne irq_exit
    lda #3
    sta rx_phase
    bne irq_exit
checksum:
    lda rx_byte
    cmp rx_sum
    beq done
    lda rx_result
    cmp #1
    bne done
    lda #143
failed:
    sta rx_result
done:
    lda #4
    sta rx_phase
    lda #1
    sta rx_done
irq_exit:
    pla
    tay
    pla
    rti
