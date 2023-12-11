#include "constants.h"
#include "operations.h"
#include "parser.h"

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc != 4 && argc != 5) { // Verify if the number of arguments is correct.
    fprintf(stderr, "Usage: %s <directory> <max_processes> <max_threads> [delay]\n", argv[0]);
    return 1;
  }

  int max_processes = atoi(argv[2]);    // Transforms the maximum number of processes from string to integer.
  int max_threads = atoi(argv[3]);      // Transforms the maximum number of threads from string to integer.
  if (max_processes <= 0) {             // Verify if the number of maximum processes is valid.
    fprintf(stderr, "Invalid value for max_processes: %s\n", argv[2]);
    return 1;
  }
  else if (max_threads <= 0){           // Verify if the number of maximum threads is valid.
    fprintf(stderr, "Invalid value for max_threads: %s\n", argv[3]);
    return 1;
  }
  


  if (argc > 4) {
    char *endptr;
    unsigned long int delay = strtoul(argv[4], &endptr, 10); // Reads the delay passed in the command line.

    if (*endptr != '\0' || delay > UINT_MAX) { // Verify if the delay is valid.
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  char *directory = argv[1]; // Reads the directory passed in the command line.


  process_directory(directory, max_processes, max_threads, state_access_delay_ms);

  return 0; 
}


