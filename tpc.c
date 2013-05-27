/*
 * NOTES (things to add on the short future):
 *
 */

#include "tpc.h"

int is_dealer = 0;
int shmid = 0;
shared_fields_t *shm_ptr = NULL;
char own_fifo_path[PATH_MAX];
int fifo_filedes = -1;
char hand[NR_CARDS / 2][CHARS_PER_CARD];
int nr_cards_in_hand = 0;
int player_nr = 0;
char table_path[PATH_MAX];
char log_name[LINE_SIZE];
int quit_thread = 0;

int main (int argc, char **argv) {
  #ifdef CLEAR
  strcpy(table_path, dirname(argv[0]));
  strcat(table_path, "/");
  strcat(table_path, argv[2]);
  shm_unlink(table_path);
  #else
  // verifies if the number of arguments is correct
  if (argc != 4) {
    printf("usage: %s <player's name> <table's name> <nr. players>\n", argv[0]);
    return -1;
  }
  
  if (verifyCmdArgs(argv) == -1) {
    return -1;
  }
  
  // installs an exit handler
  if (atexit(exitHandler) == -1) {
    perror("atexit()");
    exit(-1);
  }
  
  blockSignals();
  
  initFIFO(argv[1]);
  
  initSharedMem(argv);
  
  waitForPlayers();
  
  pthread_t tid;
  
  if (is_dealer) {
    
    initDefaultDeck();
    printCardsList(cards, NULL);
    shuffleDeck();
    randomiseFirstPlayer();
    
    if ((errno = pthread_create(&tid, NULL, dealCards, NULL)) != 0) {
      perror("pthread_create()");
      exit(-1);
    }
  }
  receiveCards();
  reorderCardsList(hand);
  
  if (is_dealer) {
    pthread_join(tid, NULL);
  }
  //call thread responsible for showing info about the game and manage the game
  pthread_t tidG;
  if ((errno = pthread_create(&tidG, NULL, playGame, NULL)) != 0) {
    perror("pthread_create()");
    exit(-1);
  }
  
  pthread_join(tidG, NULL);
  
  #endif
  return 0;
}

/** Verify if the arguments passed in the program call are correct
 * 
 */
int verifyCmdArgs(char **argv) {
  
  // verifies if the 3rd arg is an integer
  char *c;
  for ( c = argv[3]; c != NULL && *c != '\0'; c++) {
    if (!isdigit(*c)) {
      printf("The 3rd argument must me an integer.\n");
      return -1;
    }
  }
  
  int players_name_size = strlen(argv[1]);
  
  if (players_name_size > MAX_NICK_LENGTH ) {
    printf("Invalid nickname dimension.\n");
    return -1;
  }
  
  if (is_dealer) {
    int tables_name_size = strlen(argv[2]);
    
    if (tables_name_size > MAX_NICK_LENGTH ) {
      printf("Invalid table's name dimension.\n");
      return -1;
    }
  }
  
  int nr_players = atoi(argv[3]);
  if (nr_players > MAX_NR_PLAYERS || nr_players < MIN_NR_PLAYERS) {
    printf("Invalid number of players.\n");
    return -1;
  }
  
  return 0;
}

/** Creates the fifo for the player and opens it ready for reading
 * 
 */
void initFIFO(char *name) {
  
  // tries to create the FIFO
  mkfifo(name, S_IRUSR | S_IWUSR);
  
  if (realpath(name, own_fifo_path) == NULL) {
    perror("realpath()");
    exit(-1);
  }
  
  if ((fifo_filedes = open(own_fifo_path, O_RDONLY | O_CREAT | O_TRUNC | O_NONBLOCK)) == -1) {
    perror("open()");
  }
}

/** Creates the shared memory having the dealer initializing all the variables
 *  
 */
