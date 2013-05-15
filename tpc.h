#ifndef TPC_H
#define TPC_H

#include "headers.h"

#define MAX_NR_PLAYERS 52
#define MAX_NICK_LENGTH 21
#define NR_CARDS 52

// array to hold the ordered deck of cards
char cards[NR_CARDS][2];

// structure to hold each player's info
typedef struct {
  int number;
  char nickname[MAX_NICK_LENGTH];
  char fifo_path[PATH_MAX];
  
} players_info_t;

// structure to be allocated on the shared memory region
typedef struct {
  int nr_players;
  int dealer;
  int last_loggedin_player;
  int turn_to_play;
  int round_number;
  players_info_t players[MAX_NR_PLAYERS];
  char cards_on_table[NR_CARDS][2];
  
  /*
   * missing mutexes and condition variables
   */
} shared_fields_t;

#endif