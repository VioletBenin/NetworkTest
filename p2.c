

typedef enum {frame_arrival} event_type;
#include "protocol.h"

void sender2(void)
{
  frame s;	
  packet buffer;	
  event_type event;	

  while (true) {
        from_network_layer(&buffer);	/* go get something to send */
        s.info = buffer;	/* copy it into s for transmission */
        to_physical_layer(&s);	/* bye bye little frame */
        wait_for_event(&event);	
  }
}

void receiver2(void)
{
  frame r, s;	/* buffers for frames */
  event_type event;	/* frame_arrival is the only possibility */
  while (true) {
        wait_for_event(&event);	/* only possibility is frame_arrival */
        from_physical_layer(&r);	/* go get the inbound frame */
        to_network_layer(&r.info);	/* pass the data to the network layer */
        to_physical_layer(&s);	
  }
}

