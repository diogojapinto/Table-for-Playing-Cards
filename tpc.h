#ifndef TPC_H
#define TPC_H

#include "headers.h"

#define MAX_NR_PLAYERS 52
#define MIN_NR_PLAYERS 2
#define MAX_NICK_LENGTH 21
#define NR_CARDS 52
#define CHARS_PER_CARD 4
#define LINE_SIZE 200

/*
 * event's strings
 */
#define DEAL_EVENT "deal"
#define RECEIVE_CARDS_EVENT "receive_cards"
#define PLAY_EVENT "play"
#define HAND_EVENT "hand"

typedef struct {
  char who[LINE_SIZE];
  char what[LINE_SIZE];
  char result[LINE_SIZE];
} print_info_t;

// array to hold the ordered deck of cards
char cards[NR_CARDS + 1][CHARS_PER_CARD];

// structure to hold each player's info
typedef struct {
  int number;
  char nickname[MAX_NICK_LENGTH];
  char fifo_path[PATH_MAX];
} players_info_t;

// structure to be allocated on the shared memory region
typedef struct {
  char tables_name[MAX_NICK_LENGTH];
  int nr_players;
  int dealer;
  int last_loggedin_player;
  int turn_to_play;
  int first_player;
  int round_number;
  players_info_t players[MAX_NR_PLAYERS];
  char cards_on_table[NR_CARDS][CHARS_PER_CARD];
  pthread_mutex_t startup_mut;
  pthread_cond_t startup_cond_var;
  pthread_mutex_t deal_cards_mut[MAX_NR_PLAYERS];
  pthread_mutex_t play_mut;
  pthread_cond_t play_cond_var;
  int game_ended;
} shared_fields_t;

int verifyCmdArgs(char **argv);
void initFIFO(char *name);
void initSharedMem(char **args);
void exitHandler(void);
void initDefaultDeck();
void shuffleDeck();
void *dealCards(void *ptr);
void receiveCards();
void waitForPlayers();
void *playCard(void *ptr);
void removeCardFromHand(int cardNumber);
void addCardToTable(int cardNumber);
void updatePlayersTurn();
void displayRound();
void turnTime(int playing, int min, int sec);
void reorderCardsList(char cards[][4]);
void blockSignals();
void *playGame(void *ptr);
void printCardsList(char cards[][4]);
void randomiseFirstPlayer();
void callFirstPlayer();
int searchCard(char card[4], int i);
void *writeEventToLog(void *info_ptr);
void randomiseFirstPlayer();

#endif