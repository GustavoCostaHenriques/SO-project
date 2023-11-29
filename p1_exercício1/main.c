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

  if (argc > 2) {
    char *endptr;
    unsigned long int delay = strtoul(argv[1], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  

  char *jobs_directory = "jobs";

  DIR *dir = opendir(jobs_directory);
  if (!dir) {
    fprintf(stderr, "Failed to open JOBS directory");
    return 1;
  }


  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    struct stat entry_stat;
    if (fstatat(dirfd(dir), entry->d_name, &entry_stat, 0) == 0 && S_ISREG(entry_stat.st_mode) && strstr(entry->d_name, ".jobs") != NULL) {
      
      char input_path[512], output_path[512]; 
      size_t dir_length = strlen(jobs_directory);
      memcpy(input_path, jobs_directory, dir_length);
      if (input_path[dir_length - 1] != '/') {
          input_path[dir_length] = '/';
      }
      memcpy(input_path + dir_length + 1, entry->d_name, strlen(entry->d_name) + 1); 
      
      char *output_file_name = strdup(entry->d_name);
      char *dot = strrchr(output_file_name, '.');
      if (dot && dot > output_file_name) {
          *dot = '\0';
      }
      memcpy(output_path, jobs_directory, dir_length);
      if (output_path[dir_length - 1] != '/') {
          output_path[dir_length] = '/';
      }
      size_t file_name_length = strlen(output_file_name);
      memcpy(output_path + dir_length + 1, output_file_name, file_name_length);
      memcpy(output_path + dir_length + 1 + file_name_length, ".out", 5);

      int input_fd = open(input_path, O_RDONLY);
      int output_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

      if (ems_init(output_fd, state_access_delay_ms)) {
        write(output_fd, "Failed to initialize EMS\n", 25);
        return 1;
      }

      if (input_fd == -1 || output_fd == -1) {
        write(output_fd, "Failed to open input or output file\n", 36);
        return 1;
      }

      if (ems_process_command(input_fd, output_fd)) {
        const char *error_msg[2];
        error_msg[0] = "Error processing commands for file ";
        error_msg[1] = entry->d_name;
        build_string(output_fd,error_msg,2);
        return 1;
      }

      close(input_fd);
      close(output_fd);
      free(output_file_name);
    }
  }

  closedir(dir);
  return 0; 
}

