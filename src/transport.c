#include <atari.h>
#include <peekpoke.h>
#include "transport.h"
#ifndef TRANSPORT_HOST_TEST
#pragma code-name ("TRANSPORT")
#endif

typedef struct
{
    unsigned char device, unit, command, direction;
    void *buffer;
    unsigned char timeout, unused;
    unsigned int length;
    unsigned char aux1, aux2;
} DCB;
#ifndef TRANSPORT_HOST_TEST
#define DCB_ADDRESS ((volatile DCB *)0x0300)
#else
extern volatile DCB test_dcb;
#define DCB_ADDRESS (&test_dcb)
#endif
static unsigned int available;

static unsigned char command(unsigned char cmd, unsigned char direction, void *buffer,
                             unsigned int length, unsigned char aux1, unsigned char aux2)
{
    unsigned char status;
    volatile DCB *dcb = DCB_ADDRESS;
    dcb->device = 0x71;
    dcb->unit = 1;
    dcb->command = cmd;
    dcb->direction = direction;
    dcb->buffer = buffer;
    /* Preserve the established N: command timeout. */
    dcb->timeout = 0x1f;
    dcb->unused = 0;
    dcb->length = length;
    dcb->aux1 = aux1;
    dcb->aux2 = aux2;
    OS.soundr = 0;
    status = transport_sio();
    return status == 1 ? 0 : 1;
}

unsigned char transport_open(const char *uri)
{
    available = 0;
    return command('O', 0x80, (void *)uri, 256, 12, 0);
}
unsigned char transport_close(void)
{
    available = 0;
    return command('C', 0, 0, 0, 0, 0);
}
unsigned char transport_status(unsigned int *waiting, unsigned char *connected,
                               unsigned char *error)
{
    unsigned char i, result;
    available = 0;
    *waiting = 0;
    *connected = 0;
    *error = 0;
    /* Match the baseline status destination and decode its bytes explicitly. */
    for (i = 0; i < 4; ++i)
        POKE(0x02ea + i, 0);
    result = command('S', 0x40, (void *)0x02ea, 4, 0, 0);
    if (result)
        return result;
    available = (unsigned int)PEEK(0x02ea) | ((unsigned int)PEEK(0x02eb) << 8);
    *waiting = available;
    *connected = PEEK(0x02ec);
    *error = PEEK(0x02ed);
    return 0;
}
unsigned char transport_read(void *buffer, unsigned int length)
{
    unsigned char result;
    if (!length || length > 256 || length > available)
        return 1;
    /* Every read consumes its authorization. The next read needs new status. */
    available = 0;
    result =
        command('R', 0x40, buffer, length, (unsigned char)length, (unsigned char)(length >> 8));
    return result;
}
unsigned char transport_write(const void *buffer, unsigned int length)
{
    available = 0;
    if (!length || length > 64)
        return 1;
    return command('W', 0x80, (void *)buffer, length, (unsigned char)length, 0);
}
