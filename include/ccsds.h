#ifndef CCSDS_H
#define CCSDS_H
#include <stdint.h>
#include <stddef.h>

/* ── Constants ─────────────────────────────────────────────────────── */

#define CCSDS_PRIMARY_HDR_LEN 6
/*
 * Every CCSDS packet starts with exactly 6 bytes of header. Always.
 * This is defined in the standard and never changes.
 */

#define CCSDS_MAX_PACKET_LEN 65536
/* Maximum UDP buffer size we'll allocate — 64KB is more than enough */

#define CCSDS_MAX_APID 0x07FF
/*
 * The APID field is 11 bits wide. The maximum value of an 11-bit number
 * is 2047, which is 0x7FF in hex. We'll use this as an array size and
 * as a bitmask to extract the APID from a raw header word.
 */

#define CCSDS_SEQ_WRAP 16384
/*
 * The sequence counter is 14 bits wide, so it counts 0 → 16383 then
 * wraps back to 0. When checking for gaps, we need to account for this
 * wrap-around. 16384 = 2^14.
 */

/* ── APID definitions ──────────────────────────────────────────────── */
/*
 * An enum (enumeration) is just a named list of integer constants.
 * We use it here to give human-readable names to APID numbers.
 * APID_ATTITUDE_CONTROL = 0x001 means "subsystem 1 is attitude control".
 * You decide what each APID means for your simulation.
 */
typedef enum
{
    APID_ATTITUDE_CONTROL = 0x001,
    APID_POWER_MGMT = 0x002,
    APID_THERMAL = 0x003,
    APID_COMMS = 0x004,
    APID_PAYLOAD = 0x005,
    APID_UNKNOWN = 0x7FF
} apid_t;

/* ── The parsed packet struct ──────────────────────────────────────── */
/*
 * A struct is a custom data type that groups related fields together.
 * This is what a CCSDS packet looks like AFTER we've parsed it out of
 * raw bytes into human-readable fields. The "typedef" lets us write
 * "ccsds_packet_t" instead of "struct ccsds_packet" everywhere.
 */
typedef struct
{
    uint16_t version;     /* always 0 — if not, packet is malformed  */
    uint8_t type;         /* 0 = telemetry downlink, 1 = command uplink */
    uint8_t sec_hdr_flag; /* 1 if a secondary header follows the primary */
    uint16_t apid;        /* which subsystem sent this packet */
    uint8_t seq_flags;    /* 0x3 means standalone packet (most common) */
    uint16_t seq_count;   /* 0–16383, increments per APID per packet */
    uint16_t data_length; /* total packet byte length minus 1 */
    uint8_t *payload;     /* pointer to where the payload bytes start */
    uint16_t payload_len; /* how many payload bytes there are */
} ccsds_packet_t;

/* ── Anomaly flags ─────────────────────────────────────────────────── */
/*
 * We use bit flags here so multiple anomalies can be set at once.
 * "1 << 0" = binary 0001, "1 << 1" = binary 0010, etc.
 * You can combine them with bitwise OR: ANOMALY_SEQ_GAP | ANOMALY_BAD_VERSION
 * and check individual ones with bitwise AND.
 */
typedef enum
{
    ANOMALY_NONE = 0,
    ANOMALY_SEQ_GAP = 1 << 0,
    ANOMALY_BAD_VERSION = 1 << 1,
    ANOMALY_ZERO_LENGTH = 1 << 2,
    ANOMALY_UNKNOWN_APID = 1 << 3
} anomaly_flags_t;

/* ── Function declarations ─────────────────────────────────────────── */
/*
 * These tell the compiler "these functions exist somewhere — trust me".
 * The actual implementations live in .c files. This is called a
 * "forward declaration" and it's how C connects files to each other.
 */
int ccsds_parse(const uint8_t *buf, size_t len,
                ccsds_packet_t *pkt);
anomaly_flags_t anomaly_check(uint16_t apid, uint16_t seq_count);
void anomaly_init(void);
void ccsds_dispatch_init(void);
void ccsds_dispatch(const ccsds_packet_t *pkt);
void ccsds_log(const ccsds_packet_t *pkt,
               anomaly_flags_t flags);
void logger_close(void);
void logger_init(const char *filename);

#endif /* CCSDS_H */