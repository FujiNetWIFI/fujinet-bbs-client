#ifndef BBS_TRANSPORT_H
#define BBS_TRANSPORT_H
/* Single-owner raw N1: transport. No socket work runs in interrupt context. */
void transport_begin(void);
void transport_end(void);
unsigned char transport_notified(void);
unsigned char transport_sio(void);
unsigned char transport_open(const char *uri);
unsigned char transport_close(void);
unsigned char transport_status(unsigned int *waiting, unsigned char *connected,
                               unsigned char *error);
unsigned char transport_read(void *buffer, unsigned int length);
unsigned char transport_write(const void *buffer, unsigned int length);
#endif
