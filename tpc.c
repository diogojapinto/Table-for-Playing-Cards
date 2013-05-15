#include "tpc.h"

int main (int argc, char **argv) {
  
  // EXTRACT EACH CODE PORTION TO EXTERNAL FUNCTION
  
  // verifies if the number of arguments is correct
  if (argc != 4) {
    printf("usage: %s <player's name> <table's name> <nr. players>", argv[0]);
    exit(-1);
  }
  
  // verifies if the 3rd arg is an integer
  char *c;
  for ( c = argv[3]; c != NULL && *c != '\0'; c++) {
    if (!isdigit(*c)) {
      printf("the 3rd argument must me an integer\n");
      exit(-1);
    }
  }
  
  // MISSING VERIFICATION OF SIZE OF OTHER ARGS
  
  key_t shm_key;
  char* exec_path = argv[0];
  
  // obtain an exclusive key for the application
  shm_key = ftok(exec_path, 0);
  
  int shmid = 0;
  
  // verifies if the memory block already exists
  if ((shmid = shmget(shm_key, sizeof(shared_fields_t), IPC_EXCL | SHM_W | SHM_R)) == -1) {
    perror("shmget()");
    exit(-1);
  }
  
  return 0;
}