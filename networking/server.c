#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <openssl/md5.h>
#include <errno.h>   // For chat_mode select error
#include "sham.h"

#define SERVER_BUFFER_SIZE 51200

// ------------------ Forward Declarations ------------------
void calculate_and_print_md5(const char *filename);
void chat_mode(int sockfd, struct sockaddr_in *peer_addr, socklen_t peer_len);
void receive_file(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, float loss_rate, uint32_t client_isn);


// ==========================================================
//                   File Reception Function
// ==========================================================
void receive_file(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len,
                  float loss_rate, uint32_t client_isn) {
    FILE *output_file = NULL;
    char output_filename[256];
    struct sham_packet packet;

    uint32_t expected_seq_num = client_isn;
    uint32_t server_fin_seq   = rand() % 70000;

    // --------- Receive file name first ---------
    while (1) {
        ssize_t bytes_received = recvfrom(sockfd, &packet, sizeof(packet), 0,
                                          (struct sockaddr *)client_addr, &client_len);
        if (bytes_received < 0) continue;

        if (ntohl(packet.header.seq_num) == expected_seq_num) {
            strncpy(output_filename, packet.data, sizeof(output_filename) - 1);
            output_filename[sizeof(output_filename) - 1] = '\0';

            log_event("RCV DATA SEQ=%u (Filename)", expected_seq_num);

            output_file = fopen(output_filename, "wb");
            if (!output_file) {
                perror("Failed to open output file");
                return;
            }

            printf("Receiving file and saving as: %s\n", output_filename);

            expected_seq_num += strlen(packet.data) + 1;

            struct sham_packet ack_packet;
            memset(&ack_packet, 0, sizeof(ack_packet));
            ack_packet.header.flags       = ACK;
            ack_packet.header.ack_num     = htonl(expected_seq_num);
            ack_packet.header.window_size = htons(SERVER_BUFFER_SIZE);

            log_event("SND ACK=%u WIN=%u", expected_seq_num, SERVER_BUFFER_SIZE);
            sendto(sockfd, &ack_packet, sizeof(ack_packet.header), 0,
                   (const struct sockaddr *)client_addr, client_len);

            break;
        }
    }

    // --------- Receive actual file content ---------
    while (1) {
        ssize_t bytes_received = recvfrom(sockfd, &packet, sizeof(packet), 0,
                                          (struct sockaddr *)client_addr, &client_len);
        if (bytes_received < 0) continue;

        if (should_drop(loss_rate)) {
            log_event("DROP DATA SEQ=%u", ntohl(packet.header.seq_num));
            continue;
        }

        struct sham_packet ack_packet;
        memset(&ack_packet, 0, sizeof(ack_packet));
        ack_packet.header.flags       = ACK;
        ack_packet.header.window_size = htons(SERVER_BUFFER_SIZE);

        if (packet.header.flags & FIN) {
            log_event("RCV FIN SEQ=%u", ntohl(packet.header.seq_num));

            ack_packet.header.ack_num = htonl(ntohl(packet.header.seq_num) + 1);
            log_event("SND ACK FOR FIN WIN=%u", SERVER_BUFFER_SIZE);

            sendto(sockfd, &ack_packet, sizeof(ack_packet.header), 0,
                   (const struct sockaddr *)client_addr, client_len);

            struct sham_packet fin_packet;
            memset(&fin_packet, 0, sizeof(fin_packet));
            fin_packet.header.flags   = FIN;
            fin_packet.header.seq_num = htonl(server_fin_seq);

            log_event("SND FIN SEQ=%u", server_fin_seq);
            sendto(sockfd, &fin_packet, sizeof(fin_packet.header), 0,
                   (const struct sockaddr *)client_addr, client_len);

            recvfrom(sockfd, &packet, sizeof(packet), 0,
                     (struct sockaddr *)client_addr, &client_len);

            if ((packet.header.flags & ACK) &&
                ntohl(packet.header.ack_num) == server_fin_seq + 1) {
                log_event("RCV ACK FOR FIN");
            }
            break;
        }

        uint32_t received_seq_num = ntohl(packet.header.seq_num);
        uint16_t data_len         = bytes_received - sizeof(struct sham_header);

        log_event("RCV DATA SEQ=%u LEN=%u", received_seq_num, data_len);

        if (received_seq_num == expected_seq_num) {
            fwrite(packet.data, 1, data_len, output_file);
            expected_seq_num += data_len;
        }

        ack_packet.header.ack_num = htonl(expected_seq_num);
        log_event("SND ACK=%u WIN=%u", expected_seq_num, SERVER_BUFFER_SIZE);

        sendto(sockfd, &ack_packet, sizeof(ack_packet.header), 0,
               (const struct sockaddr *)client_addr, client_len);
    }

    printf("File reception complete.\n");
    if (output_file) fclose(output_file);

    calculate_and_print_md5(output_filename);
}


