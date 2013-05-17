#include "tpc.h"

int is_dealer = 0;
int shmid = 0;
shared_fields_t *shm_ptr = NULL;
char own_fifo_path[PATH_MAX];
int fifo_filedes = -1;
char hand[NR_CARDS / 2][CHARS_PER_CARD];

int main (int argc, char **argv) {
  
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
  
  initDefaultDeck();
  shuffleDeck();
  
  initSharedMem(argv);
  
  initFIFO(argv[1]);
  
  if (is_dealer) {
    pthread_t tid;
    if ((errno = pthread_create(&tid, NULL, giveCards, NULL)) != 0) {
      perror("pthread_create()");
      exit(-1);
    }
  } {
    
  }
  
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
  
  if (strlen(argv[1]) > MAX_NICK_LENGTH) {
    printf("Invalid nickname dimension.\n");
    return -1;
  }
  
  int nr_players = atoi(argv[3]);
  if (nr_players > MAX_NR_PLAYERS || nr_players < MIN_NR_PLAYERS) {
    printf("Invalid number of players.\n");
    return -1;
  }
  
  // if successfull:
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
  key_t shm_key;
  char* exec_path = args[0];
  
  // obtain an exclusive key for the application
  shm_key = ftok(exec_path, 0);
  
  // tries to create the shared block of memory
  if ((shmid = shmget(shm_key, sizeof(shared_fields_t), IPC_CREAT | IPC_EXCL | SHM_W | SHM_R)) == -1) {
    // if it wasn't successfull, test if it already existed
    if ((shmid = shmget(shm_key, 0, 0)) == -1) {
      perror("shmget()");
      exit(-1);
    }
    is_dealer = 0;
  } else {
    is_dealer = -1;
  }
  
  // attack this process to the shared memory block
  if ((shm_ptr = (shared_fields_t *) shmat(shmid, NULL, 0)) == (void *) -1) {
    perror("shmat()");
    exit(-1);
  }
  
  if (is_dealer) {
    shm_ptr->nr_players = atoi(args[3]);
    shm_ptr->dealer = 0;
    shm_ptr->last_loggedin_player = 0;
    shm_ptr->turn_to_play = 0;
    shm_ptr->round_number = 0;
    shm_ptr->players[0].number = 0;
    strcpy(shm_ptr->players[0].nickname, args[1]);
    strcpy(shm_ptr->players[0].fifo_path, own_fifo_path);
  } else {
    if (atoi(args[3]) != shm_ptr->nr_players) {
      printf("Number of players different from the one saved!\n");
      exit(-1);
    }
    int player_nr = ++(shm_ptr->last_loggedin_player);
    shm_ptr->players[player_nr].number = player_nr;
    strcpy(shm_ptr->players[player_nr].nickname, args[1]);
    strcpy(shm_ptr->players[player_nr].fifo_path, own_fifo_path);
  }
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
  return NULL;
}

void receiveCards() {
  
}

void exitHandler(void) {
  
  // closes and deletes the own fifo
  close(fifo_filedes);
  unlink(own_fifo_path);
  
  // deatach from shared memory block
  shmdt(shm_ptr);
  
  // by convention, the dealer frees the shared memory block
  if (is_dealer) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
      perror("shmctl()");
    }
  }
}