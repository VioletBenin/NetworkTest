

typedef enum {frame_arrival} event_type;
#include "protocol.h"

void sender1(void)
{
  frame s;	
  packet buffer;	/* buffer for an outbound packet */

  while (true) {
        from_network_layer(&buffer);	/* go get something to send */
        s.info = buffer;	/* copy it into s for transmission */
        to_physical_layer(&s);	/* send it on its way */
  }	
}

void receiver1(void)
{
  frame r;
  event_type event;	

  while (true) {
        wait_for_event(&event);	
        from_physical_layer(&r);	/* go get the inbound frame */
        to_network_layer(&r.info);	/* pass the data to the network layer */
  }
}
