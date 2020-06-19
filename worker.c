#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "common.h"

#define NR_TIMERS 8		
#define MAX_QUEUE 100000	
#define NO_EVENT -1		
#define FRAME_SIZE (sizeof(frame))
#define BYTE 0377		
#define UINT_MAX  0xFFFFFFFF	
#define INTERVAL 100000		
#define AUX 2			


#define SENDS        0x0001	
#define RECEIVES     0x0002	
#define TIMEOUTS     0x0004	
#define PERIODIC     0x0008	


bigint ack_timer[NR_TIMERS];	
unsigned int seqs[NR_TIMERS];	
bigint lowest_timer;		
bigint aux_timer;		
int network_layer_status;	
unsigned int next_net_pkt;	
unsigned int last_pkt_given= 0xFFFFFFFF;	
frame last_frame;		
int offset;			
bigint tick;			
int retransmitting;		
int nseqs = -1;			
extern unsigned int oldest_frame;	

char *badgood[] = {"bad ", "good"};
char *tag[] = {"Data", "Ack ", "Nak "};


int data_sent;			
int data_retransmitted;		
int data_lost;			
int data_not_lost;		
int good_data_recd;		
int cksum_data_recd;		

int acks_sent;			
int acks_lost;			
int acks_not_lost;		
int good_acks_recd;		
int cksum_acks_recd;		

int payloads_accepted;		
int timeouts;			
int ack_timeouts;		


frame queue[MAX_QUEUE];		
frame *inp = &queue[0];		
frame *outp = &queue[0];	
int nframes;			

void wait_for_event(event_type *event);
void queue_frames(void);
int pick_event(void);
event_type frametype(void);
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
int check_timers(void);
int check_ack_timer(void);
unsigned int pktnum(packet *p);
void fr(frame *f);
void recalc_timers(void);
void print_statistics(void);
void sim_error(char *s);


void wait_for_event(event_type *event)
{
 bigint ct, word = OK;

  if (nseqs < 0) nseqs = oldest_frame;	
  offset = 0;			
  retransmitting = 0;		
  while (true) {
	queue_frames();		
	if (write(mwfd, &word, TICK_SIZE) != TICK_SIZE) print_statistics();
	if (read(mrfd, &ct, TICK_SIZE) != TICK_SIZE) print_statistics();
	if (ct == 0) print_statistics();
	tick = ct;		
	if ((debug_flags & PERIODIC) && (tick%INTERVAL == 0))
		printf("Tick %u. Proc %d. Data sent=%d  Payloads accepted=%d  Timeouts=%d\n", tick/DELTA, id, data_sent, payloads_accepted, timeouts);


	*event = pick_event();
	if (*event == NO_EVENT) {
		word = (lowest_timer == 0 ? NOTHING : OK);
		continue;
	}
	word = OK;
	if (*event == timeout) {
		timeouts++;
		retransmitting = 1;	
		if (debug_flags & TIMEOUTS)
		      printf("Tick %u. Proc %d got timeout for frame %d\n",
					       tick/DELTA, id, oldest_frame);
	}

	if (*event == ack_timeout) {
		ack_timeouts++;
		if (debug_flags & TIMEOUTS)
		      printf("Tick %u. Proc %d got ack timeout\n",
					       tick/DELTA, id);
	}
	return;
  }
}


void queue_frames(void)
{

  int prfd, frct, k;
  frame *top;
  struct stat statbuf;

  prfd = (id == 0 ? r2 : r1);	

  if (fstat(prfd, &statbuf) < 0) sim_error("Cannot fstat peer pipe");
  frct = statbuf.st_size/FRAME_SIZE;	

  if (nframes + frct >= MAX_QUEUE)	
	sim_error("Out of queue space. Increase MAX_QUEUE and re-make.");  

  
  if (frct > 0) {
	
	top = (outp <= inp ? &queue[MAX_QUEUE] : outp);
	k = top - inp;	
	if (k > frct) k = frct;	
	if (read(prfd, inp, k * FRAME_SIZE) != k * FRAME_SIZE)
		sim_error("Error reading frames from peer");
	frct -= k;		
	inp += k;
	if (inp == &queue[MAX_QUEUE]) inp = queue;
	nframes += k;


	if (frct > 0) {
		if (read(prfd, queue, frct * FRAME_SIZE) != frct*FRAME_SIZE)
			sim_error("Error 2 reading frames from peer");
		nframes += frct;
		inp = &queue[frct];
	}
  }
}