// ==========================================================
//                 MD5 Calculation Function
// ==========================================================
void calculate_and_print_md5(const char *filename) {
    unsigned char c[MD5_DIGEST_LENGTH];
    unsigned char data[1024];

    FILE *inFile = fopen(filename, "rb");
    if (inFile == NULL) {
        printf("MD5 Error: Could not open file %s\n", filename);
        return;
    }

    MD5_CTX mdContext;
    int bytes;

    MD5_Init(&mdContext);

    while ((bytes = fread(data, 1, 1024, inFile)) != 0) {
        MD5_Update(&mdContext, data, bytes);
    }

    MD5_Final(c, &mdContext);

    printf("MD5: ");
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        printf("%02x", c[i]);
    }
    printf("\n");

    fclose(inFile);
}


// ==========================================================
//                  Chat Mode Function
// ==========================================================
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

        int max_fd  = (STDIN_FILENO > sockfd) ? STDIN_FILENO : sockfd;
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
        }

        // Handle user input
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(send_buffer, sizeof(send_buffer), stdin)) {
                send_buffer[strcspn(send_buffer, "\n")] = 0;

                if (strcmp(send_buffer, "/quit") == 0) {
                    printf("Initiating shutdown...\n");

                    struct sham_packet fin_packet;
                    memset(&fin_packet, 0, sizeof(fin_packet));
                    fin_packet.header.flags   = FIN;
                    fin_packet.header.seq_num = htonl(my_seq);

                    log_event("SND FIN SEQ=%u", my_seq);

                    sendto(sockfd, &fin_packet, sizeof(fin_packet.header), 0,
                           (struct sockaddr*)peer_addr, peer_len);
                    break;
                }

                struct sham_packet data_packet;
                memset(&data_packet, 0, sizeof(data_packet));
                data_packet.header.seq_num = htonl(my_seq++);
                strncpy(data_packet.data, send_buffer, PAYLOAD_SIZE);

                log_event("SND DATA SEQ=%u (Chat)", ntohl(data_packet.header.seq_num));

                sendto(sockfd,
                       &data_packet,
                       sizeof(struct sham_header) + strlen(send_buffer) + 1,
                       0,
                       (struct sockaddr*)peer_addr,
                       peer_len);
            }
        }

        // Handle received messages
        if (FD_ISSET(sockfd, &read_fds)) {
            ssize_t bytes = recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0,
                                     (struct sockaddr*)peer_addr, &peer_len);
            if (bytes > 0) {
                if (recv_packet.header.flags & FIN) {
                        log_event("RCV FIN SEQ=%u", ntohl(recv_packet.header.seq_num));
                        printf("Peer has disconnected. Closing chat.\n");

                        // 1. Acknowledge the client's FIN
                        struct sham_packet ack_packet;
                        memset(&ack_packet, 0, sizeof(ack_packet));
                        ack_packet.header.flags = ACK;
                        ack_packet.header.ack_num = htonl(ntohl(recv_packet.header.seq_num) + 1);
                        log_event("SND ACK FOR FIN");
                        sendto(sockfd, &ack_packet, sizeof(ack_packet.header), 0, (const struct sockaddr*)peer_addr, peer_len);

                        // 2. Send our own FIN
                        struct sham_packet fin_packet;
                        memset(&fin_packet, 0, sizeof(fin_packet));
                        fin_packet.header.flags = FIN;
                        fin_packet.header.seq_num = htonl(rand() % 10000); // Use a random seq num for this FIN
                        log_event("SND FIN SEQ=%u", ntohl(fin_packet.header.seq_num));
                        sendto(sockfd, &fin_packet, sizeof(fin_packet.header), 0, (const struct sockaddr*)peer_addr, peer_len);
                        
                        // 3. Wait for the final ACK (socket timeout will handle hangs)
                        recvfrom(sockfd, &recv_packet, sizeof(recv_packet), 0, (struct sockaddr*)peer_addr, &peer_len);
                        if(recv_packet.header.flags & ACK){
                            log_event("RCV ACK FOR FIN");
                        }
                        break; // Exit chat loop
                    }

                log_event("RCV DATA SEQ=%u (Chat)", ntohl(recv_packet.header.seq_num));
                printf("Peer: %s\n", recv_packet.data);
            }
        }
    }
}


