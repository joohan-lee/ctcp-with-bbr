#include "ctcp_bbr_minmax.h"

/* Check if new measurement updates the maximum and update max. 
* returns current max_idx after insert.
*/
void minmax_insert(struct minmax *m, uint32_t t, uint32_t meas)
{
	struct minmax_sample val = { .t = t, .v = meas };

    uint32_t rtt_idx = t % m->window_len;
    m->s[rtt_idx] = val;

    uint16_t i;
    for(i=0; i < m->window_len; i++){
        if((t - m->s[i].t) < m->window_len && m->s[m->max_idx].v < m->s[i].v){
            m->max_idx = i;
        }
    }

}
// uint32_t minmax_running_min(struct minmax *m, uint32_t win, uint32_t t, uint32_t meas);