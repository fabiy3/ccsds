#include <stdio.h>   /* printf */
#include <string.h>  /* memcpy, NULL */
#include "ccsds.h"   /* our own types — note the quotes, not <> */

/*
 * BIT MANIPULATION PRIMER — read this before the code below.
 *
 * The CCSDS primary header packs multiple fields into 3 x 16-bit words:
 *
 * Word 0 (bytes 0–1):
 *   bits 15-13 : version    (3 bits)
 *   bit  12    : type       (1 bit)
 *   bit  11    : sec_hdr    (1 bit)
 *   bits 10-0  : APID       (11 bits)
 *
 * Word 1 (bytes 2–3):
 *   bits 15-14 : seq_flags  (2 bits)
 *   bits 13-0  : seq_count  (14 bits)
 *
 * Word 2 (bytes 4–5):
 *   bits 15-0  : data_length (16 bits)
 *
 * To extract a field:
 *   1. Shift right (>>) to move the field down to bit position 0
 *   2. AND (&) with a mask to zero out bits we don't want
 *
 * Example: APID is bits 10–0 of word0.
 *   word0 & 0x07FF
 *   0x07FF in binary = 0000 0111 1111 1111
 *   ANDing zeroes out the top 5 bits, leaving just the 11-bit APID.
 *
 * IMPORTANT: CCSDS is big-endian (most significant byte first).
 * Most desktop CPUs are little-endian. We reconstruct each 16-bit word
 * manually from two bytes to handle this correctly — no casting tricks.
 */

int ccsds_parse(const uint8_t *buf, size_t len, ccsds_packet_t *pkt)
{
    /*
     * Guard clauses — reject obviously bad inputs right away.
     * "const uint8_t *buf" means buf is a pointer to bytes we won't modify.
     * We check buf and pkt aren't NULL (a null pointer crash is the most
     * common C bug), and that we have at least 6 bytes to work with.
     */
    if (!buf || !pkt || len < CCSDS_PRIMARY_HDR_LEN)
        return -1;  /* -1 is the conventional C way to signal "error" */

    /*
     * Manually reconstruct the three 16-bit words from raw bytes.
     * buf[0] is the most significant byte, buf[1] is least significant.
     * We shift buf[0] left 8 bits and OR with buf[1] to combine them.
     *
     * Example: buf[0]=0x18, buf[1]=0x01
     *   (0x18 << 8) = 0x1800
     *   0x1800 | 0x01 = 0x1801
     */
    uint16_t word0 = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t word1 = ((uint16_t)buf[2] << 8) | buf[3];
    uint16_t word2 = ((uint16_t)buf[4] << 8) | buf[5];

    /* Extract each field using shifts and masks */
    pkt->version      = (word0 >> 13) & 0x07;   /* top 3 bits */
    pkt->type         = (word0 >> 12) & 0x01;   /* bit 12 */
    pkt->sec_hdr_flag = (word0 >> 11) & 0x01;   /* bit 11 */
    pkt->apid         = word0 & CCSDS_MAX_APID;  /* bottom 11 bits */

    pkt->seq_flags    = (word1 >> 14) & 0x03;   /* top 2 bits */
    pkt->seq_count    = word1 & 0x3FFF;          /* bottom 14 bits */

    pkt->data_length  = word2;                   /* full 16 bits */

    /*
     * Set the payload pointer to the byte immediately after the header.
     * This doesn't copy anything — it's just pointing into the same buffer.
     * This is safe because the buffer lives in main.c and stays alive
     * for the duration of our use here.
     */
    if (len > CCSDS_PRIMARY_HDR_LEN) {
        pkt->payload     = (uint8_t *)(buf + CCSDS_PRIMARY_HDR_LEN);
        pkt->payload_len = (uint16_t)(len - CCSDS_PRIMARY_HDR_LEN);
    } else {
        pkt->payload     = NULL;
        pkt->payload_len = 0;
    }

    /* Validate: CCSDS version must always be 0 */
    if (pkt->version != 0)
        return -1;

    return 0;  /* 0 = success in C convention */
}


/* ── Dispatch table ──────────────────────────────────────────────────
 *
 * A dispatch table is an array of function pointers. Instead of a giant
 * switch statement like:
 *
 *   switch (apid) {
 *     case 0x001: handle_attitude(pkt); break;
 *     case 0x002: handle_power(pkt); break;
 *     ...
 *   }
 *
 * We store a function pointer at each array index. dispatch_table[0x001]
 * holds the address of handle_attitude. Calling it is just:
 *   dispatch_table[apid](pkt);
 *
 * This scales cleanly — adding a new subsystem is one line, not a new
 * case in a switch. Real FSW codebases use this pattern heavily.
 */

/* Forward declarations of our handler functions */
static void handle_attitude(const ccsds_packet_t *pkt);
static void handle_power(const ccsds_packet_t *pkt);
static void handle_thermal(const ccsds_packet_t *pkt);
static void handle_unknown(const ccsds_packet_t *pkt);

/*
 * "typedef void (*apid_handler_t)(const ccsds_packet_t *);" means:
 * apid_handler_t is a type that represents a pointer to a function
 * that takes a const ccsds_packet_t* and returns void.
 */
typedef void (*apid_handler_t)(const ccsds_packet_t *);

/* The table itself — indexed by APID value */
static apid_handler_t dispatch_table[CCSDS_MAX_APID + 1];

void ccsds_dispatch_init(void)
{
    /* Fill the whole table with the unknown handler as a safe default */
    for (int i = 0; i <= CCSDS_MAX_APID; i++)
        dispatch_table[i] = handle_unknown;

    /* Then assign known subsystems */
    dispatch_table[APID_ATTITUDE_CONTROL] = handle_attitude;
    dispatch_table[APID_POWER_MGMT]       = handle_power;
    dispatch_table[APID_THERMAL]          = handle_thermal;
}

void ccsds_dispatch(const ccsds_packet_t *pkt)
{
    if (pkt->apid <= CCSDS_MAX_APID)
        dispatch_table[pkt->apid](pkt);
}

/* Handler implementations — in a real system these would decode
 * payload bytes into engineering units (temperatures, voltages, etc.) */
static void handle_attitude(const ccsds_packet_t *pkt)
{
    printf("[ADCS]    seq=%-5u  payload=%u bytes\n",
           pkt->seq_count, pkt->payload_len);
}

static void handle_power(const ccsds_packet_t *pkt)
{
    printf("[POWER]   seq=%-5u  payload=%u bytes\n",
           pkt->seq_count, pkt->payload_len);
}

static void handle_thermal(const ccsds_packet_t *pkt)
{
    printf("[THERMAL] seq=%-5u  payload=%u bytes\n",
           pkt->seq_count, pkt->payload_len);
}

static void handle_unknown(const ccsds_packet_t *pkt)
{
    printf("[??????]  apid=0x%03X  seq=%-5u\n",
           pkt->apid, pkt->seq_count);
}