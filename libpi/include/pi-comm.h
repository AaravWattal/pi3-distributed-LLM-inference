#ifndef __PI_COMM_H__
#define __PI_COMM_H__

#include <stdint.h>

#define COMM_PIN_SCLK   25
#define COMM_PIN_MOSI   24
#define COMM_PIN_MISO   23
#define COMM_PIN_READY  22
#define COMM_PIN_ROLE   27

#define MSG_ACTIVATION  0x01
#define MSG_TOKEN       0x02
#define MSG_SYNC        0x03
#define MSG_RESET       0x04

#define COMM_MAGIC      0xCAFE1337

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  msg_type;
    uint16_t seq;
    uint16_t pos;
    uint32_t payload_len;
} CommHeader;

int comm_is_master(void);
void comm_init(int role);
void comm_handshake(int role);

void comm_send_msg(uint8_t msg_type, uint16_t seq, uint16_t pos,
                   const void *payload, uint32_t payload_len);
int comm_recv_msg(CommHeader *hdr, void *buf, uint32_t buf_max);

void comm_signal_ready(void);
void comm_signal_busy(void);
void comm_wait_ready(void);

void comm_send_bytes(const void *buf, uint32_t len);
void comm_recv_bytes(void *buf, uint32_t len);

void comm_master_send_byte(uint8_t b);
uint8_t comm_master_recv_byte(void);
void comm_slave_send_byte(uint8_t b);
uint8_t comm_slave_recv_byte(void);

#endif
