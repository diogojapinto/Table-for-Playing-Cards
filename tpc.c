/*
 * NOTES (things to add on the short future):
 * 
 * CHANGE TO POSIX THE SHARED MEMORY (but for now this works)
 *
 *  as funcoes que criei estao todas a seguir ao exit_handler.
 *  criei um mutex novo na struct, iniciei na funcao init, e chamei o thread no main
 *  ja da para jogar uma carta, remove a da mao adiciona a a mesa e escreve a round atual e a anterior
 *  tambem atualiza o numero da round e a vez do jogador
 *  Comecei a fazer o contador, falta passar o calculo para um formato legivel, e colocar
 *  o codigo noutro sitio, onde esta so mostra quando o outro jogador termina a jogada.
 *
 */

#include "tpc.h"
//#define CLEAR
 int is_dealer = 0;
 int shmid = 0;
 shared_fields_t *shm_ptr = NULL;
 char own_fifo_path[PATH_MAX];
 int fifo_filedes = -1;
 char hand[NR_CARDS / 2][CHARS_PER_CARD];
 int nr_cards_in_hand = 0;
 int player_nr = 0;
 char table_path[PATH_MAX];

 int main (int argc, char **argv) {
  #ifdef CLEAR
  strcpy(table_path, argv[0]);
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
    printCardsList(cards);
    shuffleDeck();
    randomiseFirstPlayer();
    
    if ((errno = pthread_create(&tid, NULL, giveCards, NULL)) != 0) {
      perror("pthread_create()");
      exit(-1);
    }
  }
  receiveCards();
  reorderCardsList(hand);
  //printCardsList(hand);
  if (is_dealer) {
    pthread_join(tid, NULL);
  }
  
  pthread_t tidG;
  if ((errno = pthread_create(&tidG, NULL, playGame, NULL)) != 0) {
    perror("pthread_create()");
    exit(-1);
  }
  
  pthread_join(tidG, NULL);
  
  #endif
  return 0;
}

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

void initSharedMem(char **args) {
  int shm_fd;
  strcpy(table_path, args[0]);
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

void *giveCards(void *ptr) {
  int card_index = NR_CARDS - 1;
  int player_index = 0;
  int i = 0;
  int nr_players = shm_ptr->nr_players;
  players_info_t *players_ptr = shm_ptr->players;
  int cards_per_player = NR_CARDS / nr_players;
  
  printf("Giving cards\n");
  
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
    
    pthread_mutex_unlock(&(shm_ptr->deal_cards_mut[player_index]));
  }
  
  return NULL;
}

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

void *playCard(void *ptr) {
  char ch;
  char chosen_card[4];
  int cardNumber;
  struct tm *delta;
  time_t current_time;
  int min, sec;
  
  if (time(&current_time) == -1) {
    perror("time()");
    exit(-1);
  }
  
  if ((delta = localtime(&current_time)) == NULL) {
    perror("localtime()");
    exit(-1);  
  }
  
  min = delta->tm_min;
  sec = delta->tm_sec;
  
  printf("Insert card: \n");
  
  int i=0;
  while (1) {
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

  addCardToTable(cardNumber);

  removeCardFromHand(cardNumber);

  updatePlayersTurn();

  return NULL;
}

int searchCard(char chosen_card[4], int i) {

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

void addCardToTable(int cardNumber) {

  pthread_mutex_lock(&(shm_ptr->play_mut));
  
  int i = shm_ptr->round_number + player_nr;
  strcpy(shm_ptr->cards_on_table[i], hand[cardNumber]);
  
  printf("%s\n",shm_ptr->cards_on_table[i] );
  
  pthread_mutex_unlock(&(shm_ptr->play_mut));
  
}

void removeCardFromHand(int cardNumber) {

  int i=cardNumber;
  
  while (i<nr_cards_in_hand - 1) {

    strcpy(hand[i],hand[i+1]);
    i++;
  }
  strcpy(hand[i],"\0");
  nr_cards_in_hand--;
}

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
    printf("\n\n round incremented\n\n");
  }
  
  pthread_cond_broadcast(&(shm_ptr->play_cond_var));
  
  pthread_mutex_unlock(&(shm_ptr->play_mut));
}

void displayRound() {

  int i =0;
  
  if (shm_ptr->round_number != 0) {
    printf("\nLast Round: ");
    while(i<player_nr) {
      printf("%s - ", shm_ptr->cards_on_table[shm_ptr->round_number+i]);
      i++;
    }
    printf("%s\n", shm_ptr->cards_on_table[shm_ptr->round_number+i]);
  }
  
  i=0;
  if (player_nr != 0)
    printf("\nCards this round: ");
  
  while(i<player_nr-1) {
    printf("%s - ", shm_ptr->cards_on_table[shm_ptr->round_number+i]);
    i++;
  }
  printf("%s\n", shm_ptr->cards_on_table[shm_ptr->round_number+i]);
  
}

void turnTime(int playing, int min, int sec) {

  struct tm *current;
  time_t current_time1;
  int delmin, delsec;
  
  if (time(&current_time1) == -1) {
    perror("time()");
    exit(-1);
  }
  
  if ((current = localtime(&current_time1)) == NULL) {
    perror("localtime()");
    exit(-1);     
  }
  
  if (playing != shm_ptr->turn_to_play) {
    playing = shm_ptr->turn_to_play;
    min = current->tm_min;
    sec = current->tm_sec;
    printf("\n");
  }
  
  delmin = current->tm_min - min;
  
  delsec = current->tm_sec - sec;
  
  printf("\rPlayer %d round time: %d:%d", playing, delmin, delsec);
}

void blockSignals() {
  sigset_t set;
  sigfillset(&set);
  if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
    perror("sigprocmask()");
    exit(-1);
  }
}

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
}

void *playGame(void *ptr) {
  pthread_t tidP;


  while (1) {
    pthread_mutex_lock(&(shm_ptr->play_mut));

    if (shm_ptr->turn_to_play != player_nr) {
     displayRound();
     pthread_cond_wait(&(shm_ptr->play_cond_var), &(shm_ptr->play_mut));
   }
   else {
     printf("Cards in hand: %d\n", nr_cards_in_hand);

     printCardsList(hand);

     if ((errno = pthread_create(&tidP, NULL, playCard, NULL)) != 0) {
       perror("pthread_create()");
       exit(-1);
     }

     pthread_cond_wait(&(shm_ptr->play_cond_var), &(shm_ptr->play_mut));
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

void printCardsList(char cards[][4]) {

  char n;

  int a = 0;
  n = cards[0][2];
  for (; cards[a] != NULL && strcmp(cards[a], "\0") != 0; a++) {
    if (cards[a][2] == n) {
     printf("%s", cards[a]);
     if (cards[a + 1] != NULL && strcmp(cards[a + 1], "\0")) {
       if (cards[a+1][2] != n){
         printf("/");
         n = cards[a+1][2];
       }
       else {
         printf("-");
       }
     }
   }
 }
 printf("\n");
}


void randomiseFirstPlayer() {
  srand(time(NULL));
  shm_ptr->turn_to_play = rand() % shm_ptr->nr_players;
  shm_ptr->first_player = shm_ptr->turn_to_play;
}

void *writeEventToLog(char *who, char *what, char *result) {

}