#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdint.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"


int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc != 4 && argc != 5) {
    fprintf(stderr, "Usage: %s <directory> <max_processes> <max_threads> [delay]\n", argv[0]);
    return 1;
  }

  int max_processes = atoi(argv[2]);
  int max_threads = atoi(argv[3]);
  if (max_processes <= 0) {
    fprintf(stderr, "Invalid value for max_processes: %s\n", argv[2]);
    return 1;
  }
  else if (max_threads <= 0){
    fprintf(stderr, "Invalid value for max_threads: %s\n", argv[3]);
    return 1;
  }
  


  if (argc > 4) {
    char *endptr;
    unsigned long int delay = strtoul(argv[4], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  char *jobs_directory = argv[1];


  process_directory(jobs_directory, max_processes, max_threads, state_access_delay_ms);

  return 0; 
}


