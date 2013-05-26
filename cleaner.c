#include "headers.h"
int main(int argc, char **argv) {
  system("killall -s SIGKILL tpc");
  char table_path[200];
  strcpy(table_path, dirname(argv[0]));
  strcat(table_path, "/");
  strcat(table_path, argv[1]);
  if (shm_unlink(argv[1]) == -1)
    perror("shm_unlink()");
  
  return 0;
}