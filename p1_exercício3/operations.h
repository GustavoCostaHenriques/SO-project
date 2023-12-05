#ifndef EMS_OPERATIONS_H
#define EMS_OPERATIONS_H

#include <stddef.h>

/// Initializes the EMS state.
/// @param delay_ms State access delay in milliseconds.
/// @return 0 if the EMS state was initialized successfully, 1 otherwise.
int ems_init(unsigned int delay_ms);

/// Destroys the EMS state.
int ems_terminate();

/// Creates a new event with the given id and dimensions.
/// @param event_id Id of the event to be created.
/// @param num_rows Number of rows of the event to be created.
/// @param num_cols Number of columns of the event to be created.
/// @param output_fd File descriptor of the output file.
/// @return 0 if the event was created successfully, 1 otherwise.
int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols, int output_fd);

/// Creates a new reservation for the given event.
/// @param event_id Id of the event to create a reservation for.
/// @param num_seats Number of seats to reserve.
/// @param xs Array of rows of the seats to reserve.
/// @param ys Array of columns of the seats to reserve.
/// @param output_fd File descriptor of the output file.
/// @return 0 if the reservation was created successfully, 1 otherwise.
int ems_reserve(unsigned int event_id, size_t num_seats, size_t *xs, size_t *ys, int output_fd);

/// Prints the given event.
/// @param event_id Id of the event to print.
/// @param output_fd File descriptor of the output file.
/// @return 0 if the event was printed successfully, 1 otherwise.
int ems_show(unsigned int event_id, int output_fd);

/// Prints all the events.
/// @param output_fd File descriptor of the output file.
/// @return 0 if the events were printed successfully, 1 otherwise.
int ems_list_events(int output_fd);

/// Waits for a given amount of time.
/// @param delay_us Delay in milliseconds.
void ems_wait(unsigned int delay_ms);

/// Processes all the comands that exist in the input file
/// to the output file.
/// @param input_fd File descriptor of the input file.
/// @param output_fd File descriptor of the output file.
int ems_process_command(int input_fd, int output_fd, unsigned int *thread_id);

#endif  // EMS_OPERATIONS_H