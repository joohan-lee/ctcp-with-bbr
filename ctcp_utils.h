/******************************************************************************
 * ctcp_utils.h
 * ------------
 * Contains definitions for helper functions that you might find useful.
 * Implementations can be found in ctcp_utils.c.
 *
 *****************************************************************************/

#ifndef CTCP_UTILS_H
#define CTCP_UTILS_H

#include "ctcp_sys.h"

/**
 * Computes a checksum over the given data and returns the result in
 * NETWORK-byte order.
 *
 * _data: Data to compute checksum over.
 * len: Length of data.
 *
 * returns: The checksum in network-byte order.
 */
uint16_t cksum(const void *_data, uint16_t len);

/* 
 * Every time your code runs, it should output a log file that contains the timestamp and BDP. The file name should be 
 * "bdp.txt" and the format of each line is "timestamp BDP". When your ctcp sends a packet, it should append a new line 
 * to the file with current timestamp (get it from current_time() in ctcp_utils.h). You only need to log BDP for 
 * sending so you don’t need to log for re-sending. The BDP should be **expressed in units of bits. This will help us 
 * generate graphs to test if your implementation is working correctly. Here is what your output should look like. The 
 * BDP and the timestamp should be separated by commas.
 * 1508350555362,95820
 * 1508350555389,96181
 * 1508350555408,96374
*/
static inline void _ctcp_bbr_log_data(long timestamp, uint64_t bdp){
  
  FILE *file;

  file = fopen("bdp.txt", "a");

  // Check if the file is opened successfully
  if (file == NULL) {
      perror("Error opening the file");
      return;
  }

  // Write data to the file with formatting
  fprintf(file, "%lu,%lu\n", timestamp, bdp);
  // fprintf(file, "%lu,%lu,%s\n", timestamp, bdp, bbr_mode); // If you want to know bbr_mode, add parameter with convert_bbr_mode_to_str function.

  fclose(file);

}

/**
 * Gets the current time in milliseconds.
 */
long current_time();

/**
 * Gets the current time in usec(microseconds).
 */
int64_t monotonic_current_time_us();

/**
 * Returns the number of microseconds(usec) until the next timeout.
 *
 * last: The previous timeout.
 * interval: The timeout interval.
 */
int64_t utils_need_timer_in_us(const struct timespec*, int64_t );

/**
 * Prints out the headers of a cTCP segment. Expects the segment to come in
 * network-byte order. All fields are converted and printed out in host order,
 * except for the checksum.
 *
 * segment: The cTCP segment, in network-byte order.
 */
void print_hdr_ctcp(ctcp_segment_t *segment);

#endif /* CTCP_UTILS_H */
