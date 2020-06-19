#define MAX_PKT 4	

typedef enum {false, true} boolean;	
typedef unsigned int seq_nr;	
typedef struct {unsigned char data[MAX_PKT];} packet;	/* packet definition */
typedef enum {data, ack, nak} frame_kind;	

typedef struct {	/* frames are transported in this layer */
  frame_kind kind;	
  seq_nr seq;   	/* sequence number */
  seq_nr ack;   	/* acknowledgement number */
  packet info;  	/* the network layer packet */
} frame;


void wait_for_event(event_type *event);

void from_network_layer(packet *p);

void to_network_layer(packet *p);

void from_physical_layer(frame *r);

void to_physical_layer(frame *s);

void start_timer(seq_nr k);

void stop_timer(seq_nr k);

void start_ack_timer(void);

void stop_ack_timer(void);

void enable_network_layer(void);

void disable_network_layer(void);

#define inc(k) if (k < MAX_SEQ) k = k + 1; else k = 0
