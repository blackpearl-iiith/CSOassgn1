#ifndef SHAM_H
#define SHAM_H

#include <stdint.h> // For fixed-width integers like uint32_t

// --- Configuration Constants ---
#define PAYLOAD_SIZE 1024 // Size of the data chunk in each packet
#define WINDOW_SIZE 10    // The sender's fixed congestion window size (in packets)
#define RTO_MS 500        // Retransmission Timeout in milliseconds

// --- S.H.A.M. Protocol Flags ---
#define SYN 0x1
#define ACK 0x2
#define FIN 0x4

// --- S.H.A.M. Header Structure ---
// This struct is packed to ensure consistent memory layout across systems.
struct sham_header {
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t flags;
    uint16_t window_size; // Flow control window size (receiver's buffer)
} __attribute__((packed));

// --- S.H.A.M. Packet Structure ---
// This combines the header and the application data payload.
struct sham_packet {
    struct sham_header header;
    char data[PAYLOAD_SIZE];
} __attribute__((packed));

// --- Function Prototypes for all functions in utils.c ---
void init_logger(const char* filename);
void close_logger(void);
void log_event(const char *format, ...);
int should_drop(float loss_rate);

#endif // SHAM_H