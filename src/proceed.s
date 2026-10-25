; Save and restore VPRCED ($0202) and the PACTL interrupt enable bit.
.export _transport_begin, _transport_end, _transport_notified, _transport_sio
.import _receive_sio
.import __TRANSPORT_LOAD__, __TRANSPORT_SIZE__
.segment "TRANSHEADER"
    .word __TRANSPORT_LOAD__, __TRANSPORT_LOAD__+__TRANSPORT_SIZE__-1
.bss
old_vector: .res 2
old_enable: .res 1
pending: .res 1
gap_valid: .res 1
gap_tick: .res 1
.segment "TRANSPORT"
_transport_begin:
    lda #0
    sta gap_valid
    php
    sei
    lda $0202
    sta old_vector
    lda $0203
    sta old_vector+1
    lda $d302
    and #1
    sta old_enable
    lda #<proceed_irq
    sta $0202
    lda #>proceed_irq
    sta $0203
    lda #1
    sta pending
    lda $d302
    ora #1
    sta $d302
    plp
    rts
proceed_irq:
    lda #1
    sta pending
    pla
    rti
_transport_notified:
    php
    sei
    lda pending
    pha
    lda #0
    sta pending
    pla
    plp
    ldx #0
    rts
_transport_end:
    php
    sei
    lda $d302
    and #$fe
    sta $d302
    lda old_vector
    sta $0202
    lda old_vector+1
    sta $0203
    lda $d302
    ora old_enable
    sta $d302
    plp
    rts
_transport_sio:
    ; A two-tick separation guarantees at least one complete VBI period
    ; after the previous session transaction (16.7ms NTSC / 20ms PAL).
    ; RTCLOK keeps advancing in ROM SIO. Do not reset its clock.
    lda gap_valid
    beq gap_ready
wait_gap:
    lda $14
    sec
    sbc gap_tick
    cmp #2
    bcc wait_gap
gap_ready:
    ; Defer PROCEED during synchronous ROM SIO, preserving its enable bit.
    lda $d302
    and #1
    pha
    lda $d302
    and #$fe
    sta $d302
    lda $0302
    cmp #'R'
    beq continuous_read
    cmp #'S'
    beq continuous_read
    jsr $e459
    jmp transaction_done
continuous_read:
    jsr _receive_sio
transaction_done:
    lda $14
    sta gap_tick
    lda #1
    sta gap_valid
    pla
    ora $d302
    sta $d302
    lda $0303
    ldx #0
    rts
