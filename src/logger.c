#include <stdio.h>
#include <time.h>
#include "ccsds.h"

/*
 * We write a CSV log file so the output can be opened in Excel or
 * analyzed with Python/pandas — useful for showing in a portfolio demo.
 *
 * "static" on a variable at file scope means it persists across function
 * calls but is invisible to other files. log_file stays open for the
 * lifetime of the program.
 */
static FILE *log_file = NULL;

void logger_init(const char *filepath)
{
    log_file = fopen(filepath, "w");
    if (!log_file) {
        fprintf(stderr, "Warning: could not open log file %s\n", filepath);
        return;
    }
    /* Write the CSV header row */
    fprintf(log_file,
            "timestamp_ms,apid,seq_count,payload_len,anomaly\n");
    fflush(log_file);
}

void ccsds_log(const ccsds_packet_t *pkt, anomaly_flags_t flags)
{
    /*
     * Get a millisecond timestamp using clock_gettime.
     * CLOCK_MONOTONIC is a clock that only moves forward — it won't
     * jump if the system clock is adjusted, making it safe for logging.
     */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long ms = (long)(ts.tv_sec * 1000) + (long)(ts.tv_nsec / 1000000);

    /* Build a short anomaly string for the CSV column */
    const char *anomaly_str = "OK";
    if (flags & ANOMALY_SEQ_GAP)      anomaly_str = "SEQ_GAP";
    if (flags & ANOMALY_BAD_VERSION)  anomaly_str = "BAD_VERSION";
    if (flags & ANOMALY_UNKNOWN_APID) anomaly_str = "UNKNOWN_APID";

    /* Print to terminal */
    printf("[LOG] t=%-10ld  APID=0x%03X  seq=%-5u  "
           "len=%-4u  status=%s\n",
           ms, pkt->apid, pkt->seq_count,
           pkt->payload_len, anomaly_str);

    /* Write to CSV file if it's open */
    if (log_file) {
        fprintf(log_file, "%ld,0x%03X,%u,%u,%s\n",
                ms, pkt->apid, pkt->seq_count,
                pkt->payload_len, anomaly_str);
        fflush(log_file);  /* flush so data isn't lost if we Ctrl+C */
    }
}

void logger_close(void)
{
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}