void initSharedMem(char **args) {
  int shm_fd;
  strcpy(table_path, dirname(args[0]));
  strcat(table_path, "/");
  strcat(table_path, args[2]);
  
  if ((shm_fd = shm_open(table_path, O_CREAT | O_EXCL | O_RDWR, 0600)) == -1) {
    if (errno == EEXIST) {
      // if it wasn't successfull, test if it already existed
      if ((shm_fd = shm_open(table_path, O_RDWR, 0600)) == -1) {
	perror("shm_open()");
	exit(-1);
      }
      is_dealer = 0;
    } else {
      perror("shm_open()");
      exit(-1);
    }
  } else {
    is_dealer = -1;
    
    if (ftruncate(shm_fd, sizeof(shared_fields_t)) == -1) {
      perror("ftruncate()");
      exit(-1);
    }
  }
  if ((shm_ptr = (shared_fields_t *)mmap(0, sizeof(shared_fields_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == NULL) {
    perror("mmap");
    exit(-1);
  }  
  
  //initialize the mutexes and condition variables attributes  
  pthread_mutexattr_t mattr;
  pthread_condattr_t cattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
  pthread_condattr_init(&cattr);
  pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_t *mptr;
  pthread_cond_t *cptr;
  
  if (is_dealer) {
    strcpy(shm_ptr->tables_name, args[2]);
    shm_ptr->nr_players = atoi(args[3]);
    shm_ptr->dealer = 0;
    shm_ptr->last_loggedin_player = 0;
    shm_ptr->turn_to_play = 0;
    shm_ptr->round_number = 0;
    shm_ptr->players[0].number = 0;
    strcpy(shm_ptr->players[0].nickname, args[1]);
    strcpy(shm_ptr->players[0].fifo_path, own_fifo_path);
    shm_ptr->game_ended = 0;
    
    //initialize shared mutexes and conditional variables    
    
    mptr = &(shm_ptr->startup_mut);
    if (pthread_mutex_init(mptr, &mattr) == -1) {
      perror("pthread_mutex_init()");
      exit(-1);
    }
    
    mptr = &(shm_ptr->deal_cards_mut[0]);
    if (pthread_mutex_init(mptr, &mattr) == -1) {
      perror("pthread_mutex_init()");
      exit(-1);
    }
    
    mptr = &(shm_ptr->play_mut);
    if (pthread_mutex_init(mptr, &mattr) == -1) {
      perror("pthread_mutex_init()");
      exit(-1);
    }
    
    
    mptr = &(shm_ptr->log_mut);
    if (pthread_mutex_init(mptr, &mattr) == -1) {
      perror("pthread_mutex_init()");
      exit(-1);
    }
    
    cptr = &(shm_ptr->startup_cond_var);
    if (pthread_cond_init(cptr, &cattr) == -1) {
      perror("pthread_cond_init()");
      exit(-1);
    }
    
    cptr = &(shm_ptr->play_cond_var);
    if (pthread_cond_init(cptr, &cattr) == -1) {
      perror("pthread_cond_init()");
      exit(-1);
    }
    
  } else {
    if (atoi(args[3]) != shm_ptr->nr_players) {
      printf("Number of players different from the one saved!\n");
      exit(-1);
    }
    player_nr = ++(shm_ptr->last_loggedin_player);
    shm_ptr->players[player_nr].number = player_nr;
    strcpy(shm_ptr->players[player_nr].nickname, args[1]);
    strcpy(shm_ptr->players[player_nr].fifo_path, own_fifo_path);
    
    mptr = &(shm_ptr->deal_cards_mut[player_nr]);
    if (pthread_mutex_init(mptr, &mattr) == -1) {
      perror("pthread_mutex_init()");
      exit(-1);
    }
    
    pthread_cond_broadcast(&(shm_ptr->startup_cond_var));
  }
  
  printf("Player %s logged in with the index %d.\n", shm_ptr->players[shm_ptr->last_loggedin_player].nickname, shm_ptr->last_loggedin_player);
}

/** Creates a new deck to the proper structure
 * 
 */
void initDefaultDeck() {
  char ranks[][3] = {" A", " 2", " 3", " 4", " 5", " 6", " 7", " 8", " 9", "10", " J", " Q", " K"};
  int ranks_nr = 13;
  char suits[][2] = {"c", "d", "h", "s"};
  int suits_nr = 4;
  
  int i = 0;
  int j = 0;
  for (i = 0; i < suits_nr; i++) {
    for (j = 0; j < ranks_nr; j++) {
      int curr_card = (i * ranks_nr) + j;
      strcpy(cards[curr_card], ranks[j]);
      strcat(cards[curr_card], suits[i]);
    }
  }
  strcpy(cards[NR_CARDS], "\0");
}

/** Shuffles the deck
 * 
 */
void shuffleDeck() {
  
  srand(time(NULL));
  
  int nr_cycles = 200, i = 0;
  
  for (i = 0; i < nr_cycles; i++) {
    int rand_card_1 = rand() % NR_CARDS;
    int rand_card_2 = rand() % NR_CARDS;
    char tmp[4];
    strcpy(tmp,cards[rand_card_1]);
    strcpy(cards[rand_card_1],cards[rand_card_2]);
    strcpy(cards[rand_card_2], tmp);
  }
}

/** Deals each player cards by writing to their own fifo
 * 
 */
void *dealCards(void *ptr) {
  int card_index = NR_CARDS - 1;
  int player_index = 0;
  int i = 0;
  int nr_players = shm_ptr->nr_players;
  players_info_t *players_ptr = shm_ptr->players;
  int cards_per_player = NR_CARDS / nr_players;
  
  printf("Giving cards\n");
  
  pthread_t tid;
  if ((errno = pthread_create(&tid, NULL, writeHeaderToLog, NULL)) != 0) {
    perror("pthread_create()");
    exit(-1);
  }
  
  for (player_index = 0; player_index < nr_players; player_index++) {
    int players_fifo;
    if ((players_fifo = open(players_ptr[player_index].fifo_path, O_WRONLY)) == -1) {
      perror("open()");
      exit(-1);
    }
    
    pthread_mutex_lock(&(shm_ptr->deal_cards_mut[player_index]));
    
    for (i = 0; i < cards_per_player; i++) {
      write(players_fifo, cards[card_index--], 3);
      write(players_fifo, "\n", 1);
    }
    
    write(players_fifo, "\0", 1);
    
    if (close(players_fifo) == -1) {
      perror("close()");
      exit(-1);
    }
    
    callDealEvent();
    
    pthread_mutex_unlock(&(shm_ptr->deal_cards_mut[player_index]));
  }
  
  return NULL;
}

/** Creates a thread that writes the deal event to the log file
 * 
 */
void callDealEvent() {
  print_info_t *print_struct = malloc(sizeof(print_info_t));
  
  strcpy(print_struct->who, shm_ptr->players[0].nickname);
  strcpy(print_struct->what, DEAL_EVENT);
  strcpy(print_struct->result, "-");
  
  pthread_t tidP;
  if ((errno = pthread_create(&tidP, NULL, writeEventToLog, print_struct)) != 0) {
    perror("pthread_create()");
    exit(-1);
  }
}

/** Players receive the cards by reading their fifo
 * 
 */
void receiveCards() {
  char card[CHARS_PER_CARD];
  int hand_index = 0;
  
  while(1) {
    pthread_mutex_lock(&(shm_ptr->deal_cards_mut[player_nr]));
    ssize_t nr_chars_read;
    if ((nr_chars_read = read(fifo_filedes, card, CHARS_PER_CARD)) == CHARS_PER_CARD) {
      card[3] = '\0';
      strcpy(hand[hand_index++], card);
    } else {
      if (card[0] == '\0' && hand_index != 0) {
	break;
      }
    }
    pthread_mutex_unlock(&(shm_ptr->deal_cards_mut[player_nr]));
  }
  
  strcpy(log_name, shm_ptr->tables_name);
  strcat(log_name, ".log");
  
  pthread_mutex_unlock(&(shm_ptr->deal_cards_mut[player_nr]));
  pthread_mutex_destroy(&(shm_ptr->deal_cards_mut[player_nr]));
  
  strcpy(hand[hand_index], "\0");
  nr_cards_in_hand = hand_index;
  
  if (close(fifo_filedes) == -1) {
    perror("close()");
    exit(-1);
  }
  
  if (unlink(own_fifo_path) == -1) {
    perror("unlink()");
    exit(-1);
  }
  
  printf("Received Cards\n");
}

/** uses a condition variable to wait for all the players to join the game
 * 
 */
void waitForPlayers() {
  pthread_mutex_lock(&(shm_ptr->startup_mut));
  while(shm_ptr->nr_players > shm_ptr->last_loggedin_player + 1) {
    printf("Waiting for other players\n");
    pthread_cond_wait(&(shm_ptr->startup_cond_var), &(shm_ptr->startup_mut));
  }
  printf("%s is complete.\nThe game may start.\nDealer is %s.\n", shm_ptr->tables_name, shm_ptr->players[shm_ptr->dealer].nickname);
  pthread_mutex_unlock(&(shm_ptr->startup_mut));
  
  if (is_dealer) {
    // destroy mutexes and condition variables
    pthread_mutex_destroy(&(shm_ptr->startup_mut));
    pthread_cond_destroy(&(shm_ptr->startup_cond_var));
  }
}

void exitHandler(void) {
  
  munmap(shm_ptr, sizeof(shared_fields_t));
  
  // by convention, the dealer frees the shared memory block
  if (is_dealer) {
    if (shm_unlink(table_path) == -1) {
      perror("shm_unlink()");
    }
  }
}

/** Thread that reads user input to choose a card and play it
 * 
 */
void *playCard(void *ptr) {
  char ch;
  char chosen_card[4];
  int cardNumber;
  
  
  int i=0;
  while (1) {
    
    printf("Insert card: \n");
    
    while (read(STDIN_FILENO, &ch, 1) == 1) {
      if (ch != '\n') {
	chosen_card[i] = ch;
	i++;
      }
      else {
	chosen_card[i] = '\0';
	break;
      }
    }
    
    cardNumber = searchCard(chosen_card,i);
    if (cardNumber != -1) {
      break;
    }
    else {
      printf("\nWrong Card! Try again.\n");
      i=0;
    }
  }
  
  printf("card played: %s\n", hand[cardNumber]);
  
  callPlayEvent(cardNumber);
  
  addCardToTable(cardNumber);
  
  removeCardFromHand(cardNumber);
  
  updatePlayersTurn();
  
  return NULL;
}

/** Calls a thread to write the play event to the log file
 * 
 */
void callPlayEvent(int cardNumber) {
  print_info_t *print_struct = malloc(sizeof(print_info_t));
  
  strcpy(print_struct->who, shm_ptr->players[player_nr].nickname);
  strcpy(print_struct->what, PLAY_EVENT);
  
  strcpy(print_struct->result, hand[cardNumber]);
  
  pthread_t tidP;
  if ((errno = pthread_create(&tidP, NULL, writeEventToLog, print_struct)) != 0) {
    perror("pthread_create()");
    exit(-1);
  }
}

/** Function that searchs the user input and checks if it represents a valid card
 * 
 */
int searchCard(char chosen_card[4], int i) {
  
  //if the card only has 3 chars fills the array with the proper spaces
  if (i != 3) {
    chosen_card[3] = '\0';
    chosen_card[2] = chosen_card[1];
    chosen_card[1] = chosen_card[0];
    chosen_card[0] = ' ';
  }
  
  int j=0;
  for(;j < nr_cards_in_hand;j++) {
    if (strcmp(chosen_card, hand[j]) == 0) {
      return j;
    }
  }
  
  return -1;
}

/** Adds the played card to the table cards structure
 * 
 */
void addCardToTable(int cardNumber) {
  
  pthread_mutex_lock(&(shm_ptr->play_mut));
  int i = 0;
  
  if (player_nr >= shm_ptr->first_player) { 
    i = shm_ptr->round_number * shm_ptr->nr_players + player_nr - shm_ptr->first_player;
  }
  else {
    i = shm_ptr->round_number * shm_ptr->nr_players + shm_ptr->last_loggedin_player - shm_ptr->first_player + player_nr+1;
  }
  
  strcpy(shm_ptr->cards_on_table[i], hand[cardNumber]);
  
  printf("%s\n",shm_ptr->cards_on_table[i] );
  
  pthread_mutex_unlock(&(shm_ptr->play_mut));
  
}

/** removes the card from the player hand
 * 
 */
void removeCardFromHand(int cardNumber) {
  
  int i=cardNumber;
  
  while (i<nr_cards_in_hand - 1) {
    
    strcpy(hand[i],hand[i+1]);
    i++;
  }
  strcpy(hand[i],"\0");
  nr_cards_in_hand--;
}

/** Updates the variable representing the next player to play, increases the round number when needed
 *  and ends the game
 *
 */
void updatePlayersTurn() {
  
  pthread_mutex_lock(&(shm_ptr->play_mut));
  
  if ((shm_ptr->turn_to_play + 1) < shm_ptr->nr_players) {
    shm_ptr->turn_to_play += 1;
  }
  else {
    shm_ptr->turn_to_play = 0;
  }
  
  if (shm_ptr->turn_to_play == shm_ptr->first_player) {
    (shm_ptr->round_number)++;
    if (strcmp(hand[0],"\0") == 0) {
      shm_ptr->game_ended = -1;
    }
  }
  
  pthread_cond_broadcast(&(shm_ptr->play_cond_var));
  
  pthread_mutex_unlock(&(shm_ptr->play_mut));
}

/** Displays the rounds cards
 * 
 */
void displayRoundInfo() {
  
  int i =0;
  
  if (shm_ptr->round_number != 0) {
    printf("\nLast Round: ");
    while(i < shm_ptr->nr_players-1) {
      printf("%s - ", shm_ptr->cards_on_table[(shm_ptr->round_number-1)*shm_ptr->nr_players+i]);
      i++;
    }
    printf("%s\n", shm_ptr->cards_on_table[(shm_ptr->round_number-1)*shm_ptr->nr_players+i]);
  }
  
  if (shm_ptr->turn_to_play != shm_ptr->first_player)
    printf("\nCards this round: ");
  else
    printf("\nNo cards this round.\n ");
  
  i=0;
  int a = 0;
  
  i = shm_ptr->first_player;
  while(i < shm_ptr->turn_to_play - 1) {
    printf("%s - ", shm_ptr->cards_on_table[shm_ptr->round_number * shm_ptr->nr_players + a]);
    i++;
    a++;
  }
  
  if (shm_ptr->turn_to_play < shm_ptr->first_player) {
    printf("%s - ", shm_ptr->cards_on_table[shm_ptr->round_number * shm_ptr->nr_players + a]);
    a++;
    i = 0;
    while(i < shm_ptr->turn_to_play - 1) {
      printf("%s - ", shm_ptr->cards_on_table[shm_ptr->round_number * shm_ptr->nr_players + a]);
      i++;
      a++;
    }
  }
  printf("%s\n", shm_ptr->cards_on_table[shm_ptr->round_number * shm_ptr->nr_players + a]);
}

/** Counts the time of the current turn
 * 
 */
void *turnTime(void *ptr) {
  
  int playing = *(int *)ptr;
  free (ptr);
  
  struct tm *current;
  struct tm *init;
  time_t current_time1, init_time;
  int delmin = 0, delsec = 0;
  int sec = 0;
  
  pthread_t tid = pthread_self();
  
  //takes the initial time of the turn
  if (pthread_detach(tid) != 0) {
    perror("pthread_detach()");
    exit(-1);
  }
  
  if (time(&init_time) == -1) {
    perror("time()");
    exit(-1);
  }
  
  if ((init = localtime(&init_time)) == NULL) {
    perror("localtime()");
    exit(-1);
  }
  
  sec = init->tm_sec;
  int updated = 0;
  
  while (!quit_thread) { 
    if (time(&current_time1) == -1) {
      perror("time()");
      exit(-1);
    }
    
    if ((current = localtime(&current_time1)) == NULL) {
      perror("localtime()");
      exit(-1);     
    }
    
    if (current->tm_sec == 0) {
      if (updated == 0) {
	sec -= 60;
	updated = -1;
      }
    } else {
      updated = 0;
    }
    delsec = current->tm_sec - sec;
    delmin = delsec / 60;
    delsec %= 60;
    
    printf("\rPlayer %d round time: %02d:%02d ", playing, delmin, delsec);
    usleep(100);
  }
  
  printf("\n");
  quit_thread = 0;
  
  return NULL;
}

/** Creates a signal mask to block the players from force quiting the game
 * 
 */
void blockSignals() {
  sigset_t set;
  sigfillset(&set);
  if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
    perror("sigprocmask()");
    exit(-1);
  }
}

/** Reorders the cards by suit
 * 
 */
void reorderCardsList(char cards[][4]) {
  char order[] = {'c', 'h', 'd', 's'};
  
  int i = 0, j = 0;
  
  for (; cards[i] != NULL && strcmp(cards[i], "\0") != 0; i++) {
    for (j = i + 1; cards[j] != NULL && strcmp(cards[j], "\0") != 0; j++) {
      int first_index = 0, second_index = 0;
      while(cards[i][2] != order[first_index]) {
	first_index++;
      }
      while(cards[j][2] != order[second_index]) {
	second_index++;
      }
      if (first_index > second_index) {
	char tmp[4];
	strcpy(tmp, cards[j]);
	strcpy(cards[j], cards[i]);
	strcpy(cards[i], tmp);
      }
    }
  }
  
  callReceiveEvent();
  
}

/** calls a thread to write the receive event to the log file
 * 
 */
void callReceiveEvent() {
  print_info_t *print_struct = malloc(sizeof(print_info_t));
  
  strcpy(print_struct->who, shm_ptr->players[player_nr].nickname);
  strcpy(print_struct->what, RECEIVE_CARDS_EVENT);
  
  char curr_hand[LINE_SIZE];
  
  printCardsList(hand, curr_hand);
  
  strcpy(print_struct->result, curr_hand);
  
  pthread_t tidP;
  if ((errno = pthread_create(&tidP, NULL, writeEventToLog, print_struct)) != 0) {
    perror("pthread_create()");
    exit(-1);
  }
}

/** thread that syncronizes the game events and displays all information to the player 
 * 
 */
void *playGame(void *ptr) {
  pthread_t tidP;
  
  
  while (1) {
    pthread_mutex_lock(&(shm_ptr->play_mut));
    system("clear");
    displayRoundInfo();
    
    if (shm_ptr->turn_to_play != player_nr) {
      callTimeThread(shm_ptr->turn_to_play);
      pthread_cond_wait(&(shm_ptr->play_cond_var), &(shm_ptr->play_mut));
      quit_thread = -1;
    }
    else {
      printf("Cards in hand: %d\n", nr_cards_in_hand);
      
      printCardsList(hand, NULL);
      
      if ((errno = pthread_create(&tidP, NULL, playCard, NULL)) != 0) {
	perror("pthread_create()");
	exit(-1);
      }
      
      pthread_cond_wait(&(shm_ptr->play_cond_var), &(shm_ptr->play_mut));
      
      callHandEvent();
      
    }
    
    if (shm_ptr->game_ended) {
      printf("Game has ended!");
	pthread_mutex_unlock(&(shm_ptr->play_mut));
	return NULL;
    }
    pthread_mutex_unlock(&(shm_ptr->play_mut));
  }
  
  
  pthread_mutex_unlock(&(shm_ptr->play_mut));
  
  pthread_join(tidP, NULL);
  
  return NULL;
}

/** Calls the thread that displays the time of the current turn
 * 
 */
void callTimeThread(int playerNr) {
  
  pthread_t tid;
  
  int *oth_player_nr = malloc(sizeof(int));
  *oth_player_nr = playerNr;
  
  if ((errno = pthread_create(&tid, NULL, turnTime, (void*)oth_player_nr)) != 0) {
    perror("pthread_create()");
    exit(-1);
  }
  
}

/** Calls a thread to write the hand event to the log file
 * 
 */
void callHandEvent() {
  
  print_info_t *print_struct = malloc(sizeof(print_info_t));
  
  strcpy(print_struct->who, shm_ptr->players[player_nr].nickname);
  strcpy(print_struct->what, HAND_EVENT);
  
  char curr_hand[LINE_SIZE];
  
  printCardsList(hand, curr_hand);
  
  strcpy(print_struct->result, curr_hand);
  
  pthread_t tidP;
  if ((errno = pthread_create(&tidP, NULL, writeEventToLog, print_struct)) != 0) {
    perror("pthread_create()");
    exit(-1);
  }
}

/** prints the cards to the console or to alloc_str
 * 
 */
void printCardsList(char cards[][4], char *alloc_str) {
  
  char n;
  if (alloc_str != NULL)
    strcpy(alloc_str, "");
  
  char tmp[LINE_SIZE];
  
  int a = 0;
  n = cards[0][2];
  for (; cards[a] != NULL && strcmp(cards[a], "\0") != 0; a++) {
    if (alloc_str == NULL) {
      printf("%s", cards[a]);
    } else {
      sprintf(tmp, "%s", cards[a]);
      strcat(alloc_str, tmp);
    }
    
    if (cards[a + 1] != NULL && strcmp(cards[a + 1], "\0") != 0) {
      if (cards[a+1][2] != n) {
	if (alloc_str == NULL) {
	  printf("/");
	} else {
	  sprintf(tmp,"/");
	  strcat(alloc_str, tmp);
	}
	n = cards[a+1][2];
	
      } else {
	if (alloc_str == NULL) {
	  printf("-");
	} else {
	  sprintf(tmp, "-");
	  strcat(alloc_str, tmp);
	}
      }
    }
  }
  if (alloc_str == NULL) {
    printf("\n");
  }
}

/** Chooses first player
 * 
 */
void randomiseFirstPlayer() {
  srand(time(NULL));
  shm_ptr->turn_to_play = rand() % shm_ptr->nr_players;
  shm_ptr->first_player = shm_ptr->turn_to_play;
}

/** writes the header to the log file
 * 
 */
void *writeHeaderToLog(void *ptr) {
  int log_fd;
  
  
  pthread_mutex_lock(&(shm_ptr->log_mut));
  
  pthread_t tid = pthread_self();
  
  if (pthread_detach(tid) != 0) {
    perror("pthread_detach()");
    exit(-1);
  }
  
  strcpy(log_name, shm_ptr->tables_name);
  strcat(log_name, ".log");
  
  if ((log_fd = open(log_name, O_WRONLY | O_CREAT | O_TRUNC, 0600)) == -1) {
    perror("open()");
    exit(-1);
  }
  char header[LINE_SIZE];
  int nr_chars = sprintf(header, "%-20s|%-20s|%-20s|%-20s", "when", "who", "what", "result");
  write(log_fd, header, nr_chars);
  close(log_fd);
  
  pthread_mutex_unlock(&(shm_ptr->log_mut));
  
  
  free(ptr);
  return NULL;
}

/** writes different events to the log file
 * 
 */
void *writeEventToLog(void *info_ptr) {
  
  
  
  print_info_t *info = (print_info_t *)info_ptr;
  int log_fd;
  
  
  
  pthread_t tid = pthread_self();
  
  if (pthread_detach(tid) != 0) {
    perror("pthread_detach()");
    exit(-1);
  }
  
  pthread_mutex_lock(&(shm_ptr->log_mut));
  
  if ((log_fd = open(log_name, O_WRONLY | O_APPEND, 0600)) == -1) {
    perror("open1()");
    exit(-1);
  }
  
  char header[LINE_SIZE];
  
  char when[LINE_SIZE];
  
  struct tm *delta;
  time_t current_time;
  int year, mon, day, hour, min, sec;
  
  if (time(&current_time) == -1) {
    perror("time()");
    exit(-1);
  }
  
  if ((delta = localtime(&current_time)) == NULL) {
    perror("localtime()");
    exit(-1);  
  }
  
  year = 1900 + delta->tm_year;
  mon = delta->tm_mon;
  day = delta->tm_mday;
  hour = delta->tm_hour;
  min = delta->tm_min;
  sec = delta->tm_sec;
  
  sprintf(when, "%04d-%02d-%02d %02d:%02d:%02d", year, mon, day, hour, min, sec);
  
  
  int nr_chars = sprintf(header, "\n%-20s|%-20s|%-20s|%-20s", when, info->who, info->what, info->result);
  write(log_fd, header, nr_chars);
  close(log_fd);
  
  pthread_mutex_unlock(&(shm_ptr->log_mut));
  
  free(info_ptr);
  return NULL;
}