// ==========================================================
//                       Main Function
// ==========================================================
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: ./server <port> [--chat] [loss_rate]\n");
        return 1;
    }

    int port         = atoi(argv[1]);
    int is_chat_mode = (argc > 2 && strcmp(argv[2], "--chat") == 0);

    float loss_rate = 0.0;
    if (is_chat_mode && argc > 3) loss_rate = atof(argv[3]);
    if (!is_chat_mode && argc > 2) loss_rate = atof(argv[2]);

    init_logger("server_log.txt");
    srand(time(NULL));

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(port);

    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    while (1) {
        printf("Waiting for a client to connect...\n");

        struct sham_packet received_packet;
        recvfrom(sockfd, &received_packet, sizeof(received_packet), 0,
                 (struct sockaddr *)&client_addr, &client_len);

        if (received_packet.header.flags & SYN) {
            uint32_t client_isn = ntohl(received_packet.header.seq_num);
            log_event("RCV SYN SEQ=%u", client_isn);

            struct sham_packet syn_ack_packet;
            memset(&syn_ack_packet, 0, sizeof(syn_ack_packet));
            uint32_t server_isn = rand() % 50000;

            syn_ack_packet.header.seq_num = htonl(server_isn);
            syn_ack_packet.header.ack_num = htonl(client_isn + 1);
            syn_ack_packet.header.flags   = SYN | ACK;

            log_event("SND SYN-ACK SEQ=%u ACK=%u", server_isn, client_isn + 1);

            sendto(sockfd, &syn_ack_packet, sizeof(syn_ack_packet.header), 0,
                   (const struct sockaddr *)&client_addr, client_len);

            struct sham_packet final_ack_packet;
            recvfrom(sockfd, &final_ack_packet, sizeof(final_ack_packet), 0,
                     (struct sockaddr *)&client_addr, &client_len);

            // --- FIX: Correct if/else structure ---
            if ((final_ack_packet.header.flags & ACK) &&
                (ntohl(final_ack_packet.header.ack_num) == server_isn + 1)) {
                log_event("RCV ACK FOR SYN");
                printf("Connection established with a client.\n");

                if (!is_chat_mode) {
                    uint32_t client_starting_seq = ntohl(final_ack_packet.header.seq_num);
                    receive_file(sockfd, &client_addr, client_len, loss_rate, client_starting_seq);
                } else {
                    chat_mode(sockfd, &client_addr, client_len);
                }
            } else {
                fprintf(stderr, "Handshake failed: Did not receive final ACK.\n");
            }
        }
    }

    close(sockfd);
    close_logger();
    return 0;
}
