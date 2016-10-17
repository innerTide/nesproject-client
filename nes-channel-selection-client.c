/*
 * 
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Client Source Code for NES project
 * \author
 *         Joakim Eriksson, joakime@sics.se
 *          Adam Dunkels, adam@sics.se 
 *          Yuefeng Wu, y.wu.5@student.tue.nl
 */

#include "contiki.h"
#include "net/rime/rime.h"
#include "sys/etimer.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "cc2420.h"
#include "cc2420_const.h"
#include "dev/spi.h"
#include <stdio.h>
#include <string.h>


/*---------------------------------------------------------------------------*/
/* This is a set of predefined values for this laboratory*/

#define DEBUG 1

/*---------------------------------------------------------------------------*/
/* This is a set of global variables for this laboratory*/

int currentChannel = 26;
int receivedChannel = 26;
unsigned int currentCounter = 0;
char instructionBuff [6];
int coordinatorAddr[2];
unsigned char broadcastEnabled = 1;

/*---------------------------------------------------------------------------*/
/* This assumes that the CC2420 is always on and "stable" */

/*---------------------------------------------------------------------------*/
PROCESS(client_listener, "Client Listener");
AUTOSTART_PROCESSES(&client_listener);
/*---------------------------------------------------------------------------*/
/* Broadcast channel selection to clients*/
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
	char* receivedInst = (char *)packetbuf_dataptr();
	if (*receivedInst == 'C' || *receivedInst == 'R'){
		printf("broadcast message received from %d.%d: '%s'\n", from->u8[0], from->u8[1], receivedInst);
		receivedChannel = (((int) *(receivedInst+1))-48)*10 + (((int) *(receivedInst+2))-48);
		int receivedCounter = (((int) *(receivedInst+3))-48)*10 + (((int) *(receivedInst+4))-48);
		printf ("Received channel is %d, counter is %d\n", receivedChannel, receivedCounter);
		if (*receivedInst == 'C'){
			if (receivedCounter>currentCounter || receivedCounter == 0){
				coordinatorAddr[0] = from -> u8[0];
				coordinatorAddr[1] = from -> u8[1];
				char *repostedInst = receivedInst;
				
				/*Replace the first char in initial with "R" to indicate the reposted inst.*/
				*repostedInst = 'R';
				packetbuf_copyfrom(repostedInst, 6);
				printf ("Channel is reset to CH=%d, instruction : %s is being reposted!\n",receivedChannel,repostedInst);
				currentCounter=receivedCounter;
			}
			else{
				printf ("This is a late instruction!\n");
			}
		}
		else if (*receivedInst == 'R'){
			if (receivedCounter>currentCounter || receivedCounter == 0){
				printf ("Channel is reset to CH=%d, based on the info from other clients\n",receivedChannel);
			}
			currentCounter=receivedCounter;
		}
	}
	else if (*receivedInst == 'S'){
            broadcastEnabled = 0;
		printf ("Broadcast stops due to inst.: \"%s\" from coordinator\n", receivedInst);
	}
	else if (*receivedInst == 'B'){
            broadcastEnabled = 1;
            printf ("Broadcast restarts due to inst.: \"%s\" from coordinator\n", receivedInst);
        }
        else{
            printf ("Invalid instruction, Discard!\n");
        }
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(client_listener, ev, data)
{
	static struct etimer etScan;
	
	PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
	
	PROCESS_BEGIN();
	/* switch mac layer off, and turn radio on */
	
	broadcast_open(&broadcast, 129, &broadcast_call);
	
	NETSTACK_MAC.off(0);
	
	cc2420_on();
	
	while(1) {
		etimer_set (&etScan, CLOCK_SECOND*3);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etScan));
		
		/*Return to initial channel*/
		cc2420_set_channel (currentChannel);
		if (broadcastEnabled){
                    broadcast_send(&broadcast);
                }
		
		/*Change to better channel*/
		cc2420_set_channel (receivedChannel);
		currentChannel=receivedChannel;
		
	}
	
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
