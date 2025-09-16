#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <time.h>
#include <errno.h>
#include "sham.h"

/* --------------------------------------------------------
   Data Structures & Helpers
   -------------------------------------------------------- */
typedef struct {
    struct sham_packet packet;
    struct timeval send_time;
    int is_acked;
    size_t data_len;
} inflight_packet;

/* Compute time difference in milliseconds */
long get_time_diff_ms(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) * 1000L +
           (end->tv_usec - start->tv_usec) / 1000L;
}

/* --------------------------------------------------------
   File Transfer Function
   -------------------------------------------------------- */
void send_file(int sockfd,
               struct sockaddr_in *server_addr,
               const char *input_filename,
               const char *output_filename,
               uint32_t seq_num_start) 
{
    /* --- Open File --- */
    FILE *input_file = fopen(input_filename, "rb");
    if (!input_file) {
        perror("Failed to open input file");
        return;
    }

    /* --- Variables for Sliding Window --- */
    uint32_t base = seq_num_start;
    uint32_t next_seq_num = seq_num_start;
    uint16_t receiver_window_size = PAYLOAD_SIZE;

    /* --- Send Filename First --- */
    struct sham_packet name_packet, name_ack_packet;
    memset(&name_packet, 0, sizeof(name_packet));
    name_packet.header.seq_num = htonl(next_seq_num);

    strncpy(name_packet.data, output_filename, PAYLOAD_SIZE - 1);
    size_t name_len = strlen(output_filename) + 1;

    log_event("SND DATA SEQ=%u (Filename)", next_seq_num);
    sendto(sockfd, &name_packet, sizeof(struct sham_header) + name_len,
           0, (struct sockaddr *)server_addr, sizeof(*server_addr));

    recvfrom(sockfd, &name_ack_packet, sizeof(name_ack_packet), 0, NULL, NULL);
    receiver_window_size = ntohs(name_ack_packet.header.window_size);

    log_event("RCV ACK=%u", ntohl(name_ack_packet.header.ack_num));
    log_event("FLOW WIN UPDATE=%u", receiver_window_size);

    base += name_len;
    next_seq_num += name_len;

    /* --- Initialize Window --- */
    inflight_packet window[WINDOW_SIZE];
    for (int i = 0; i < WINDOW_SIZE; ++i) window[i].is_acked = 1;

    int file_finished = 0;
    printf("Starting file transfer...\n");

    /* --- File Transfer Loop --- */
    while (!file_finished || base < next_seq_num) {
        /* Count in-flight packets */
        int inflight_count = 0;
        for (int i = 0; i < WINDOW_SIZE; ++i)
            if (!window[i].is_acked) inflight_count++;

        /* Send new packets if window allows */
        while (inflight_count < WINDOW_SIZE &&
               (next_seq_num - base) < receiver_window_size &&
               !file_finished) 
        {
            char buffer[PAYLOAD_SIZE];
            ssize_t bytes_read = fread(buffer, 1, PAYLOAD_SIZE, input_file);
            if (bytes_read <= 0) { file_finished = 1; break; }

            int free_slot = -1;
            for (int i = 0; i < WINDOW_SIZE; ++i)
                if (window[i].is_acked) { free_slot = i; break; }

            if (free_slot != -1) {
                inflight_packet *p = &window[free_slot];
                p->packet.header.seq_num = htonl(next_seq_num);
                p->data_len = bytes_read;
                memcpy(p->packet.data, buffer, bytes_read);
                gettimeofday(&p->send_time, NULL);
                p->is_acked = 0;

                log_event("SND DATA SEQ=%u LEN=%zu", next_seq_num, bytes_read);
                sendto(sockfd, &p->packet, sizeof(struct sham_header) + p->data_len,
                       0, (struct sockaddr*)server_addr, sizeof(*server_addr));

                next_seq_num += bytes_read;
                inflight_count++;
            } else {
                break;
            }
        }

        /* --- Wait for ACKs --- */
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        struct timeval timeout = {0, 100000};

        int activity = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        if (activity > 0) {
            struct sham_packet ack_packet;
            recvfrom(sockfd, &ack_packet, sizeof(ack_packet), 0, NULL, NULL);

            if (ack_packet.header.flags & ACK) {
                receiver_window_size = ntohs(ack_packet.header.window_size);
                uint32_t acked_num = ntohl(ack_packet.header.ack_num);

                log_event("RCV ACK=%u", acked_num);
                log_event("FLOW WIN UPDATE=%u", receiver_window_size);

                base = (acked_num > base) ? acked_num : base;
                for (int i = 0; i < WINDOW_SIZE; ++i) {
                    if (!window[i].is_acked &&
                        (ntohl(window[i].packet.header.seq_num) + window[i].data_len) <= base) {
                        window[i].is_acked = 1;
                    }
                }
            }
        }

        /* --- Retransmission Timeout (RTO) --- */
        struct timeval now;
        gettimeofday(&now, NULL);

        for (int i = 0; i < WINDOW_SIZE; ++i) {
            if (!window[i].is_acked &&
                get_time_diff_ms(&window[i].send_time, &now) > RTO_MS) 
            {
                uint32_t seq_to_retx = ntohl(window[i].packet.header.seq_num);
                log_event("TIMEOUT SEQ=%u", seq_to_retx);
                log_event("RETX DATA SEQ=%u LEN=%zu", seq_to_retx, window[i].data_len);

                sendto(sockfd, &window[i].packet,
                       sizeof(struct sham_header) + window[i].data_len,
                       0, (struct sockaddr*)server_addr, sizeof(*server_addr));

                gettimeofday(&window[i].send_time, NULL);
            }
        }
    }


    // --- ROBUST 4-WAY HANDSHAKE (CLIENT SIDE) ---
    struct sham_packet fin_packet, received_packet;
    memset(&fin_packet, 0, sizeof(fin_packet));
    fin_packet.header.seq_num = htonl(next_seq_num);
    fin_packet.header.flags = FIN;
    
    int fin_ack_received = 0;
    
    // 1. Send FIN and wait for ACK. Retry on timeout.
    for(int i = 0; i < 5 && !fin_ack_received; i++) { // Try up to 5 times
        log_event("SND FIN SEQ=%u", next_seq_num);
        sendto(sockfd, &fin_packet, sizeof(fin_packet.header), 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
        
        // The socket already has a receive timeout. If recvfrom returns < 0, it timed out.
        ssize_t bytes = recvfrom(sockfd, &received_packet, sizeof(received_packet), 0, NULL, NULL);
        if(bytes > 0 && (received_packet.header.flags & ACK) && (ntohl(received_packet.header.ack_num) == next_seq_num + 1)) {
            log_event("RCV ACK FOR FIN");
            fin_ack_received = 1;
        } else {
            log_event("TIMEOUT waiting for FIN-ACK, retrying...");
        }
    }

    if(!fin_ack_received){
        fprintf(stderr, "Termination failed: No ACK for FIN received.\n");
    } else {
        // 2. Now wait for the server's FIN. The packet we just received might already have it.
        int server_fin_received = (received_packet.header.flags & FIN);
        while(!server_fin_received){
            // This recvfrom will also use the socket timeout
            ssize_t bytes = recvfrom(sockfd, &received_packet, sizeof(received_packet), 0, NULL, NULL);
            if(bytes > 0 && (received_packet.header.flags & FIN)){
                server_fin_received = 1;
            } else if (bytes < 0) {
                // If we time out waiting for the server's FIN, something is wrong.
                fprintf(stderr, "Termination failed: Timed out waiting for server FIN.\n");
                break;
            }
        }

        if(server_fin_received) {
            log_event("RCV FIN SEQ=%u", ntohl(received_packet.header.seq_num));
            uint32_t server_fin_seq = ntohl(received_packet.header.seq_num);

            // 3. Send final ACK for server's FIN
            struct sham_packet final_ack;
            memset(&final_ack, 0, sizeof(final_ack));
            final_ack.header.flags = ACK;
            final_ack.header.ack_num = htonl(server_fin_seq + 1);
            log_event("SND ACK FOR FIN");
            sendto(sockfd, &final_ack, sizeof(final_ack.header), 0, (struct sockaddr *)server_addr, sizeof(*server_addr));
        }
    }
    
    printf("File transfer complete.\n");
    fclose(input_file);
}

/* --------------------------------------------------------
   Chat Mode
   -------------------------------------------------------- */
void chat_mode(int sockfd, struct sockaddr_in *peer_addr, socklen_t peer_len) {
    printf("\nChat mode activated. Type '/quit' to exit.\n\n");

    fd_set read_fds;
    char send_buffer[PAYLOAD_SIZE];
    struct sham_packet recv_packet;
    uint32_t my_seq = rand() % 1000;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sockfd, &read_fds);

        int max_fd = (STDIN_FILENO > sockfd) ? STDIN_FILENO : sockfd;
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
        }

        /* --- Send User Input --- */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(send_buffer, sizeof(send_buffer), stdin)) {
                send_buffer[strcspn(send_buffer, "\n")] = 0;

                if (strcmp(send_buffer, "/quit") == 0) {
                    printf("Initiating shutdown...\n");
                            
                    // 1. Send our FIN
                    struct sham_packet fin_packet;
                    memset(&fin_packet, 0, sizeof(fin_packet));
                    fin_packet.header.flags = FIN;
                    fin_packet.header.seq_num = htonl(my_seq);
                    log_event("SND FIN SEQ=%u", my_seq);
                    sendto(sockfd, &fin_packet, sizeof(fin_packet.header), 0, (const struct sockaddr*)peer_addr, peer_len);

                    // 2. Wait for the server's ACK and FIN
                    int ack_received = 0, fin_received = 0;
                    uint32_t server_fin_seq = 0;
                    while(!ack_received || !fin_received) {
                    ssize_t bytes = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, NULL, NULL);
                    if(bytes > 0){
                        if(recv_packet.header.flags & ACK) {
                            log_event("RCV ACK FOR FIN");
                            ack_received = 1;
                        }
                        if(recv_packet.header.flags & FIN) {
                            log_event("RCV FIN SEQ=%u", ntohl(recv_packet.header.seq_num));
                            server_fin_seq = ntohl(recv_packet.header.seq_num);
                            fin_received = 1;
                        }
                    } else {
                        // Timeout, break to let main loop handle exit
                        break;
                    }
                }
                            
                // 3. Send the final ACK
                if(fin_received) {
                    struct sham_packet final_ack;
                    memset(&final_ack, 0, sizeof(final_ack));
                    final_ack.header.flags = ACK;
                    final_ack.header.ack_num = htonl(server_fin_seq + 1);
                    log_event("SND ACK FOR FIN");
                    sendto(sockfd, &final_ack, sizeof(final_ack.header), 0, (const struct sockaddr*)peer_addr, peer_len);
                }
                            
                break; // Exit chat loop
            }

                struct sham_packet data_packet;
                memset(&data_packet, 0, sizeof(data_packet));
                data_packet.header.seq_num = htonl(my_seq++);
                strncpy(data_packet.data, send_buffer, PAYLOAD_SIZE);

                log_event("SND DATA SEQ=%u (Chat)", ntohl(data_packet.header.seq_num));
                sendto(sockfd, &data_packet,
                       sizeof(struct sham_header) + strlen(send_buffer) + 1,
                       0, (struct sockaddr*)peer_addr, peer_len);
            }
        }

        /* --- Receive Messages --- */
        if (FD_ISSET(sockfd, &read_fds)) {
            ssize_t bytes = recvfrom(sockfd, &recv_packet, sizeof(recv_packet),
                                     0, (struct sockaddr*)peer_addr, &peer_len);

            if (bytes > 0) {
                if (recv_packet.header.flags & FIN) {
                    log_event("RCV FIN SEQ=%u", ntohl(recv_packet.header.seq_num));
                    printf("Peer has disconnected. Closing chat.\n");

                    struct sham_packet ack_packet;
                    memset(&ack_packet, 0, sizeof(ack_packet));
                    ack_packet.header.flags = ACK;
                    ack_packet.header.ack_num = htonl(ntohl(recv_packet.header.seq_num) + 1);

                    sendto(sockfd, &ack_packet, sizeof(ack_packet.header),
                           0, (struct sockaddr*)peer_addr, peer_len);
                    break;
                }

                log_event("RCV DATA SEQ=%u (Chat)", ntohl(recv_packet.header.seq_num));
                printf("Peer: %s\n", recv_packet.data);
            }
        }
    }
}

