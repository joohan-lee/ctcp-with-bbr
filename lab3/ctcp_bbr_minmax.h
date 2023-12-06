#ifndef MINMAX_H
#define MINMAX_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


/* Referece: https://elixir.bootlin.com/linux/latest/source/include/linux/win_minmax.h */

/* A single data point for our parameterized min-max tracker */
typedef struct minmax_sample {
	uint32_t	t;	/* time measurement was taken. Here, it is packet-timed rounds elapsed. */
	uint32_t	v;	/* value measured */
} ctcp_minmax_sample_t;

/* State for the parameterized min-max tracker */
typedef struct minmax {
	ctcp_minmax_sample_t* s;
	uint32_t max_idx;
    uint32_t window_len;
} ctcp_minmax_t;

/* Returns current max value(measurement) */
static inline uint32_t minmax_get(const struct minmax *m)
{
	// fprintf(stderr, "m->window_len: %u\n", m->window_len); // XXX
	// fprintf(stderr, "m->max_idx: %u\n", m->max_idx); // XXX
	// fprintf(stderr, "m->s[m->max_idx].v: %u\n", m->s[m->max_idx].v); // xxx
	return m->s[m->max_idx].v;
}

static inline uint32_t minmax_reset(struct minmax *m, uint32_t t, uint32_t meas, uint32_t window_len)
{
	struct minmax_sample val = { .t = t, .v = meas };

	fprintf(stderr, "In reset, window_len: %u\n", window_len); // XXX

	m->s = (ctcp_minmax_sample_t*)malloc(sizeof(ctcp_minmax_sample_t) * window_len);
	m->max_idx = 0;
	m->window_len = window_len;

	fprintf(stderr, "In reset, m->window_len: %u\n", m->window_len); // XXX

	// Init all the values in window.
    uint32_t i = 0;
    for(i=0; i < m->window_len; i++){
        m->s[i] = val;
    }
	return m->s[0].v;
}

static inline void minmax_destory(ctcp_minmax_t* m) {
    free(m->s);
}

uint32_t minmax_insert(struct minmax *m, uint32_t t, uint32_t meas);
// uint32_t minmax_running_min(struct minmax *m, uint32_t win, uint32_t t, uint32_t meas);

#endif