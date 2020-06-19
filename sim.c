
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include "common.h"

#define DEADLOCK (3 * timeout_interval)	
#define MAX_PROTOCOL 6		
#define MANY 256		

bigint tick = 0;		
bigint last_tick;		
int exited[2];			
int hanging[2];			
struct sigaction act, oact;


void main(int argc, char *argv[]);
int parse_args(int argc, char *argv[]);
void set_up_pipes(void);
void fork_off_workers(void);
void terminate(char *s);
void sender2(void);
void receiver2(void);
void sender3(void);
void receiver3(void);
void protocol4(void);
void protocol5(void);
void protocol6(void);

void main(int argc, char *argv[])
{

  int process = 0;		
  int rfd, wfd;			
  bigint word;			

  act.sa_handler = SIG_IGN;
  setvbuf(stdout, (char *) 0, _IONBF, (size_t) 0);	
  if (parse_args(argc, argv) < 0) exit(1);     
  set_up_pipes();		
  fork_off_workers();


  while (tick <last_tick) {
	process = rand() & 1;		
	tick = tick + DELTA;
	rfd = (process == 0 ? r4 : r6);
	if (read(rfd, &word, TICK_SIZE) != TICK_SIZE) terminate("");
	if (word == OK) hanging[process] = 0;
	if (word == NOTHING) hanging[process] += DELTA;
	if (hanging[0] >= DEADLOCK && hanging[1] >= DEADLOCK)
		terminate("A deadlock has been detected");

	wfd = (process == 0 ? w3 : w5);
	if (write(wfd, &tick, TICK_SIZE) != TICK_SIZE)
		terminate("Main could not write to worker");

  }

  terminate("End of simulation");
}


int parse_args(int argc, char *argv[])
{
  if (argc != 7) {
	printf("Usage: sim protocol events timeout loss cksum debug\n");
	return(-1);
  }

  protocol = atoi(argv[1]);
  if (protocol < 2 || protocol > MAX_PROTOCOL) {
	printf("Protocol %d is not valid.\n", protocol);
	return(-1);
  }

  last_tick = DELTA * atol(argv[2]);	
  if ((long) last_tick < 0) {
	printf("Number of simulation events must be positive\n");
	return(-1);
  }


  timeout_interval = DELTA * atoi(argv[3]);
  if ((long)timeout_interval < 0 || (protocol > 2 && timeout_interval == 0) ){
	printf("Timeout interval must be positive\n");
	return(-1);
  }


  pkt_loss = atoi(argv[4]);	
  if (pkt_loss < 0 || pkt_loss > 99) {
	printf("Packet loss rate must be between 0 and 99\n");
	return(-1);
  }
  pkt_loss = 10 * pkt_loss;	


  garbled = atoi(argv[5]);
  if (garbled < 0 || garbled > 99) {
	printf("Packet cksum rate must be between 0 and 99\n", garbled);
	return(-1);
  }
  garbled = 10 * garbled;	

  
  debug_flags = atoi(argv[6]);
  if (debug_flags < 0) {
	printf("Debug flags may not be negative\n", debug_flags);
	return(-1);
  }
  printf("\n\nProtocol %d.   Events: %u    Parameters: %u %d %u\n", protocol,
      last_tick/DELTA, timeout_interval/DELTA, pkt_loss/10, garbled/10,
								debug_flags);
  return(0);			
}

void set_up_pipes(void)
{


  int fd[2];

  pipe(fd);  r1 = fd[0];  w1 = fd[1];	/* M0 to M1 for frames */
  pipe(fd);  r2 = fd[0];  w2 = fd[1];	/* M1 to M0 for frames */
  pipe(fd);  r3 = fd[0];  w3 = fd[1];	/* main to M0 for go-ahead */
  pipe(fd);  r4 = fd[0];  w4 = fd[1];	/* M0 to main to signal readiness */
  pipe(fd);  r5 = fd[0];  w5 = fd[1];	/* main to M1 for go-ahead */
  pipe(fd);  r6 = fd[0];  w6 = fd[1];	/* M1 to main to signal readiness */
}

void fork_off_workers(void)
{


  if (fork() != 0) {
	
	if (fork() != 0) {		
		sigaction(SIGPIPE, &act, &oact);
	        setvbuf(stdout, (char *)0, _IONBF, (size_t)0);/*don't buffer*/
		close(r1);
		close(w1);
		close(r2);
		close(w2);
		close(r3);
		close(w4);
		close(r5);
		close(w6);
		return;
	} else {		
		sigaction(SIGPIPE, &act, &oact);
	        setvbuf(stdout, (char *)0, _IONBF, (size_t)0);/*don't buffer*/
		close(w1);
		close(r2);
		close(r3);
		close(w3);
		close(r4);
		close(w4);
		close(w5);
		close(r6);
	
		id = 1;		
		mrfd = r5;	
		mwfd = w6;	
		prfd = r1;	
		switch(protocol) {
			case 2:	receiver2();	break;
			case 3:	receiver3();	break;
			case 4: protocol4();	break;
			case 5: protocol5();	break;
			case 6: protocol6();	break;
		}
		terminate("Impossible.  Protocol terminated");
	}
  } else {
	
	sigaction(SIGPIPE, &act, &oact);
        setvbuf(stdout, (char *)0, _IONBF, (size_t)0);
	close(r1);
	close(w2);
	close(w3);
	close(r4);
	close(r5);
	close(w5);
	close(r6);

	id = 0;		
	mrfd = r3;	
	mwfd = w4;	
	prfd = r2;	

	switch(protocol) {
		case 2:	sender2();	break;
		case 3:	sender3();	break;
		case 4: protocol4();	break;
		case 5: protocol5();	break;
		case 6: protocol6();	break;
	}
	terminate("Impossible. protocol terminated");
  }
}

void terminate(char *s)
{


  int n, k1, k2, res1[MANY], res2[MANY], eff, acc, sent;

  for (n = 0; n < MANY; n++) {res1[n] = 0; res2[n] = 0;}
  write(w3, &zero, TICK_SIZE);
  write(w5, &zero, TICK_SIZE);
  sleep(2);

  
  n = read(r4, res1, MANY*sizeof(int));
  k1 = 0;
  while (res1[k1] != 0) k1++;
  k1++;				

  
  n = read(r6, res2, MANY*sizeof(int));
  k2 = 0;
  while (res1[k2] != 0) k2++;
  k2++;				

  if (strlen(s) > 0) {
	acc = res1[k1] + res2[k2];
	sent = res1[k1+1] + res2[k2+1];
	if (sent > 0) {
		eff = (100 * acc)/sent;
 	        printf("\nEfficiency (payloads accepted/data pkts sent) = %d%c\n", eff, '%');
	}
	printf("%s.  Time=%u\n",s, tick/DELTA);
  }
  exit(1);
 }
