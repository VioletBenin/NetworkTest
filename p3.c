

#define MAX_SEQ 1	
typedef enum  {frame_arrival, cksum_err, timeout} event_type;
#include "protocol.h"

void sender3(void)
{
  seq_nr next_frame_to_send;	
  frame s;	/* scratch variable */
  packet buffer;	/* buffer for an outbound packet */
  event_type event;

  next_frame_to_send = 0;	
  from_network_layer(&buffer);	
  while (true) {
        s.info = buffer;	/* construct a frame for transmission */
        s.seq = next_frame_to_send;	/* insert sequence number in frame */
        to_physical_layer(&s);	/* send it on its way */
        start_timer(s.seq);	/* if answer takes too long, time out */
        wait_for_event(&event);	/* frame_arrival, cksum_err, timeout */
        if (event == frame_arrival) {
                from_physical_layer(&s);	/* get the acknowledgement */
                if (s.ack == next_frame_to_send) {
                        from_network_layer(&buffer);	/* get the next one to send */
                        inc(next_frame_to_send);	/* invert next_frame_to_send */
                }
        }
  }
}

void receiver3(void)
{
  seq_nr frame_expected;
  frame r, s;
  event_type event;

  frame_expected = 0;
  while (true) {
        wait_for_event(&event);
        if (event == frame_arrival) {
                from_physical_layer(&r);	
                if (r.seq == frame_expected) {
                        to_network_layer(&r.info);	/* pass the data to the network layer */
                        inc(frame_expected);	/* next time expect the other sequence nr */
                }
                s.ack = 1 - frame_expected;	
                to_physical_layer(&s);	
        }
  }
}
