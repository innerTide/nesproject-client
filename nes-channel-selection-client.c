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
 * 			Yuefeng Wu, y.wu.5@student.tue.nl
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
unsigned int currentCounter = 0;
char instructionBuff [6];
int coordinatorAddr[2];

/*---------------------------------------------------------------------------*/
/* This assumes that the CC2420 is always on and "stable" */
static void set_frq(int c)
{
	int f;
	/* We can read even other channels with CC2420! */
	/* Fc = 2048 + FSCTRL  ;  Fc = 2405 + 5(k-11) MHz, k=11,12, ... , 26 */
	f = (c-11)*5 + 357; /* Start from 2405 MHz to 2480 MHz, step by 5, and channel starts at 11*/
	
	/* Write the new channel value */
	CC2420_SPI_ENABLE();
	SPI_WRITE_FAST(CC2420_FSCTRL);
	SPI_WRITE_FAST((uint8_t)(f >> 8));
	SPI_WRITE_FAST((uint8_t)(f & 0xff));
	SPI_WAITFORTx_ENDED();
	SPI_WRITE(0);
	
	/* Send the strobe */
	SPI_WRITE(CC2420_SRXON);
	CC2420_SPI_DISABLE();
}

/*---------------------------------------------------------------------------*/
PROCESS(client_listener, "Client Listener");
AUTOSTART_PROCESSES(&client_listener);
/*---------------------------------------------------------------------------*/
/* Broadcast channel selection to clients*/
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
	char* receivedInst = (char *)packetbuf_dataptr();
	printf("broadcast message received from %d.%d: '%s'\n", from->u8[0], from->u8[1], receivedInst);
	int receivedChannel = (((int) *(receivedInst+1))-48)*10 + (((int) *(receivedInst+2))-48);
	int receivedCounter = (((int) *(receivedInst+4))-48)*10 + (((int) *(receivedInst+5))-48);
	printf ("Received channel is %d, counter is %d\n", receivedChannel, receivedCounter);
	if (*receivedInst == 'C'){
		if (receivedCounter>currentCounter || receivedCounter == 0){
			coordinatorAddr[0] = from -> u8[0];
			coordinatorAddr[1] = from -> u8[1];
			char *repostedInst = receivedInst;
			*repostedInst = 'R';
			packetbuf_copyfrom(repostedInst, 6);
			printf ("Channel is reset to CH=%d, instruction : %s is being reposted!\n",receivedChannel,repostedInst);
			set_frq (receivedChannel);
		}
		else{
			printf ("This is a late instruction!\n");
		}
	}
	else if (*receivedInst == 'R'){
		if (receivedCounter>currentCounter || receivedCounter == 0){
			printf ("Channel is reset to CH=%d, based on the info from other clients\n",receivedChannel);
			set_frq (receivedChannel);
		}
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
		broadcast_send(&broadcast);
	}
	
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
