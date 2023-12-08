#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "eventlist.h"
#include "constants.h"
#include "parser.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_ms = 0;
static pthread_rwlock_t init_mutex = PTHREAD_RWLOCK_INITIALIZER;
static pthread_rwlock_t event_mutex = PTHREAD_RWLOCK_INITIALIZER;
static pthread_rwlock_t write_mutex = PTHREAD_RWLOCK_INITIALIZER;
static int ems_mutex_initialized = 0;  // Flag para indicar se o mutex foi inicializado
unsigned int thread_id_wait = (unsigned int)-1;
unsigned int delay_wait;

struct Coord {
  size_t xs[MAX_RESERVATION_SIZE];
  size_t ys[MAX_RESERVATION_SIZE];
};

struct ThreadInfo {
  int thread_id;
  unsigned int thread_id_wait;
  unsigned int delay_wait;
  enum Command command;
  int output_fd;
  int input_fd;
  unsigned int event_id;
  size_t num_rows;
  size_t num_columns;
  size_t num_coords;
  struct Coord coord;
  int invalid_command;
  int barrier;
};


/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed
  
  return get_event(event_list, event_id);
}

/// Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
static unsigned int* get_seat_with_delay(struct Event* event, size_t index) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return &event->data[index];
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_ms) {
  pthread_rwlock_wrlock(&init_mutex);

  if (ems_mutex_initialized) {
    pthread_rwlock_unlock(&init_mutex);
    return 0;  // Já inicializado, não é necessário fazer nada
  }

  // Inicializa a estrutura de dados (event_list) apenas uma vez
  event_list = create_list();
  state_access_delay_ms = delay_ms;

  if (event_list == NULL) {
    printf("ERR: Failed to create event_list.\n");
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  ems_mutex_initialized = 1;

  pthread_rwlock_unlock(&init_mutex);

  return event_list == NULL;
}

int ems_terminate() {
  pthread_rwlock_wrlock(&init_mutex);

  if (event_list == NULL) {
    printf("ERR: EMS state must be initialized.\n");
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  if (!ems_mutex_initialized) {
    pthread_rwlock_unlock(&init_mutex);
    return 0;  // Já terminado, não é necessário fazer nada
  }

  free_list(event_list);
  event_list = NULL;
  state_access_delay_ms = 0;

  ems_mutex_initialized = 0;

  pthread_rwlock_unlock(&init_mutex);
  return 0;
}


int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  pthread_rwlock_rdlock(&init_mutex);

  if (!ems_mutex_initialized || event_list == NULL) {
    printf("ERR: EMS state must be initialized.\n");
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }
  
  pthread_rwlock_wrlock(&event_mutex);
  if (get_event_with_delay(event_id) != NULL) {
    printf("ERR: Event already exists.\n");
    pthread_rwlock_unlock(&event_mutex);
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }
  
  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    printf("ERR: Unable to allocate memory for event.\n");
    pthread_rwlock_unlock(&init_mutex);
    pthread_rwlock_unlock(&event_mutex);
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(unsigned int));
  pthread_rwlock_init(&event->seat_mutex, NULL);

  if (event->data == NULL) {
    printf("ERR: Unable to allocate memory for event data.\n");
    free(event);
    pthread_rwlock_unlock(&init_mutex);
      pthread_rwlock_unlock(&event_mutex);
    return 1;
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i] = 0;
  }

  if (append_to_list(event_list, event) != 0) {
    printf("ERR: Unable to append event to list.\n");
    free(event->data);
    free(event);
    pthread_rwlock_unlock(&init_mutex);
    pthread_rwlock_unlock(&event_mutex);
    return 1;
  }
  pthread_rwlock_unlock(&event_mutex);

  pthread_rwlock_unlock(&init_mutex);
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  pthread_rwlock_rdlock(&init_mutex);

  if (!ems_mutex_initialized || event_list == NULL) {
    printf("ERR: EMS state must be initialized.\n");
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  pthread_rwlock_rdlock(&event_mutex);
  struct Event* event = get_event_with_delay(event_id);
  if (event == NULL) {
    printf("ERR: Event not found.\n");
    pthread_rwlock_unlock(&init_mutex);
    pthread_rwlock_unlock(&event_mutex);
    return 1;
  }
  pthread_rwlock_unlock(&event_mutex);
  
  pthread_rwlock_wrlock(&event->seat_mutex);
  unsigned int reservation_id = ++event->reservations;

  size_t i = 0;
  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      printf("ERR: Invalid seat.\n");
      break;
    }

    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0) {
      printf("ERR: Seat already reserved.\n");
      break;
    }

    *get_seat_with_delay(event, seat_index(event, row, col)) = reservation_id;
  }

  // If the reservation was not successful, free the seats that were reserved.
  if (i < num_seats) {
    event->reservations--;
    for (size_t j = 0; j < i; j++) {
      *get_seat_with_delay(event, seat_index(event, xs[j], ys[j])) = 0;
    }
    pthread_rwlock_unlock(&event->seat_mutex);
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }
  pthread_rwlock_unlock(&event->seat_mutex);

  pthread_rwlock_unlock(&init_mutex);
  return 0;
}

