#include "contiki.h"
#include "lib/random.h"
#include "net/rime/rime.h"
#include <stdio.h>	

enum {
  ANNOUNCE,
  DISPUTE,
  FINASHED
};

int stage = ANNOUNCE;
static int id_cluster_head = 0;
static int block_broadcast_message = 0;
static int num_strength_received = 0;
static int max_strength_registered = 0;

/*---------------------------------------------------------------------------*/
PROCESS(broadcast_process, "Broadcast process");
AUTOSTART_PROCESSES(&broadcast_process);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* This function is called whenever a broadcast message is received. */

/* This is the structure of broadcast messages. */
struct broadcast_message {
  int stage;
  uint8_t strength;
};


static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  struct broadcast_message *m;

  m = packetbuf_dataptr();
  if (m->stage == ANNOUNCE) {
    num_strength_received++;
    printf("ANNOUNCE: num_strength_received %d\n", num_strength_received);
  }

  if (m->stage == DISPUTE) {
    
    printf("DISPUTE: num_strength_received %d\n", m->strength);
    if (num_strength_received > m->strength ) {
        printf("#A color=RED\n");
        printf("WIN to %d \n", from->u8[0]);
    } else {
        printf("LOST to %d \n",  from->u8[0]);
        if (max_strength_registered < m->strength) {
            max_strength_registered = m->strength;
            id_cluster_head = from->u8[0];
        }

        printf("#A color=GREEN\n");
        printf("\n");
    }
  }
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

static struct broadcast_conn broadcast;

PROCESS_THREAD(broadcast_process, ev, data)
{
  static int rounds;
  static struct etimer et;
  struct broadcast_message msg;
 
  PROCESS_EXITHANDLER(broadcast_close(&broadcast));
  PROCESS_BEGIN();

  broadcast_open(&broadcast, 129, &broadcast_call);
  while(1) {
    /* round duration is  every 1 - 2 seconds */
    etimer_set(&et, CLOCK_SECOND * 1 + random_rand() % (CLOCK_SECOND * 2));
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    rounds++;
    if (rounds > 2 && stage == ANNOUNCE) {
        stage = DISPUTE;
        block_broadcast_message = 0;
    }

    if (stage == DISPUTE && rounds > 4) {
        stage = FINASHED;
        block_broadcast_message = 0;
        continue;
    }

    if (!block_broadcast_message && stage == FINASHED) {
        if (id_cluster_head == 0) {
            printf("I AM A CLUSTER HEAD CANDIDATE \n");
            block_broadcast_message = 1;
        }

        if (id_cluster_head > 0) {
            printf("I AM ONLY A NODE MY CLUSTER HEAD CANDIDATE IS %d \n", id_cluster_head);
            printf("#L %d 1\n", id_cluster_head);
            block_broadcast_message = 1;
        }
    }
    
    if (block_broadcast_message == 1) {
      continue;
    }
    
    if (stage == DISPUTE) {
        puts("DISPUTE \n");
        msg.strength = num_strength_received;
        msg.stage = stage;

        packetbuf_copyfrom(&msg, sizeof(struct broadcast_message));
        broadcast_send(&broadcast);

        block_broadcast_message = 1;
      continue;
    }

    if (stage == ANNOUNCE) {
        puts("ANNOUNCE \n");
        msg.strength = num_strength_received;
        msg.stage = stage;

        packetbuf_copyfrom(&msg, sizeof(struct broadcast_message));
        broadcast_send(&broadcast);

        block_broadcast_message = 1;
      continue;
    }
  }

  PROCESS_END();
}