int pick_event(void)
{

  switch(protocol) {
    case 2:			
	if (nframes == 0 && lowest_timer == 0) return(NO_EVENT);
	return(frametype());

    case 3:			
    case 4:
	if (nframes > 0) return((int)frametype());
	if (check_timers() >= 0) return(timeout);	
	return(NO_EVENT);

    case 5:	
	if (nframes > 0) return((int)frametype());
	if (network_layer_status) return(network_layer_ready);
	if (check_timers() >= 0) return(timeout);	
	return(NO_EVENT);

    case 6:	
	if (check_ack_timer() > 0) return(ack_timeout);
	if (nframes > 0) return((int)frametype());
	if (network_layer_status) return(network_layer_ready);
	if (check_timers() >= 0) return(timeout);	
	return(NO_EVENT);
  }
}


event_type frametype(void)
{


  int n, i;
  event_type event;

  
  last_frame = *outp;		
  outp++;
  if (outp == &queue[MAX_QUEUE]) outp = queue;
  nframes--;

  
  n = rand() & 01777;
  if (n < garbled) {
	
	event = cksum_err;
	if (last_frame.kind == data) cksum_data_recd++;
	if (last_frame.kind == ack) cksum_acks_recd++;
	i = 0;
  } else {
	event = frame_arrival;
	if (last_frame.kind == data) good_data_recd++;
	if (last_frame.kind == ack) good_acks_recd++;
	i = 1;
  }

  if (debug_flags & RECEIVES) {
	printf("Tick %u. Proc %d got %s frame:  ",
						tick/DELTA,id,badgood[i]);
	fr(&last_frame);
  }
  return(event);
}


void from_network_layer(packet *p)
{


  p->data[0] = (next_net_pkt >> 24) & BYTE;
  p->data[1] = (next_net_pkt >> 16) & BYTE;
  p->data[2] = (next_net_pkt >>  8) & BYTE;
  p->data[3] = (next_net_pkt      ) & BYTE;
  next_net_pkt++;
}


void to_network_layer(packet *p)
{

  unsigned int num;

  num = pktnum(p);
  if (num != last_pkt_given + 1) {
	printf("Tick %u. Proc %d got protocol error.  Packet delivered out of order.\n", tick/DELTA, id); 
	printf("Expected payload %d but got payload %d\n",last_pkt_given+1,num);
	exit(0);
  }
  last_pkt_given = num;
  payloads_accepted++;
}

  
void from_physical_layer (frame *r)
{
 *r = last_frame;
}


void to_physical_layer(frame *s)
{


  int fd, got, k;

  switch(protocol) {
    case 2:
	s->seq = 0;

    case 3:
	s->kind = (id == 0 ? data : ack);
	if (s->kind == ack) {
		s->seq = 0;
		s->info.data[0] = 0;
		s->info.data[1] = 0;
		s->info.data[2] = 0;
		s->info.data[3] = 0;
	}
        break;

     case 4:
     case 5:
 	s->kind = data;
	break;

     case 6:
	if (s->kind == nak) {
		s->info.data[0] = 0;
		s->info.data[1] = 0;
		s->info.data[2] = 0;
		s->info.data[3] = 0;
	}


	if (s->kind==data) seqs[s->seq % (nseqs/2)] = s->seq; /* save seq # */
  }

  if (s->kind == data) data_sent++;
  if (s->kind == ack) acks_sent++;
  if (retransmitting) data_retransmitted++;


  k = rand() & 01777;		
  if (k < pkt_loss) {	
	if (debug_flags & SENDS) {
		printf("Tick %u. Proc %d sent frame that got lost: ",
							    tick/DELTA, id);
		fr(s);
	}
	if (s->kind == data) data_lost++;	
	if (s->kind == ack) acks_lost++;	
	return;

  }
  if (s->kind == data) data_not_lost++;		
  if (s->kind == ack) acks_not_lost++;		
  fd = (id == 0 ? w1 : w2);

  got = write(fd, s, FRAME_SIZE);
  if (got != FRAME_SIZE) print_statistics();	

  if (debug_flags & SENDS) {
	printf("Tick %u. Proc %d sent frame: ", tick/DELTA, id);
	fr(s);
  }
}


