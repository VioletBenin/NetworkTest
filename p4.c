

#define MAX_SEQ 1	
typedef enum {frame_arrival, cksum_err, timeout} event_type;
#include "protocol.h"

void protocol4 (void)
{
  seq_nr next_frame_to_send;	
  seq_nr frame_expected;	
  frame r, s;	
  packet buffer;	
  event_type event;

  next_frame_to_send = 0;	
  frame_expected = 0;	
  from_network_layer(&buffer);	
  s.info = buffer;	
  s.seq = next_frame_to_send;	
  s.ack = 1 - frame_expected;	
  to_physical_layer(&s);	
  start_timer(s.seq);	

  while (true) {
        wait_for_event(&event);	
        if (event == frame_arrival) { 
                from_physical_layer(&r);	/* go get it */

                if (r.seq == frame_expected) {
                        
                        to_network_layer(&r.info);	/* pass packet to network layer */
                        inc(frame_expected);	/* invert sequence number expected next */
                }

                if (r.ack == next_frame_to_send) { 
                        from_network_layer(&buffer);	/* fetch new packet from network layer */
                        inc(next_frame_to_send);	/* invert sender's sequence number */
                }
        }
             
        s.info = buffer;	
        s.seq = next_frame_to_send;	/* insert sequence number into it */
        s.ack = 1 - frame_expected;	/* seq number of last received frame */
        to_physical_layer(&s);	
        start_timer(s.seq);	
  }
}