int ems_show(unsigned int event_id, int output_fd) {
  pthread_rwlock_rdlock(&init_mutex);

  if (!ems_mutex_initialized || event_list == NULL) {
    printf("ERR: EMS state must be initialized.\n");
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  pthread_rwlock_rdlock(&event_mutex);
  struct Event* event = get_event_with_delay(event_id);
  
  if (event == NULL) {
    printf("ERR: Event not found.\n");
    pthread_rwlock_unlock(&init_mutex);
    pthread_rwlock_unlock(&event_mutex);
    return 1;
  }

  pthread_rwlock_rdlock(&event->seat_mutex);
  pthread_rwlock_wrlock(&write_mutex);
  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      unsigned int* seat = get_seat_with_delay(event, seat_index(event, i, j));

      char seat_str[12];
      //Turns the seat variable from an int into a string.  
      int_to_str(*seat, seat_str);

      const char *seat_msg[1];
      seat_msg[0] = seat_str;
      //Writes the value of the seat in row i and column j in the output file.
      build_string(output_fd, seat_msg, 1);

      if (j < event->cols) {
        write(output_fd, " ", 1);
      }
    }
    write(output_fd, "\n", 1);
  }
  pthread_rwlock_unlock(&event->seat_mutex);
  pthread_rwlock_unlock(&event_mutex);
  pthread_rwlock_unlock(&init_mutex);
  pthread_rwlock_unlock(&write_mutex);
  return 0;
}

