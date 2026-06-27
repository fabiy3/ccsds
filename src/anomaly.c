#include <stdio.h>
#include "ccsds.h"

/*
 * We track the last seen sequence count for every possible APID.
 * Using -1 as a sentinel value meaning "never seen this APID before".
 *
 * "static" here means this array is private to this file — no other
 * .c file can access it directly. That's good practice: hide internal
 * state, expose only the functions that operate on it.
 */
static int seq_tracker[CCSDS_MAX_APID + 1];

void anomaly_init(void)
{
    for (int i = 0; i <= CCSDS_MAX_APID; i++)
        seq_tracker[i] = -1;
}

anomaly_flags_t anomaly_check(uint16_t apid, uint16_t seq_count)
{
    if (apid > CCSDS_MAX_APID)
        return ANOMALY_UNKNOWN_APID;

    int prev = seq_tracker[apid];
    seq_tracker[apid] = (int)seq_count;  /* update tracker */

    /* First time we've seen this APID — nothing to compare against */
    if (prev == -1)
        return ANOMALY_NONE;

    /*
     * Expected next count is (prev + 1) mod 16384.
     * The modulo handles the wrap-around from 16383 back to 0.
     * Cast to uint16_t so the arithmetic stays in the right range.
     */
    uint16_t expected = (uint16_t)((prev + 1) % CCSDS_SEQ_WRAP);

    if (seq_count != expected) {
        /*
         * Calculate the gap size, again accounting for wrap-around.
         * Adding CCSDS_SEQ_WRAP before modulo handles the case where
         * seq_count < prev (i.e. the counter wrapped).
         */
        int gap = ((int)seq_count - prev + CCSDS_SEQ_WRAP) % CCSDS_SEQ_WRAP;
        printf("[ANOMALY] APID 0x%03X — dropped %d packet(s) "
               "(expected seq %u, got %u)\n",
               apid, gap - 1, expected, seq_count);
        return ANOMALY_SEQ_GAP;
    }

    return ANOMALY_NONE;
}