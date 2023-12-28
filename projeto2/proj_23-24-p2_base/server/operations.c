#include "common/io.h"
#include "eventlist.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_us = 0;

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @param from First node to be searched.
/// @param to Last node to be searched.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id, struct ListNode* from, struct ListNode* to) {
  struct timespec delay = {0, state_access_delay_us * 1000};
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(event_list, event_id, from, to);
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_us) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_us = delay_us;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  free_list(event_list);
  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  if (get_event_with_delay(event_id, event_list->head, event_list->tail) != NULL) {
    fprintf(stderr, "Event already exists\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  if (pthread_mutex_init(&event->mutex, NULL) != 0) {
    pthread_rwlock_unlock(&event_list->rwl);
    free(event);
    return 1;
  }
  event->data = calloc(num_rows * num_cols, sizeof(unsigned int));

  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    pthread_rwlock_unlock(&event_list->rwl);
    free(event);
    return 1;
  }

  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    pthread_rwlock_unlock(&event_list->rwl);
    free(event->data);
    free(event);
    return 1;
  }

  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

  pthread_rwlock_unlock(&event_list->rwl);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  if (pthread_mutex_lock(&event->mutex) != 0) {
    fprintf(stderr, "Error locking mutex\n");
    return 1;
  }

  for (size_t i = 0; i < num_seats; i++) {
    if (xs[i] <= 0 || xs[i] > event->rows || ys[i] <= 0 || ys[i] > event->cols) {
      fprintf(stderr, "Seat out of bounds\n");
      pthread_mutex_unlock(&event->mutex);
      return 1;
    }
  }

  for (size_t i = 0; i < event->rows * event->cols; i++) {
    for (size_t j = 0; j < num_seats; j++) {
      if (seat_index(event, xs[j], ys[j]) != i) {
        continue;
      }

      if (event->data[i] != 0) {
        fprintf(stderr, "Seat already reserved\n");
        pthread_mutex_unlock(&event->mutex);
        return 1;
      }

      break;
    }
  }

  unsigned int reservation_id = ++event->reservations;

  for (size_t i = 0; i < num_seats; i++) {
    event->data[seat_index(event, xs[i], ys[i])] = reservation_id;
    printf("SEAT (%ld, %ld) RESERVED\n", xs[i], ys[i]);
  }

  pthread_mutex_unlock(&event->mutex);
  return 0;
}

int ems_show(int fd_resp, unsigned int event_id) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

  pthread_rwlock_unlock(&event_list->rwl);

  if (event == NULL) {
    int success = 1;
    printf("SUCCESS: %d\n", success);
    ssize_t ret = write(fd_resp, &success, sizeof(int));
    if (ret < 0) {
      fprintf(stdout, "ERR: write failed\n");
      return 1;
    }
    printf("WROTE success\n");

    ret = write(fd_resp, "Event not found\n", strlen("Event not found\n"));
    if (ret < 0) {
      fprintf(stdout, "ERR: write failed\n");
      return 1;
    }
    printf("WROTE event not found\n");
    return 1;
  }

  if (pthread_mutex_lock(&event->mutex) != 0) {
    fprintf(stderr, "Error locking mutex\n");
  }

  printf("ROWS: %ld COLS: %ld\n", event->rows, event->cols);

  unsigned int *seats = malloc(sizeof(unsigned int) * (event->rows*event->cols));

  if (seats == NULL) {
    fprintf(stdout, "ERR: failed to allocate memory\n");
    free(seats);
    pthread_mutex_unlock(&event->mutex);
    return 1;
  }
  int aux = 0;
  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {  
      seats[aux] = event->data[seat_index(event, i, j)];
      aux++;
    }
  }

  int success = 0;
  printf("SUCCESS: %d\n", success);
  ssize_t ret = write(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    free(seats);
    pthread_mutex_unlock(&event->mutex);
    return 1;
  }
  printf("WROTE success\n");

  printf("NUM_ROWS: %ld\n", event->rows);
  ret = write(fd_resp, &event->rows, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    free(seats);
    pthread_mutex_unlock(&event->mutex);
    return 1;
  }
  printf("WROTE num_rows\n");

  printf("NUM_COLS: %ld\n", event->cols);
  ret = write(fd_resp, &event->cols, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    free(seats);
    pthread_mutex_unlock(&event->mutex);
    return 1;
  }
  printf("WROTE num_cols\n");

  ret = write(fd_resp, seats, sizeof(unsigned int[event->rows*event->cols]));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    free(seats);
    pthread_mutex_unlock(&event->mutex);
    return 1;
  }
  printf("WROTE seats\n");
  free(seats);
  pthread_mutex_unlock(&event->mutex);
  return 0;
}

int ems_list_events(int fd_resp, int fd_req, worker_client_t *client) {

  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  struct ListNode* to = event_list->tail;
  struct ListNode* current = event_list->head;

  if (current == NULL) {
    int success = 1;
    printf("SUCCESS: %d\n", success);
    ssize_t ret = write(fd_resp, &success, sizeof(int));
    if (ret < 0) {
      fprintf(stdout, "ERR: write failed\n");
      pthread_rwlock_unlock(&event_list->rwl);
      return 1;
    }
    printf("WROTE success\n");

    ret = write(fd_resp, "No events\n", strlen("No events\n"));
    if (ret < 0) {
      fprintf(stdout, "ERR: write failed\n");
      pthread_rwlock_unlock(&event_list->rwl);
      return 1;
    }
    printf("WROTE No events\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;

  }

  size_t num_events = 0;
  while (1) {
    num_events++;
    if (current == to) {
      break;
    }
    current = current->next;
  }

  current = event_list->head;

  unsigned int ids[num_events];
  for(size_t i = 0; i < num_events; i++) {
    ids[i] = (current->event)->id;
    current = current->next;
  }

  int success = 0;
  printf("SUCCESS: %d\n", success);
  ssize_t ret = write(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }
  printf("WROTE success\n");

  ret = write(fd_resp, &num_events, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }
  printf("WROTE num_events\n");

  ret = write(fd_resp, ids, sizeof(unsigned int[num_events]));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }
  printf("WROTE ids\n");

  char op_code;
  ret = read(fd_req, &op_code, sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  client->opcode = op_code;

  pthread_rwlock_unlock(&event_list->rwl);

  return 0;
}