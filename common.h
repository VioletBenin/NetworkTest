/* Data structures. */

typedef enum {frame_arrival, cksum_err, timeout, network_layer_ready, ack_timeout} event_type;
#include "protocol.h"
typedef unsigned long bigint;	/* bigint integer type available */

/* General constants */
#define TICK_SIZE (sizeof(tick))
#define DELTA 10		


#define OK      1		
#define NOTHING 2		

int protocol;			
bigint timeout_interval;	
int pkt_loss;			/* controls packet loss rate: 0 to 99 */
int garbled;			/* control cksum error rate: 0 to 99 */
int debug_flags;		/* debug flags */ 

/* File descriptors for pipes. */
int r1, w1, r2, w2, r3, w3, r4, w4, r5, w5, r6, w6;


int id;				

bigint zero;

int mrfd, mwfd, prfd;
