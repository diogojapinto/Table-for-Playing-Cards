#ifndef TPC_H
#define TPC_H

#include "headers.h"

#define MAX_NR_PLAYERS 52
#define MIN_NR_PLAYERS 2
#define MAX_NICK_LENGTH 21
#define NR_CARDS 52
#define CHARS_PER_CARD 4
#define LINE_SIZE 500

/*
 * event's strings
 */
#define DEAL_EVENT "deal"
#define RECEIVE_CARDS_EVENT "receive_cards"
#define PLAY_EVENT "play"
#define HAND_EVENT "hand"
#define END_GAME_EVENT "end game"

//structure to write events to log file
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
  pthread_mutex_t log_mut;
  int game_ended;
} shared_fields_t;

/** Verify if the arguments passed in the program call are correct
 *
 */
 int verifyCmdArgs(char **argv);
/** Creates the fifo for the player and opens it ready for reading
 * 
 */
 void initFIFO(char *name);
/** Creates the shared memory having the dealer initializing all the variables
 *  
 */
 void initSharedMem(char **args);
 void exitHandler(void);
/** Creates a new deck to the proper structure
 *
 */
 void initDefaultDeck();
/** Shuffles the deck
 *
 */
 void shuffleDeck();
/** Deals each player cards by writing to their own fifo
 *
 */
 void *dealCards(void *ptr);
/** Players receive the cards by reading their fifo
 *
 */
 void receiveCards();
/** uses a condition variable to wait for all the players to join the game
 *
 */
 void waitForPlayers();
/** Thread that reads user input to choose a card and play it
 *
 */
 void *playCard(void *ptr);
/** removes the card from the player hand
 *
 */
 void removeCardFromHand(int cardNumber);
/** Adds the played card to the table cards structure
 *
 */
 void addCardToTable(int cardNumber);
/** Updates the variable representing the next player to play, increases the round number when needed
 *  and ends the game
 *
 */
 void updatePlayersTurn();
/** Displays the rounds cards
 *
 */
 void displayRoundInfo();
/** Counts the time of the current turn
 *
 */
 void *turnTime(void *ptr);
/** Reorders the cards by suit
 *
 */
 void reorderCardsList(char cards[][4]);
/** Creates a signal mask to block the players from force quiting the game
 *
 */
 void blockSignals();
/** thread that syncronizes the game events and displays all information to the player 
 *
 */
 void *playGame(void *ptr);
/** prints the cards to the console or to alloc_str
 *
 */
 void printCardsList(char cards[][4], char *alloc_str);
/** Chooses first player
 *
 */
 void randomiseFirstPlayer();
 void callFirstPlayer();
/** Function that searchs the user input and checks if it represents a valid card
 *
 */
 int searchCard(char card[4], int i);
/** writes the header to the log file
 *
 */
 void *writeHeaderToLog(void *ptr);
/** writes different events to the log file
 *
 */
 void *writeEventToLog(void *info_ptr);
/** Calls a thread to write the hand event to the log file
 *
 */
 void callHandEvent();
/** Creates a thread that writes the deal event to the log file
 *
 */
 void callDealEvent();
/** calls a thread to write the receive event to the log file
 *
 */
 void callReceiveEvent();
/** Calls a thread to write the play event to the log file
 *
 */
 void callPlayEvent(int cardNumber);
/** Calls the thread that displays the time of the current turn
 *
 */
 void callTimeThread(int playerNr);
 /**
 * Calls the thread to write the end event to log
 */
 void callEndEvent();

#endif