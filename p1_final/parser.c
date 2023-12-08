#include "parser.h"
#include "operations.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include "constants.h"

static int read_uint(int fd, unsigned int *value, char *next) {
  char buf[16];

  int i = 0;
  while (1) {
    if (read(fd, buf + i, 1) == 0) {
      *next = '\0';
      break;
    }

    *next = buf[i];

    if (buf[i] > '9' || buf[i] < '0') {
      buf[i] = '\0';
      break;
    }

    i++;
  }

  unsigned long ul = strtoul(buf, NULL, 10);

  if (ul > UINT_MAX) {
    return 1;
  }

  *value = (unsigned int)ul;

  return 0;
}

static void cleanup(int fd) {
  char ch;
  while (read(fd, &ch, 1) == 1 && ch != '\n')
    ;
}

enum Command get_next(int fd) {
  char buf[16];
  if (read(fd, buf, 1) != 1) {
    return EOC;
  }

  switch (buf[0]) {
    case 'C':
      if (read(fd, buf + 1, 6) != 6 || strncmp(buf, "CREATE ", 7) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_CREATE;

    case 'R':
      if (read(fd, buf + 1, 7) != 7 || strncmp(buf, "RESERVE ", 8) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_RESERVE;

    case 'S':
      if (read(fd, buf + 1, 4) != 4 || strncmp(buf, "SHOW ", 5) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_SHOW;

    case 'L':
      if (read(fd, buf + 1, 3) != 3 || strncmp(buf, "LIST", 4) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      if (read(fd, buf + 4, 1) != 0 && buf[4] != '\n') {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_LIST_EVENTS;

    case 'B':
      if (read(fd, buf + 1, 6) != 6 || strncmp(buf, "BARRIER", 7) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      if (read(fd, buf + 7, 1) != 0 && buf[7] != '\n') {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_BARRIER;

    case 'W':
      if (read(fd, buf + 1, 4) != 4 || strncmp(buf, "WAIT ", 5) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_WAIT;

    case 'H':
      if (read(fd, buf + 1, 3) != 3 || strncmp(buf, "HELP", 4) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      if (read(fd, buf + 4, 1) != 0 && buf[4] != '\n') {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_HELP;

    case '#':
      cleanup(fd);
      return CMD_EMPTY;

    case '\n':
      return CMD_EMPTY;

    default:
      cleanup(fd);
      return CMD_INVALID;
  }
}

int parse_create(int fd, unsigned int *event_id, size_t *num_rows, size_t *num_cols) {
  char ch;

  if (read_uint(fd, event_id, &ch) != 0 || ch != ' ') {
    cleanup(fd);
    return 1;
  }

  unsigned int u_num_rows;
  if (read_uint(fd, &u_num_rows, &ch) != 0 || ch != ' ') {
    cleanup(fd);
    return 1;
  }
  *num_rows = (size_t)u_num_rows;

  unsigned int u_num_cols;
  if (read_uint(fd, &u_num_cols, &ch) != 0 || (ch != '\n' && ch != '\0')) {
    cleanup(fd);
    return 1;
  }
  *num_cols = (size_t)u_num_cols;

  return 0;
}

size_t parse_reserve(int fd, size_t max, unsigned int *event_id, size_t *xs, size_t *ys) {
  char ch;

  if (read_uint(fd, event_id, &ch) != 0 || ch != ' ') {
    cleanup(fd);
    return 0;
  }

  if (read(fd, &ch, 1) != 1 || ch != '[') {
    cleanup(fd);
    return 0;
  }

  size_t num_coords = 0;
  while (num_coords < max) {
    if (read(fd, &ch, 1) != 1 || ch != '(') {
      cleanup(fd);
      return 0;
    }

    unsigned int x;
    if (read_uint(fd, &x, &ch) != 0 || ch != ',') {
      cleanup(fd);
      return 0;
    }
    xs[num_coords] = (size_t)x;

    unsigned int y;
    if (read_uint(fd, &y, &ch) != 0 || ch != ')') {
      cleanup(fd);
      return 0;
    }
    ys[num_coords] = (size_t)y;

    num_coords++;

    if (read(fd, &ch, 1) != 1 || (ch != ' ' && ch != ']')) {
      cleanup(fd);
      return 0;
    }

    if (ch == ']') {
      break;
    }
  }

  if (num_coords == max) {
    cleanup(fd);
    return 0;
  }

  if (read(fd, &ch, 1) != 1 || (ch != '\n' && ch != '\0')) {
    cleanup(fd);
    return 0;
  }

  return num_coords;
}

int parse_show(int fd, unsigned int *event_id) {
  char ch;

  if (read_uint(fd, event_id, &ch) != 0 || (ch != '\n' && ch != '\0')) {
    cleanup(fd);
    return 1;
  }

  return 0;
}

int parse_wait(int fd, unsigned int *delay, unsigned int *thread_id) {
  char ch;

  if (read_uint(fd, delay, &ch) != 0) {
    cleanup(fd);
    return -1;
  }

  if (ch == ' ') {
    if (thread_id == NULL) {
      cleanup(fd);
      return 0;
    }

    if (read_uint(fd, thread_id, &ch) != 0 || (ch != '\n' && ch != '\0')) {
      cleanup(fd);
      return -1;
    }

    return 1;
  } else if (ch == '\n' || ch == '\0') {
    return 0;
  } else {
    cleanup(fd);
    return -1;
  }

}

/*---------------------------------Functions for PIDList manipulation--------------------------------------------*/


void init_pid_list(PIDList *list, size_t capacity) {
  list->pids = malloc(capacity * sizeof(pid_t)); // Allocate memory for the PID array.
  list->size = 0;                                // Set the initial size to 0.
  list->capacity = capacity;                     // Set the initial capacity.
}

void add_pid(PIDList *list, pid_t pid) {
  if (list->size == list->capacity) {
      //If the list is full, double its capacity.
      list->capacity *= 2;
      list->pids = realloc(list->pids, list->capacity * sizeof(pid_t));
  }
  list->pids[list->size++] = pid; // Add the new PID to the list and increment the size.
}

void remove_pid(PIDList *list, pid_t pid) {
  for (size_t i = 0; i < list->size; i++) {
    if (list->pids[i] == pid) {
      // Move the remaining elements to fill the gap.
      for (size_t j = i; j < list->size - 1; j++) {
        list->pids[j] = list->pids[j + 1];
      }
      list->size--;    // Decrease the size of the list.
      return;          // No need to continue the loop as we found and removed the PID.
    }
  }
}

void free_pid_list(PIDList *list) {
  free(list->pids);    // Free the memory used for storing PIDs.
  list->size = 0;      // Reset the size to 0.
  list->capacity = 0;  // Reset the capacity to 0.
}


/*-----------------------------------------Auxiliary Functions---------------------------------------------------*/

void build_string(int output_fd, const char **strings, int n_strings) {
  size_t total_length = 0;
  size_t acumulator = 0;
  // Calculation of the total length of the string to write in the file with the file descriptor output_fd.
  for (int i = 0; i < n_strings; ++i) {
    total_length += strlen(strings[i]);
  }
  total_length += (size_t) n_strings; // Count spaces between strings plus the \n.

  // Alocation of the string.
  char *result = (char *)malloc(total_length * sizeof(char));
  if (result == NULL) {
    write(output_fd, "ERR: Unable to allocate memory.\n", 32);
    return;
  }


  // Fill the result string with the provided strings.
  for (int i = 0; i < n_strings; ++i) {
    memcpy(result+acumulator, strings[i], strlen(strings[i]));
    acumulator += strlen(strings[i]);
  }

  memcpy(result+acumulator, "\n", 1);
  // Write the result string to the file using the file descriptor output_fd.
  write(output_fd, result, acumulator);

  // Free the memory allocated for the result string.
  free(result);
}

void int_to_str(unsigned int value, char *str) {
  // Dealing with the special case zero.
  if (value == 0) {
      str[0] = '0';
      str[1] = '\0';
      return;
  }

  // Convertion of each individual digit.
  int index = 0;
  while (value > 0) {
    // Use of unsigned int to avoid signal conversion warnings.
    unsigned int digit = value % 10;
    str[index++] = (char)(digit + '0');
    value /= 10;
  }

    // Invertion of a string.
    int start = 0;
    int end = index - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    // Add null terminator.
    str[index] = '\0';
}

size_t count_files(const char *directory){
  DIR *dir;
  size_t number_of_files = 0;

  // Open the directory.
  dir = opendir(directory);
  if (dir == NULL) {
      printf("ERR: Failed to open directory '%s'.\n",directory);
      return (size_t)-1;
  }

  struct dirent *entry;
  // Process each entry in the directory.
  while ((entry = readdir(dir)) != NULL) {
    char filename[256];  
    // Construct the full path for the current file.
    strcpy(filename, directory);
    strcat(filename, "/");
    strcat(filename, entry->d_name);
    struct stat file_stat;
    // Retrieve file information.
    if (stat(filename, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
      // Check if it is a regular file and if the extension is .jobs.
      if (strcmp(strrchr(entry->d_name, '.'), ".jobs") == 0) 
        number_of_files++;
    }
  }

  // Close the directory.
  closedir(dir);
  return number_of_files;
}

void process_file(const char *filename, PIDList *active_children, int max_threads, unsigned int delay_ms) {
  // Open the file for reading.
  int fd = open(filename, O_RDONLY);
  if (fd == -1) {
    printf("ERR: Unable to open file.\n");
    return;
  }

  // Create the output file name by appending ".out" to the input file name.
  size_t filename_len = strlen(filename);
  char *out_filename = malloc(filename_len + 5);  // ".out" has 4 characters plus one for '/' and another for '\0'.
  if (out_filename == NULL) {
    printf("ERR: Unable to allocate memory.\n");
    close(fd);
    return;
  }

  // Find the last occurrence of '.' in the filename.
  const char *dot = strrchr(filename, '.');

  if (dot != NULL) {
    // Determine the length of the prefix before the '.' in the filename.
    size_t prefix_len = strlen(filename) - strlen(dot);
    // Copy the prefix to the out_filename.
    strncpy(out_filename, filename, prefix_len);
    // Add null terminator to the out_filename.
    out_filename[prefix_len] = '\0';
  } else {
    return;
  }

  // Append ".out" to the out_filename.
  strcat(out_filename, ".out");

  // Open the output file for writing.
  int out_fd = open(out_filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (out_fd == -1) {
    free(out_filename);
    printf("ERR: Unable to creat output file.\n");
    close(fd);
    return;
  }

  if (ems_init(delay_ms)) {
    printf("Failed to initialize EMS\n");
    return;  
  }

  // Fork a new process.
  pid_t pid = fork();
  if (pid == -1) {
    // Failed to fork a new process.
    printf("ERR: Unable to fork process.\n");
    free(out_filename);
    free_pid_list(active_children);
    close(fd);
    close(out_fd);
    return;
  } else if (pid == 0) {
    // Child process code.
    // Initialize EMS.
    // Process commands using EMS.
    ems_create_thread(fd, out_fd, max_threads);
    // Cleanup and exit child process successfully.
    free(out_filename);
    close(fd);
    close(out_fd);
    exit(EXIT_SUCCESS);
  } else {
    // Parent process code.
    // Cleanup and close file descriptors in the parent process.
    free(out_filename);
    close(fd);
    close(out_fd);
    // Add the PID of the child process to the active_children list.
    add_pid(active_children, pid);
  }
  ems_terminate();
}

void process_directory(const char *directory, int max_processes, int max_threads, unsigned int delay_ms) {
  DIR *dir;

  // Open the directory.
  dir = opendir(directory);
  if (dir == NULL) {
    printf("ERR: Failed to open directory '%s.'\n",directory);
    return;
  }

  struct dirent *entry;
  size_t number_of_files;
  number_of_files = count_files(directory);

  // List to store PIDs of child processes.
  PIDList active_children;
  init_pid_list(&active_children, number_of_files);

  // Process each entry in the directory.
  while ((entry = readdir(dir)) != NULL) {
    char filename[256];  
    strcpy(filename, directory);
    strcat(filename, "/");
    strcat(filename, entry->d_name);
    struct stat file_stat;
    if (stat(filename, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
      // Check if it's a regular file and if the extension is .jobs.
      if (strcmp(strrchr(entry->d_name, '.'), ".jobs") == 0) {
        // Process the file in parallel
        if (active_children.size < (size_t)max_processes) {
          process_file(filename, &active_children, max_threads, delay_ms);
        } 
        else {
          // Wait for at least one child process to finish before starting a new one.
          int status;
          pid_t child_pid = waitpid(-1, &status, 0);
          if (WIFEXITED(status)) {
            printf("Processo filho %d terminou normalmente com código de saída %d.\n", child_pid, WEXITSTATUS(status));
          } else if (WIFSIGNALED(status)) {
            printf("Processo filho %d terminou devido a um sinal com código %d.\n", child_pid, WTERMSIG(status));
          }
          // Remove the finished child process from the list of active children PIDs.
          remove_pid(&active_children, child_pid);

          process_file(filename, &active_children, max_threads, delay_ms);
        }
      }
    }
  }

  // Wait for the remaining child processes.
  for (size_t i = 0; i < (size_t)max_processes; i++) {
    int status;
    // Check if all files have been processed.
    if(i >= number_of_files) break;
    pid_t child_pid = waitpid(-1, &status, 0);
    if (WIFEXITED(status)) {
        printf("Processo filho %d terminou normalmente com código de saída %d.\n", child_pid, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      printf("Processo filho %d terminou devido a um sinal com código %d.\n", child_pid, WTERMSIG(status));
    }
    remove_pid(&active_children, child_pid);
  }

  // Free memory allocated for PID list.
  free_pid_list(&active_children);
  // Close the directory.
  closedir(dir);
}