/* --------------------------------------------------------
   Setup Socket
   -------------------------------------------------------- */
int setup_socket(struct sockaddr_in *server_addr, const char *server_ip, int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket creation failed"); return -1; }

    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr->sin_addr) <= 0) {
        perror("invalid address");
        close(sockfd);
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/* --------------------------------------------------------
   Main
   -------------------------------------------------------- */
int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr,
            "Usage: ./client <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n"
            "   OR: ./client <server_ip> <server_port> --chat [loss_rate]\n");
        return 1;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int is_chat_mode = (strcmp(argv[3], "--chat") == 0);

    /* Parse arguments */
    float loss_rate = 0.0;
    const char *input_file = NULL;
    const char *output_file = NULL;

    if (is_chat_mode) {
        if (argc > 4) loss_rate = atof(argv[4]);
    } else {
        if (argc < 5) {
            fprintf(stderr, "Usage: ./client <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n");
            return 1;
        }
        input_file = argv[3];
        output_file = argv[4];
        if (argc > 5) loss_rate = atof(argv[5]);
    }

    /* --- Setup --- */
    init_logger("client_log.txt");
    srand(time(NULL));

    struct sockaddr_in server_addr;
    int sockfd = setup_socket(&server_addr, server_ip, server_port);
    if (sockfd < 0) { close_logger(); return 1; }

    printf("Connecting to server at %s:%d...\n", server_ip, server_port);

    /* --- Handshake --- */
    struct sham_packet syn_packet, syn_ack_packet, ack_packet;
    uint32_t client_isn = rand() % 10000;

    memset(&syn_packet, 0, sizeof(syn_packet));
    syn_packet.header.seq_num = htonl(client_isn);
    syn_packet.header.flags = SYN;

    log_event("SND SYN SEQ=%u", client_isn);
    sendto(sockfd, &syn_packet, sizeof(syn_packet.header),
           0, (struct sockaddr *)&server_addr, sizeof(server_addr));

    ssize_t bytes_received = recvfrom(sockfd, &syn_ack_packet, sizeof(syn_ack_packet), 0, NULL, NULL);
    if (bytes_received < 0) {
        perror("recvfrom failed");
        fprintf(stderr, "Handshake failed: No response from server.\n");
        close(sockfd);
        close_logger();
        return 1;
    }

    uint32_t server_isn = ntohl(syn_ack_packet.header.seq_num);
    uint32_t ack_for_client = ntohl(syn_ack_packet.header.ack_num);

    if ((syn_ack_packet.header.flags == (SYN | ACK)) && (ack_for_client == client_isn + 1)) {
        log_event("RCV SYN-ACK SEQ=%u ACK=%u", server_isn, ack_for_client);
    } else {
        fprintf(stderr, "Handshake failed: Invalid SYN-ACK received.\n");
        close(sockfd);
        close_logger();
        return 1;
    }

    memset(&ack_packet, 0, sizeof(ack_packet));
    uint32_t final_ack_seq = client_isn + 1;
    uint32_t final_ack_ack = server_isn + 1;

    ack_packet.header.seq_num = htonl(final_ack_seq);
    ack_packet.header.ack_num = htonl(final_ack_ack);
    ack_packet.header.flags = ACK;

    log_event("SND ACK FOR SYN");
    sendto(sockfd, &ack_packet, sizeof(ack_packet.header),
           0, (struct sockaddr *)&server_addr, sizeof(server_addr));

    printf("Connection established.\n");

    /* --- Mode Selection --- */
    if (!is_chat_mode) {
        send_file(sockfd, &server_addr, input_file, output_file, final_ack_seq);
    } else {
        chat_mode(sockfd, &server_addr, sizeof(server_addr));
    }

    /* --- Cleanup --- */
    close(sockfd);
    close_logger();
    return 0;
}