void start_timer(seq_nr k)
{
  ack_timer[k] = tick + timeout_interval + offset;
  offset++;
  recalc_timers();		
}


void stop_timer(seq_nr k)
{

  ack_timer[k] = 0;
  recalc_timers();		
}


void start_ack_timer(void)
{
  aux_timer = tick + timeout_interval/AUX;
  offset++;
}


void stop_ack_timer(void)
{
  aux_timer = 0;
}


void enable_network_layer(void)
{
  network_layer_status = 1;
}


void disable_network_layer(void)
{
  network_layer_status = 0;
}


int check_timers(void)
{
  int i;

  if (lowest_timer == 0 || tick < lowest_timer) return(-1);

  for (i = 0; i < NR_TIMERS; i++) {
	if (ack_timer[i] == lowest_timer) {
		ack_timer[i] = 0;	
		recalc_timers();	
                oldest_frame = seqs[i];	
		return(i);
	}
  }
  printf("Impossible.  check_timers failed at %d\n", lowest_timer);
  exit(1);
}


int check_ack_timer()
{
  if (aux_timer > 0 && tick >= aux_timer) {
	aux_timer = 0;
	return(1);
  } else {
	return(0);
  }
}


unsigned int pktnum(packet *p)
{
  unsigned int num, b0, b1, b2, b3;

  b0 = p->data[0] & BYTE;
  b1 = p->data[1] & BYTE;
  b2 = p->data[2] & BYTE;
  b3 = p->data[3] & BYTE;
  num = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
  return(num);
}


void fr(frame *f)
{
  printf("type=%s  seq=%d  ack=%d  payload=%d\n",
	tag[f->kind], f->seq, f->ack, pktnum(&f->info));
}

void recalc_timers(void)
{
  int i;
  bigint t = UINT_MAX;

  for (i = 0; i < NR_TIMERS; i++) {
	if (ack_timer[i] > 0 && ack_timer[i] < t) t = ack_timer[i];
  }
  lowest_timer = t;
}


void print_statistics(void)
{
  int word[3];

  sleep(1);
  printf("\nProcess %d:\n", id);
  printf("\tTotal data frames sent:  %9d\n", data_sent);
  printf("\tData frames lost:        %9d\n", data_lost);
  printf("\tData frames not lost:    %9d\n", data_not_lost);
  printf("\tFrames retransmitted:    %9d\n", data_retransmitted);
  printf("\tGood ack frames rec'd:   %9d\n", good_acks_recd);
  printf("\tBad ack frames rec'd:    %9d\n\n", cksum_acks_recd);

  printf("\tGood data frames rec'd:  %9d\n", good_data_recd);
  printf("\tBad data frames rec'd:   %9d\n", cksum_data_recd);
  printf("\tPayloads accepted:       %9d\n", payloads_accepted);
  printf("\tTotal ack frames sent:   %9d\n", acks_sent);
  printf("\tAck frames lost:         %9d\n", acks_lost);
  printf("\tAck frames not lost:     %9d\n", acks_not_lost);

  printf("\tTimeouts:                %9d\n", timeouts);
  printf("\tAck timeouts:            %9d\n", ack_timeouts);
  fflush(stdin);

  word[0] = 0;
  word[1] = payloads_accepted;
  word[2] = data_sent;
  write(mwfd, word, 3*sizeof(int));	
  sleep(1);
  exit(0);
}


void sim_error(char *s)
{
  int fd;

  printf("%s\n", s);
  fd = (id == 0 ? w4 : w6);
  write(fd, &zero, TICK_SIZE);
  exit(1);
}
