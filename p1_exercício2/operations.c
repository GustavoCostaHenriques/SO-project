#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "eventlist.h"
#include "constants.h"
#include "parser.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_ms = 0;

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

int ems_init(int output_fd, unsigned int delay_ms) {
  if (event_list != NULL) {
    write(output_fd, "EMS state has already been initialized\n", 39);
    return 1;
  }

  event_list = create_list();
  state_access_delay_ms = delay_ms;

  return event_list == NULL;
}

int ems_terminate(int output_fd) {
  if (event_list == NULL) {
    write(output_fd, "EMS state must be initialized\n", 30);
    return 1;
  }

  free_list(event_list);
  event_list = NULL;
  return 0;
}


int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols, int output_fd) {
  if (event_list == NULL) {
    write(output_fd, "EMS state must be initialized\n", 30);
    return 1;
  }
  
  if (get_event_with_delay(event_id) != NULL) {
    write(output_fd, "Event already exists\n", 21);
    return 1;
  }
  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    write(output_fd, "Error allocating memory for event\n", 34);
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(unsigned int));

  if (event->data == NULL) {
    write(output_fd, "Error allocating memory for event data\n", 39);
    free(event);
    return 1;
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i] = 0;
  }

  if (append_to_list(event_list, event) != 0) {
    write(output_fd, "Error appending event to list\n", 30);
    free(event->data);
    free(event);
    return 1;
  }

  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys, int output_fd) {
  if (event_list == NULL) {
    write(output_fd, "EMS state must be initialized\n", 30);
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id);

  if (event == NULL) {
    write(output_fd, "Event not found\n", 16);
    return 1;
  }

  unsigned int reservation_id = ++event->reservations;

  size_t i = 0;
  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      write(output_fd, "Invalid seat\n", 13);
      break;
    }

    if (*get_seat_with_delay(event, seat_index(event, row, col)) != 0) {
      write(output_fd, "Seat already reserved\n", 22);
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
    return 1;
  }

  return 0;
}

int ems_show(unsigned int event_id, int output_fd) {
  if (event_list == NULL) {
    write(output_fd, "EMS state must be initialized\n", 30);
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id);

  if (event == NULL) {
    write(output_fd, "Event not found\n", 16);
    return 1;
  }

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

  return 0;
}

int ems_list_events(int output_fd) {
  if (event_list == NULL) {
    write(output_fd, "EMS state must be initialized\n", 30);
    return 1;
  }

  if (event_list->head == NULL) {
    write(output_fd, "No events\n", 10);
    return 0;
  }

  struct ListNode* current = event_list->head;

  while (current != NULL) {
    char event_id[12];  
    //Turns the ID of the event from an unsigned int into a string.
    int_to_str((current->event)->id, event_id);
    const char *event_msg[2];
    event_msg[0] = "Event: ";
    event_msg[1] = event_id;
    //Write the event id in the output file.
    build_string(output_fd, event_msg, 2);
    current = current->next;
  }

  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}

int ems_process_command(int input_fd, int output_fd) {
  while (1) {
    unsigned int event_id, delay;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

    switch (get_next(input_fd)) {
      case CMD_CREATE:
        if (parse_create(input_fd, &event_id, &num_rows, &num_columns) != 0) {
          write(output_fd, "Invalid command. See HELP for usage\n", 36);
          continue;
        }
        if (ems_create(event_id, num_rows, num_columns, output_fd)) {
          write(output_fd, "Failed to create event\n", 23);        
        }
        break;

      case CMD_RESERVE:
        num_coords = parse_reserve(input_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

        if (num_coords == 0) {
          write(output_fd, "Invalid command. See HELP for usage\n", 36);        
          continue;
        }

        if (ems_reserve(event_id, num_coords, xs, ys, output_fd)) {
          write(output_fd, "Failed to reserve seats\n", 24);        
        }
        break;

      case CMD_SHOW:
        if (parse_show(input_fd, &event_id) != 0) {
          write(output_fd, "Invalid command. See HELP for usage\n", 36);        
          continue;
        }
        if (ems_show(event_id, output_fd)) {
          write(output_fd, "Failed to show event\n", 21);        
        }
        break;

      case CMD_LIST_EVENTS:
        if (ems_list_events(output_fd)) {
          write(output_fd, "Failed to list events\n", 22);
        }
        break;

      case CMD_WAIT:
        if (parse_wait(input_fd, &delay, NULL) == -1) {
          write(output_fd, "Invalid command. See HELP for usage\n", 36);        
          continue;
        }

        if (delay > 0) {
          write(output_fd, "Waiting...\n", 11);
          ems_wait(delay);
        }
        break;

      case CMD_INVALID:
        write(output_fd, "Invalid command. See HELP for usage\n", 36);        
        break;

      case CMD_HELP:
        write(
          output_fd,
          "Available commands:\n"
          "  CREATE <event_id> <num_rows> <num_columns>\n"
          "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
          "  SHOW <event_id>\n"
          "  LIST\n"
          "  WAIT <delay_ms> [thread_id]\n"
          "  BARRIER\n"
          "  HELP\n", 188);
        break;

      case CMD_BARRIER:
      case CMD_EMPTY:
        break;
      case EOC:
        ems_terminate(output_fd);
        return 0;
    }
  }
}
