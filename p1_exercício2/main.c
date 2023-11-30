#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc != 2 && argc != 3) {
    fprintf(stderr, "Usage: %s <max_processes> [delay]\n", argv[0]);
    return 1;
  }

  int max_processes = atoi(argv[1]);
  if (max_processes <= 0) {
    fprintf(stderr, "Invalid value for max_processes: %s\n", argv[1]);
    return 1;
  }


  if (argc > 3) {
    char *endptr;
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  char *jobs_directory = "jobs";

  process_jobs_directory(jobs_directory, max_processes, state_access_delay_ms);

  return 0;
}