int ems_list_events(int output_fd) {
  pthread_rwlock_rdlock(&init_mutex);

  if (!ems_mutex_initialized || event_list == NULL) {
    printf("ERR: EMS state must be initialized.\n");
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  pthread_rwlock_rdlock(&event_mutex);
  if (event_list->head == NULL) {
    printf("No events\n");
    pthread_rwlock_unlock(&event_mutex);
    pthread_rwlock_unlock(&init_mutex);
    return 0;
  }

  struct ListNode* current = event_list->head;

  pthread_rwlock_wrlock(&write_mutex);
  while (current != NULL) {
    //pthread_rwlock_unlock(&event_mutex);
    char event_id[12];  
    //Turns the ID of the event from an unsigned int into a string.
    int_to_str((current->event)->id, event_id);
    const char *event_msg[3];
    event_msg[0] = "Event: ";
    event_msg[1] = event_id;
    event_msg[2] = "\n";
    //Write the event id in the output file.
    build_string(output_fd, event_msg, 3);
    //pthread_rwlock_rdlock(&event_mutex);
    current = current->next;
  }
  pthread_rwlock_unlock(&event_mutex);
  pthread_rwlock_unlock(&write_mutex);
  pthread_rwlock_unlock(&init_mutex);
  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}

void *ems_process_command(void *arg) {
  struct ThreadInfo *threadInfo = (struct ThreadInfo *)arg;
  int thread_id = threadInfo->thread_id;
  int output_fd = threadInfo->output_fd;
  enum Command command = threadInfo->command;
  unsigned int event_id = threadInfo->event_id;
  size_t num_rows = threadInfo->num_rows; 
  size_t num_columns = threadInfo->num_columns;
  size_t num_coords = threadInfo->num_coords;

  if(threadInfo->invalid_command){
    printf("ERR: Invalid command. See HELP for usage.\n");
    return NULL;
  }

  pthread_rwlock_rdlock(&init_mutex);

  if (!ems_mutex_initialized || event_list == NULL) {
    printf("ERR: EMS state must be initialized.\n");
    pthread_rwlock_unlock(&init_mutex);
    return NULL;
  }

  if(thread_id_wait == 0){
    if (delay_wait > 0) {
      printf("Waiting...\n");
      ems_wait(delay_wait);
      delay_wait = 0;
      thread_id_wait = (unsigned int)-1;
    }
  }
  if(thread_id_wait > 0 && thread_id == (int)thread_id_wait){
    if (delay_wait > 0) {
      printf("Waiting...\n");
      ems_wait(delay_wait);
      delay_wait = 0;
      thread_id_wait = (unsigned int)-1;
    }
  }

  pthread_rwlock_unlock(&init_mutex);

  switch (command) {
    case CMD_CREATE:
      if (ems_create(event_id, num_rows, num_columns)) {
        printf("ERR: Failed to create event.\n");        
      }
      break;

    case CMD_RESERVE:
      if (num_coords == 0) {
        printf("ERR: Invalid command. See HELP for usage.\n");        
      }

      if (ems_reserve(event_id, num_coords, threadInfo->coord.xs, threadInfo->coord.ys)) {
        printf("ERR: Failed to reserve seats.\n");        
      }
      break;

    case CMD_SHOW:
      if (ems_show(event_id, output_fd)) {
        printf("ERR: Failed to show event.\n");        
      }
      break;

    case CMD_LIST_EVENTS:
      if (ems_list_events(output_fd)) {
        printf("ERR: Failed to list events.\n");
      }
      break;

    case CMD_WAIT:
      if(thread_id_wait == (unsigned int)-1)
        thread_id_wait = 0;

      if(thread_id_wait == 0) {
        if (delay_wait > 0) {
          printf("Waiting...\n");
          ems_wait(delay_wait);
          delay_wait = 0;
          thread_id_wait = (unsigned int)-1;
        }
      }
      else if(thread_id_wait > 0 && thread_id == (int)thread_id_wait){
        if (delay_wait > 0) {
          printf("Waiting...\n");
          ems_wait(delay_wait);
          delay_wait = 0;
          thread_id_wait = (unsigned int)-1;
        }
      }
      break;

    case CMD_INVALID:
      break;

    case CMD_HELP:
      printf(
        "Available commands:\n"
        "  CREATE <event_id> <num_rows> <num_columns>\n"
        "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
        "  SHOW <event_id>\n"
        "  LIST\n"
        "  WAIT <delay_ms> [thread_id]\n"
        "  BARRIER\n"
        "  HELP\n");
      break;

    case CMD_BARRIER:
      break;
    case CMD_EMPTY:
      break;
    case EOC:
      return 0;
  }
  return 0;
}

int parse_command(void *arg) {
  struct ThreadInfo *threadInfo = (struct ThreadInfo *)arg;
  unsigned int event_id;
  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE];
  size_t ys[MAX_RESERVATION_SIZE];

  threadInfo->invalid_command = 0;
  threadInfo->barrier = 0;

  switch (threadInfo->command) {
    case CMD_CREATE:
      if (parse_create(threadInfo->input_fd, &event_id, &num_rows, &num_columns) != 0) {
        threadInfo->invalid_command = 1;
      }
      threadInfo->event_id = event_id;
      threadInfo->num_rows = num_rows;
      threadInfo->num_columns = num_columns;
      break;

    case CMD_RESERVE:
      num_coords = parse_reserve(threadInfo->input_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);
      
      threadInfo->event_id = event_id;
      threadInfo->num_coords = num_coords;
      struct Coord coord;
      size_t i = 0;
      for(; i < num_coords; i++){
        coord.xs[i] = xs[i];
        coord.ys[i] = ys[i];
      }
      threadInfo->coord = coord;
      break;

    case CMD_SHOW:
      if (parse_show(threadInfo->input_fd, &event_id) != 0) {
        threadInfo->invalid_command = 1;        
      }
      threadInfo->event_id = event_id;
      break;

    case CMD_LIST_EVENTS:
      break;

    case CMD_WAIT:
      if (parse_wait(threadInfo->input_fd, &delay_wait, &thread_id_wait) == -1) {
        threadInfo->invalid_command = 1;        
      }
      threadInfo->delay_wait = delay_wait;
      threadInfo->thread_id_wait = thread_id_wait;
      break;

    case CMD_INVALID:
      break;

    case CMD_HELP:
      break;

    case CMD_BARRIER:
      threadInfo->barrier = 1;
      break;
    case CMD_EMPTY:
      break;
    case EOC:
      return 1;
  }
  return 0;
}

void ems_create_thread(int input_fd, int output_fd, int max_threads) {
  pthread_t threads[max_threads];
  struct ThreadInfo *thread_infos[max_threads];
  int thread_count = 0;
  int terminate = 0;

  while(thread_count < max_threads){
    thread_infos[thread_count] = malloc(sizeof(struct ThreadInfo));
    if (thread_infos[thread_count] == NULL) {
      printf("ERR: Failed to allocate memory for thread info\n");
      return;
    }
    thread_infos[thread_count]->thread_id = thread_count+1;
    thread_infos[thread_count]->output_fd = output_fd;
    thread_infos[thread_count]->input_fd = input_fd;
    thread_infos[thread_count]->command = get_next(input_fd);
    if(parse_command((void *)thread_infos[thread_count]) != 0)
      terminate = 1;
    
    if(!thread_infos[thread_count]->barrier){
      if (pthread_create(&threads[thread_count], NULL, ems_process_command, (void *)thread_infos[thread_count]) != 0) {
        printf("ERR: Failed to create thread\n");
        return;
      }
    }

    if(thread_infos[thread_count]->barrier){
      for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
        free(thread_infos[i]);
      }
      free(thread_infos[thread_count]);
      thread_count = 0;
    } else {
      thread_count++;
    }

    if(thread_count == max_threads){
      for (int i = 0; i < thread_count; i++) {
      pthread_join(threads[i], NULL);
      free(thread_infos[i]);
      }
      thread_count = 0;
    }

    if(terminate)
      break;
  }
  for (int i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
    free(thread_infos[i]);
  }
}

