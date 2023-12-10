#include "constants.h"
#include "eventlist.h"
#include "operations.h"
#include "parser.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_ms = 0;

static pthread_rwlock_t init_mutex = PTHREAD_RWLOCK_INITIALIZER;
static pthread_rwlock_t event_mutex = PTHREAD_RWLOCK_INITIALIZER;
static pthread_rwlock_t write_mutex = PTHREAD_RWLOCK_INITIALIZER;

unsigned int thread_id_wait = (unsigned int)-1;
unsigned int delay_wait;

/// @brief Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

/// @brief Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed
  
  return get_event(event_list, event_id);
}

/// @brief Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
static unsigned int* get_seat_with_delay(struct Event* event, size_t index) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return &event->data[index];
}

/// @brief Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////// MANIPULATION OF EVENTS ////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int ems_init(unsigned int delay_ms) {
  // Write lock for init_mutex.
  pthread_rwlock_wrlock(&init_mutex);

  // Inicializa a estrutura de dados (event_list) apenas uma vez
  event_list = create_list();
  state_access_delay_ms = delay_ms;

  if (event_list == NULL) {
    printf("ERR: Failed to create event_list.\n");
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  // Read/Write unlock for init_mutex.
  pthread_rwlock_unlock(&init_mutex);

  return event_list == NULL;
}

