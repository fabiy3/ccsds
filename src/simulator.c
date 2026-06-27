#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "ccsds.h"

#define TARGET_IP   "127.0.0.1"
#define TARGET_PORT 7000
#define PACKETS_PER_SEC 5

/*
 * Build a CCSDS packet into buf.
 * This is the reverse of parsing — we're packing fields into bytes.
 * Returns the total number of bytes written.
 */
static int build_packet(uint8_t *buf, uint16_t apid,
                        uint16_t seq, const uint8_t *payload,
                        uint16_t payload_len)
{
    /*
     * Construct word0: version=0, type=0 (TM), sec_hdr_flag=1, apid
     * We OR the pieces together after shifting each into position.
     */
    uint16_t word0 = (uint16_t)((0 << 13) |  /* version = 0 */
                                (0 << 12) |  /* type = TM */
                                (1 << 11) |  /* sec_hdr_flag = 1 */
                                (apid & CCSDS_MAX_APID));

    uint16_t word1 = (uint16_t)((0x3 << 14) |        /* seq_flags = standalone */
                                (seq & 0x3FFF));      /* seq_count */

    uint16_t word2 = (uint16_t)(CCSDS_PRIMARY_HDR_LEN + payload_len - 1);

    /*
     * Write each word in big-endian order (most significant byte first).
     * This is the reverse of how we read them in parser.c.
     */
    buf[0] = (uint8_t)(word0 >> 8);
    buf[1] = (uint8_t)(word0 & 0xFF);
    buf[2] = (uint8_t)(word1 >> 8);
    buf[3] = (uint8_t)(word1 & 0xFF);
    buf[4] = (uint8_t)(word2 >> 8);
    buf[5] = (uint8_t)(word2 & 0xFF);

    /* Copy payload bytes after the header */
    memcpy(buf + CCSDS_PRIMARY_HDR_LEN, payload, payload_len);

    return CCSDS_PRIMARY_HDR_LEN + payload_len;
}

int main(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(TARGET_PORT);
    inet_pton(AF_INET, TARGET_IP, &dest.sin_addr);

    /* Three subsystems cycling through, each with their own seq counter */
    uint16_t apids[]    = { APID_ATTITUDE_CONTROL,
                            APID_POWER_MGMT,
                            APID_THERMAL };
    uint16_t seq_ctrs[] = { 0, 0, 0 };
    int      num_apids  = 3;

    /* Dummy payload — 8 bytes of fake sensor data */
    uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF,
                          0xCA, 0xFE, 0xBA, 0xBE };
    uint8_t buf[256];

    printf("Simulator running → sending to %s:%d at %d pkt/s\n",
           TARGET_IP, TARGET_PORT, PACKETS_PER_SEC);
    printf("Every 15 packets a deliberate sequence gap will be injected.\n\n");

    for (int i = 0; ; i++) {
        int idx       = i % num_apids;
        uint16_t apid = apids[idx];

        /*
         * Every 15 packets on the attitude control subsystem,
         * skip a sequence number to simulate a dropped packet.
         * This lets you see the anomaly detector fire in real time.
         */
        if (apid == APID_ATTITUDE_CONTROL && i > 0 && (i % 15) == 0) {
            seq_ctrs[idx]++;
            printf("[SIM] Injecting gap on APID 0x%03X "
                   "(skipping seq %u)\n", apid, seq_ctrs[idx] - 1);
        }

        int len = build_packet(buf, apid, seq_ctrs[idx]++,
                               payload, sizeof(payload));

        sendto(sock, buf, (size_t)len, 0,
               (struct sockaddr *)&dest, sizeof(dest));

        /* 1,000,000 microseconds = 1 second. Divide for pkt/s rate */
        usleep(1000000 / PACKETS_PER_SEC);
    }

    close(sock);
    return 0;
}