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

struct ThreadInfo {
  unsigned int *thread_id;
  char *input_path;
  char *output_path;
  unsigned int state_access_delay_ms;
};

void *processFile(void *arg) {
  struct ThreadInfo *threadInfo = (struct ThreadInfo *)arg;

  int input_fd = open(threadInfo->input_path, O_RDONLY);
  int output_fd = open(threadInfo->output_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

  int cleanup_required = 0;  // Flag para indicar se a limpeza é necessária

  if (input_fd == -1 || output_fd == -1) {
    write(output_fd, "Failed to open input or output file\n", 36);
    cleanup_required = 1;  // Configurar a flag para indicar que a limpeza é necessária
  }

  if (!cleanup_required && ems_process_command(input_fd, output_fd, threadInfo->thread_id)) {
    const char *error_msg[2];
    error_msg[0] = "Error processing commands for file ";
    error_msg[1] = threadInfo->input_path;
    build_string(output_fd, error_msg, 2);
    cleanup_required = 1;  // Configurar a flag para indicar que a limpeza é necessária
  }

  // Limpeza
  if (input_fd != -1) {
    close(input_fd);
  }
  if (output_fd != -1) {
    close(output_fd);
  }

  pthread_exit(NULL);
}


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

  DIR *dir = opendir(jobs_directory);
  if (!dir) { // Checks if the directory was opened successfully.
    fprintf(stderr, "Failed to open %s directory", jobs_directory);
    return 1;
  }

  pthread_t threads[max_threads];
  struct ThreadInfo *thread_infos[max_threads];
  int thread_count = 0;

  if (ems_init(state_access_delay_ms)) {
    printf("Failed to initialize EMS\n");
    return 1;  
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    struct stat entry_stat;
    if (fstatat(dirfd(dir), entry->d_name, &entry_stat, 0) == 0 && S_ISREG(entry_stat.st_mode) && strstr(entry->d_name, ".jobs") != NULL && thread_count < max_threads) {
      
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

      thread_infos[thread_count] = malloc(sizeof(struct ThreadInfo));
      if (thread_infos[thread_count] == NULL) {
        fprintf(stderr, "Failed to allocate memory for thread info\n");
        return 1;
      }

      thread_infos[thread_count]->input_path = strdup(input_path);
      thread_infos[thread_count]->output_path = strdup(output_path);
      thread_infos[thread_count]->state_access_delay_ms = state_access_delay_ms;
      thread_infos[thread_count]->thread_id = (unsigned int *)(intptr_t)(thread_count+1);

      if (pthread_create(&threads[thread_count], NULL, processFile, (void *)thread_infos[thread_count]) != 0) {
        fprintf(stderr, "Failed to create thread\n");
        free(thread_infos[thread_count]->input_path);
        free(thread_infos[thread_count]->output_path);
        free(thread_infos[thread_count]);
        free(output_file_name);
        return 1;
      }
      free(output_file_name);

      //printf("Thread ID: %lu\n", (unsigned long)threads[thread_count]);
      //printf("Thread ID: %ls\n", thread_infos[thread_count]->thread_id);
      thread_count++;

      /* if (thread_count >= max_threads) {
        pthread_join(threads[thread_count-1], NULL);
        free(thread_infos[thread_count-1]->input_path);
        free(thread_infos[thread_count-1]->output_path);
        free(thread_infos[thread_count-1]);
        thread_count--;
        
      } */
    }
  }

  for (int i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
    free(thread_infos[i]->input_path);
    free(thread_infos[i]->output_path);
    free(thread_infos[i]);
  }

  closedir(dir);
  ems_terminate();
  return 0; 
}


