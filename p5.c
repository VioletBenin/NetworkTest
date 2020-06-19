
#define MAX_SEQ 7	
typedef enum {frame_arrival, cksum_err, timeout, network_layer_ready} event_type;
#include "protocol.h"

static boolean between(seq_nr a, seq_nr b, seq_nr c)
{
  if (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
        return(true);
    else
        return(false);
}

static void send_data(seq_nr frame_nr, seq_nr frame_expected, packet buffer[])
{

  frame s;	

  s.info = buffer[frame_nr];	
  s.seq = frame_nr;	
  s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);	
  to_physical_layer(&s);	/* transmit the frame */
  start_timer(frame_nr);	/* start the timer running */
}

void protocol5(void)
{
  seq_nr next_frame_to_send;	
  seq_nr ack_expected;	
  seq_nr frame_expected;	
  frame r;	/* scratch variable */
  packet buffer[MAX_SEQ+1];	
  seq_nr nbuffered;	
  seq_nr i;	/* used to index into the buffer array */
  event_type event;

  enable_network_layer();	
  ack_expected = 0;
  next_frame_to_send = 0;	
  frame_expected = 0;	
  nbuffered = 0;	

  while (true) {
     wait_for_event(&event);	

     switch(event) { 
        case network_layer_ready:	                
                from_network_layer(&buffer[next_frame_to_send]); /* fetch new packet */
                nbuffered = nbuffered + 1;	/* expand the sender's window */
                send_data(next_frame_to_send, frame_expected, buffer);	/* transmit the frame */
                inc(next_frame_to_send);	
                break;

        case frame_arrival:	
                from_physical_layer(&r);	
  
                if (r.seq == frame_expected) {
                        to_network_layer(&r.info);	/* pass packet to network layer */
                        inc(frame_expected);	
                 }  
                 
                while (between(ack_expected, r.ack, next_frame_to_send)) {
                        /* Handle piggybacked ack. */
                        nbuffered = nbuffered - 1;	
                        stop_timer(ack_expected);	
                        inc(ack_expected);	
                }                
                break;

        case cksum_err: ;	
                break;
  
        case timeout:	
                next_frame_to_send = ack_expected;	/* start retransmitting here */
                for (i = 1; i <= nbuffered; i++) {
                        send_data(next_frame_to_send, frame_expected, buffer);	
                        inc(next_frame_to_send);	
                }
     }
  
     if (nbuffered < MAX_SEQ)
        enable_network_layer(); 
     else
        disable_network_layer();
  }
}
