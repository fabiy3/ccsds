#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>   /* socket(), bind(), recvfrom() */
#include <arpa/inet.h>    /* struct sockaddr_in, htons() */
#include <unistd.h>       /* close() */
#include "ccsds.h"

#define LISTEN_PORT 7000

int main(void)
{
    /* Initialize all subsystems before starting the receive loop */
    anomaly_init();
    ccsds_dispatch_init();
    logger_init("telemetry.csv");

    /*
     * Create a UDP socket.
     * AF_INET = IPv4, SOCK_DGRAM = UDP (unreliable datagrams, like
     * how satellite telemetry actually works — no retry, just stream).
     * Returns a file descriptor (an integer handle), or -1 on error.
     */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");  /* perror prints the system error message */
        return 1;
    }

    /*
     * struct sockaddr_in describes an IPv4 address + port.
     * We zero it out first (memset), then fill in each field.
     * htons() converts the port from host byte order to network byte
     * order — the same big-endian vs little-endian issue as CCSDS.
     * INADDR_ANY means "listen on all network interfaces".
     */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(LISTEN_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    printf("CCSDS parser listening on UDP port %d\n", LISTEN_PORT);
    printf("%-12s %-8s %-8s %-6s %s\n",
           "timestamp", "APID", "seq", "len", "status");
    printf("────────────────────────────────────────────\n");

    /*
     * The main receive loop.
     * recvfrom() blocks until a UDP packet arrives, then copies the
     * bytes into buf and returns how many bytes were received.
     * We loop forever until the user hits Ctrl+C.
     */
    uint8_t buf[CCSDS_MAX_PACKET_LEN];
    while (1) {
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, NULL, NULL);
        if (n < 0) {
            perror("recvfrom");
            break;
        }

        /*
         * Declare a ccsds_packet_t on the stack (no malloc needed).
         * Pass its address to ccsds_parse, which fills it in.
         * If parsing fails, skip this packet and wait for the next one.
         */
        ccsds_packet_t pkt;
        if (ccsds_parse(buf, (size_t)n, &pkt) != 0) {
            printf("[WARN] Malformed packet (%zd bytes) — discarded\n", n);
            continue;
        }

        /* Check for sequence gaps, then log and dispatch */
        anomaly_flags_t flags = anomaly_check(pkt.apid, pkt.seq_count);
        ccsds_log(&pkt, flags);
        ccsds_dispatch(&pkt);
    }

    logger_close();
    close(sock);
    return 0;
}