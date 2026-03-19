#include "rpi.h"
#include "gpio.h"
#include "pi-comm.h"
#include "libc/our-crc32.h"

static int g_role;

int comm_is_master(void) {
    gpio_set_input(COMM_PIN_ROLE);
    enum { GPIO_PUD = 0x3F200094, GPIO_PUDCLK0 = 0x3F200098 };
    PUT32(GPIO_PUD, 1);
    for (volatile int i = 0; i < 150; i++) {}
    PUT32(GPIO_PUDCLK0, 1 << COMM_PIN_ROLE);
    for (volatile int i = 0; i < 150; i++) {}
    PUT32(GPIO_PUD, 0);
    PUT32(GPIO_PUDCLK0, 0);
    dev_barrier();
    delay_us(10);
    return gpio_read(COMM_PIN_ROLE);
}

void comm_init(int role) {
    g_role = role;

    if (role) {
        gpio_set_output(COMM_PIN_SCLK);
        gpio_set_output(COMM_PIN_MOSI);
        gpio_set_input(COMM_PIN_MISO);
        gpio_set_input(COMM_PIN_READY);
        gpio_set_off(COMM_PIN_SCLK);
        gpio_set_off(COMM_PIN_MOSI);
    } else {
        gpio_set_input(COMM_PIN_SCLK);
        gpio_set_input(COMM_PIN_MOSI);
        gpio_set_output(COMM_PIN_MISO);
        gpio_set_output(COMM_PIN_READY);
        gpio_set_off(COMM_PIN_MISO);
        gpio_set_off(COMM_PIN_READY);
    }
    dev_barrier();
}

void comm_master_send_byte(uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        if (b & (1 << i))
            gpio_set_on(COMM_PIN_MOSI);
        else
            gpio_set_off(COMM_PIN_MOSI);

        gpio_set_on(COMM_PIN_SCLK);
        for (volatile int d = 0; d < 100; d++) {}
        gpio_set_off(COMM_PIN_SCLK);
        for (volatile int d = 0; d < 100; d++) {}
    }
}

uint8_t comm_master_recv_byte(void) {
    uint8_t b = 0;
    for (int i = 7; i >= 0; i--) {
        gpio_set_on(COMM_PIN_SCLK);
        for (volatile int d = 0; d < 100; d++) {}
        if (gpio_read(COMM_PIN_MISO))
            b |= (1 << i);
        gpio_set_off(COMM_PIN_SCLK);
        for (volatile int d = 0; d < 100; d++) {}
    }
    return b;
}

uint8_t comm_slave_recv_byte(void) {
    uint8_t b = 0;
    for (int i = 7; i >= 0; i--) {
        while (gpio_read(COMM_PIN_SCLK) == 0) {}
        if (gpio_read(COMM_PIN_MOSI))
            b |= (1 << i);
        while (gpio_read(COMM_PIN_SCLK) != 0) {}
    }
    return b;
}

void comm_slave_send_byte(uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        if (b & (1 << i))
            gpio_set_on(COMM_PIN_MISO);
        else
            gpio_set_off(COMM_PIN_MISO);
        while (gpio_read(COMM_PIN_SCLK) == 0) {}
        while (gpio_read(COMM_PIN_SCLK) != 0) {}
    }
}

void comm_signal_ready(void) {
    gpio_set_on(COMM_PIN_READY);
    dev_barrier();
}

void comm_signal_busy(void) {
    gpio_set_off(COMM_PIN_READY);
    dev_barrier();
}

void comm_wait_ready(void) {
    while (gpio_read(COMM_PIN_READY) == 0) {}
    dev_barrier();
}

void comm_wait_busy(void) {
    while (gpio_read(COMM_PIN_READY) != 0) {}
    dev_barrier();
}

void comm_send_bytes(const void *buf, uint32_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    if (g_role) {
        for (uint32_t i = 0; i < len; i++)
            comm_master_send_byte(p[i]);
    } else {
        for (uint32_t i = 0; i < len; i++)
            comm_slave_send_byte(p[i]);
    }
}

void comm_recv_bytes(void *buf, uint32_t len) {
    uint8_t *p = (uint8_t *)buf;
    if (g_role) {
        for (uint32_t i = 0; i < len; i++)
            p[i] = comm_master_recv_byte();
    } else {
        for (uint32_t i = 0; i < len; i++)
            p[i] = comm_slave_recv_byte();
    }
}

static uint32_t compute_msg_crc(const CommHeader *hdr, const void *payload) {
    uint32_t total = sizeof(CommHeader) + hdr->payload_len;
    uint8_t scratch[1200];
    if (total > sizeof(scratch)) {
        uint32_t c1 = our_crc32(hdr, sizeof(CommHeader));
        uint32_t c2 = our_crc32(payload, hdr->payload_len);
        return c1 ^ c2;
    }
    memcpy(scratch, hdr, sizeof(CommHeader));
    if (hdr->payload_len > 0)
        memcpy(scratch + sizeof(CommHeader), payload, hdr->payload_len);
    return our_crc32(scratch, total);
}

void comm_send_msg(uint8_t msg_type, uint16_t seq, uint16_t pos,
                   const void *payload, uint32_t payload_len) {
    CommHeader hdr;
    hdr.magic = COMM_MAGIC;
    hdr.msg_type = msg_type;
    hdr.seq = seq;
    hdr.pos = pos;
    hdr.payload_len = payload_len;

    uint32_t crc = compute_msg_crc(&hdr, payload);

    comm_send_bytes(&hdr, sizeof(hdr));
    if (payload_len > 0)
        comm_send_bytes(payload, payload_len);
    comm_send_bytes(&crc, sizeof(crc));
}

int comm_recv_msg(CommHeader *hdr, void *buf, uint32_t buf_max) {
    comm_recv_bytes(hdr, sizeof(CommHeader));

    if (hdr->magic != COMM_MAGIC)
        return -2;

    uint32_t plen = hdr->payload_len;
    if (plen > buf_max && plen > 0)
        return -3;

    if (plen > 0)
        comm_recv_bytes(buf, plen);

    uint32_t recv_crc;
    comm_recv_bytes(&recv_crc, sizeof(recv_crc));

    uint32_t calc_crc = compute_msg_crc(hdr, buf);

    if (recv_crc != calc_crc)
        return -1;

    return 0;
}

void comm_handshake(int role) {
    if (role) {
        printk("comm: master waiting for worker READY...\n");
        comm_wait_ready();

        printk("comm: worker READY, sending SYNC\n");
        comm_send_msg(MSG_SYNC, 0, 0, NULL, 0);

        while (gpio_read(COMM_PIN_READY) != 0) {}
        comm_wait_ready();

        CommHeader hdr;
        int rc = comm_recv_msg(&hdr, NULL, 0);
        if (rc != 0 || hdr.msg_type != MSG_SYNC)
            panic("comm: handshake failed (rc=%d, type=%d)\n", rc, hdr.msg_type);
        printk("comm: handshake complete\n");
    } else {
        printk("comm: worker signaling READY\n");
        comm_signal_ready();

        CommHeader hdr;
        int rc = comm_recv_msg(&hdr, NULL, 0);
        if (rc != 0 || hdr.msg_type != MSG_SYNC)
            panic("comm: handshake failed (rc=%d, type=%d)\n", rc, hdr.msg_type);

        comm_signal_busy();
        printk("comm: received SYNC, replying\n");

        comm_signal_ready();
        comm_send_msg(MSG_SYNC, 0, 0, NULL, 0);
        printk("comm: handshake complete\n");
    }
}