int ems_terminate() {
  // Write lock for init_mutex.
  pthread_rwlock_wrlock(&init_mutex);

  if (event_list == NULL) {
    printf("ERR: EMS state must be initialized.\n");
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  free_list(event_list);
  event_list = NULL;
  state_access_delay_ms = 0;

  // Read/Write unlock for init_mutex.
  pthread_rwlock_unlock(&init_mutex);
  return 0;
}


int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  // Read lock for init_mutex.
  pthread_rwlock_rdlock(&init_mutex);

  if (event_list == NULL) {
    printf("ERR: EMS state must be initialized.\n");
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }
  
  // Write lock for event_mutex.
  pthread_rwlock_wrlock(&event_mutex);
  if (get_event_with_delay(event_id) != NULL) {
    printf("ERR: Event already exists.\n");
    // Read/Write unlock for event_mutex.
    pthread_rwlock_unlock(&event_mutex);
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }
  
  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    printf("ERR: Unable to allocate memory for event.\n");
    // Read/Write unlock for event_mutex.
    pthread_rwlock_unlock(&event_mutex);
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(unsigned int));
  // Write lock initialization for seat_mutex.
  pthread_rwlock_init(&event->seat_mutex, NULL);

  if (event->data == NULL) {
    printf("ERR: Unable to allocate memory for event data.\n");
    free(event);
    // Read/Write unlock for event_mutex.
    pthread_rwlock_unlock(&event_mutex);
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i] = 0;
  }

  if (append_to_list(event_list, event) != 0) {
    printf("ERR: Unable to append event to list.\n");
    free(event->data);
    free(event);
    // Read/Write unlock for event_mutex.
    pthread_rwlock_unlock(&event_mutex);
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  // Read/Write unlock for event_mutex.
  pthread_rwlock_unlock(&event_mutex);
  // Read/Write unlock for init_mutex.
  pthread_rwlock_unlock(&init_mutex);
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  // Read lock for init_mutex.
  pthread_rwlock_rdlock(&init_mutex);

  if (event_list == NULL) {
    printf("ERR: EMS state must be initialized.\n");
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  // Read lock for event_mutex.
  pthread_rwlock_rdlock(&event_mutex);
  struct Event* event = get_event_with_delay(event_id);
  if (event == NULL) {
    printf("ERR: Event not found.\n");
    // Read/Write unlock for event_mutex.
    pthread_rwlock_unlock(&event_mutex);
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }
  // Read/Write unlock for event_mutex.
  pthread_rwlock_unlock(&event_mutex);
  
  // Write lock for seat_mutex.
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

    // Read/Write unlock for seat_mutex.
    pthread_rwlock_unlock(&event->seat_mutex);
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  // Read/Write unlock for seat_mutex.
  pthread_rwlock_unlock(&event->seat_mutex);
  // Read/Write unlock for init_mutex.
  pthread_rwlock_unlock(&init_mutex);
  return 0;
}

int ems_show(unsigned int event_id, int output_fd) {
  // Read lock for init_mutex.
  pthread_rwlock_rdlock(&init_mutex);

  if (event_list == NULL) {
    printf("ERR: EMS state must be initialized.\n");
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  // Read lock for event_mutex.
  pthread_rwlock_rdlock(&event_mutex);
  struct Event* event = get_event_with_delay(event_id);
  
  if (event == NULL) {
    printf("ERR: Event not found.\n");
    // Read/Write unlock for event_mutex.
    pthread_rwlock_unlock(&event_mutex);
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  // Write lock for seat_mutex.
  pthread_rwlock_rdlock(&event->seat_mutex);
  // Write lock for write_mutex.
  pthread_rwlock_wrlock(&write_mutex);
  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      unsigned int* seat = get_seat_with_delay(event, seat_index(event, i, j));

      char seat_str[12];
      // Turns the seat variable from an int into a string.  
      int_to_str(*seat, seat_str);

      const char *seat_msg[1];
      seat_msg[0] = seat_str;
      // Writes the value of the seat in row i and column j in the output file.
      build_string(output_fd, seat_msg, 1);

      if (j < event->cols) {
        write(output_fd, " ", 1);
      }
    }
    write(output_fd, "\n", 1);
  }

  // Read/Write unlock for seat_mutex.
  pthread_rwlock_unlock(&event->seat_mutex);
  // Read/Write unlock for event_mutex.
  pthread_rwlock_unlock(&event_mutex);
  // Read/Write unlock for write_mutex.
  pthread_rwlock_unlock(&write_mutex);
  // Read/Write unlock for init_mutex.
  pthread_rwlock_unlock(&init_mutex);
  return 0;
}

int ems_list_events(int output_fd) {
  // Read lock for init_mutex.
  pthread_rwlock_rdlock(&init_mutex);

  if (event_list == NULL) {
    printf("ERR: EMS state must be initialized.\n");
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 1;
  }

  // Read/Write unlock for event_mutex.
  pthread_rwlock_rdlock(&event_mutex);
  // Read/Write unlock for write_mutex.
  pthread_rwlock_wrlock(&write_mutex);
  if (event_list->head == NULL) {
    write(output_fd, "No events\n", 10);
    // Read/Write unlock for event_mutex.
    pthread_rwlock_unlock(&event_mutex);
    // Read/Write unlock for write_mutex.
    pthread_rwlock_unlock(&write_mutex);
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return 0;
  }

  struct ListNode* current = event_list->head;

  while (current != NULL) {
    char event_id[12];  
    // Turns the ID of the event from an unsigned int into a string.
    int_to_str((current->event)->id, event_id);
    const char *event_msg[3];
    event_msg[0] = "Event: ";
    event_msg[1] = event_id;
    event_msg[2] = "\n";
    // Write the event id in the output file.
    build_string(output_fd, event_msg, 3);
    current = current->next;
  }

  // Read/Write unlock for event_mutex.
  pthread_rwlock_unlock(&event_mutex);
  // Read/Write unlock for write_mutex.
  pthread_rwlock_unlock(&write_mutex);
  // Read/Write unlock for init_mutex.
  pthread_rwlock_unlock(&init_mutex);
  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// PROCESSES ALL THE COMMANDS THAT EXIST IN THE INPUT FILE ///////////////////////////////
////////////////////////////////////////////////// TO THE OUTPUT FILE /////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void *ems_process_command(void *arg) {
  struct ThreadInfo *threadInfo = (struct ThreadInfo *)arg;

  // 'Activation' of the Thread.
  threadInfo->is_active = 1;

  if(threadInfo->invalid_command){ // Verify if the command is valid.
    printf("ERR: Invalid command. See HELP for usage.\n");
    return NULL;
  }

  // Read lock for init_mutex.
  pthread_rwlock_rdlock(&init_mutex);

  if (event_list == NULL) {
    printf("ERR: EMS state must be initialized.\n");
    // Read/Write unlock for init_mutex.
    pthread_rwlock_unlock(&init_mutex);
    return NULL;
  }

  if(thread_id_wait == 0){  // Verify if all the Threads have to wait.
    if (delay_wait > 0) { // Verify if the delay is valid.
      printf("Waiting...\n");
      ems_wait(delay_wait); // Performs the waiting.
      delay_wait = 0; // 
      thread_id_wait = (unsigned int)-1;
    }
  }
  // Verify if it is this specific Thread that has to do the waiting.
  if(thread_id_wait > 0 && threadInfo->thread_id == (int)thread_id_wait){ 
    if (delay_wait > 0) { // Verify if the delay is valid.
      printf("Waiting...\n");
      ems_wait(delay_wait); // Performs the waiting.
      delay_wait = 0;
      thread_id_wait = (unsigned int)-1;
    }
  }

  // Read/Write unlock for init_mutex.
  pthread_rwlock_unlock(&init_mutex);

  switch (threadInfo->command) {
    case CMD_CREATE:
      // Performs and verifies the command CREATE.
      if (ems_create(threadInfo->event_id, threadInfo->num_rows, threadInfo->num_columns)) { 
        printf("ERR: Failed to create event.\n");        
      }
      break;

    case CMD_RESERVE:
      if (threadInfo->num_coords == 0) { // Verify if the number of seats is valid
        printf("ERR: Invalid command. See HELP for usage.\n");        
      }

      // Performs and verifies the command RESERVE.
      if (ems_reserve(threadInfo->event_id, threadInfo->num_coords, threadInfo->coord.xs, threadInfo->coord.ys)) { 
        printf("ERR: Failed to reserve seats.\n");        
      }
      break;

    case CMD_SHOW:
      // Performs and verifies the command SHOW.
      if (ems_show(threadInfo->event_id, threadInfo->output_fd)) {
        printf("ERR: Failed to show event.\n");        
      }
      break;

    case CMD_LIST_EVENTS:
      // Performs and verifies the command LIST.
      if (ems_list_events(threadInfo->output_fd)) {
        printf("ERR: Failed to list events.\n");
      }
      break;

    case CMD_WAIT:
      // The waiting has already been done before the switch case, because it can be
      // for all the Threads and not the one that receive this command.
      break;

    case CMD_INVALID:
      printf("ERR: Invalid command. See HELP for usage.\n");
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
      // 'Desactivation' of the Thread.
      threadInfo->is_active = 0;
      return 0;
  }

  // 'Desactivation' of the Thread.
  threadInfo->is_active = 0;
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
      break;

    case CMD_INVALID:
      threadInfo->invalid_command = 1;        
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

  for(int i = 0; i < max_threads; i++) {
    thread_infos[i] = NULL;
  }

  int thread_count = 0;
  int thread_count_aux = 0;
  int thread_ended;
  int terminate = 0;

  while(!terminate){
    thread_ended = 0;
    thread_infos[thread_count_aux] = malloc(sizeof(struct ThreadInfo));
    if (thread_infos[thread_count_aux] == NULL) {
      printf("ERR: Failed to allocate memory for thread info\n");
      break;
    }
    thread_infos[thread_count_aux]->thread_id = thread_count_aux+1;
    thread_infos[thread_count_aux]->output_fd = output_fd;
    thread_infos[thread_count_aux]->input_fd = input_fd;
    thread_infos[thread_count_aux]->command = get_next(input_fd);
    thread_infos[thread_count_aux]->is_active = 0;
    if(parse_command((void *)thread_infos[thread_count_aux]) != 0)
      terminate = 1;
    
    if (pthread_create(&threads[thread_count_aux], NULL, ems_process_command, (void *)thread_infos[thread_count_aux]) != 0) {
      printf("ERR: Failed to create thread\n");
      thread_infos[thread_count_aux]->is_active = 1;
      break;
    }

    if(thread_infos[thread_count_aux]->barrier){
      thread_ended = 1;
      for (int i = 0; i <= thread_count; i++) {
        pthread_join(threads[i], NULL);
        free(thread_infos[i]);
        thread_infos[i] = NULL;
      }
      thread_count = 0;
      thread_count_aux = 0;
    } else {
      thread_count++;
      thread_count_aux++;
    }

    if(terminate)
      break;

    if(thread_ended && thread_count < max_threads){
      continue;
    }

    while(thread_count == max_threads && !thread_ended){
      for (int i = 0; i < thread_count; i++) {
        if(thread_infos[i]->is_active == 0) {
          thread_count--;
          thread_count_aux = i;
          pthread_join(threads[i],NULL);
          free(thread_infos[i]);
          thread_infos[i] = NULL;
          thread_ended = 1;
          break;
        }
      }
    }
  }
  for (int i = 0; i < max_threads; i++) {
    if(thread_infos[i] != NULL) {
      pthread_join(threads[i], NULL);
      free(thread_infos[i]);
    }
  }
}

