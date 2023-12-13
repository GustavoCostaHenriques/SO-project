#ifndef EMS_OPERATIONS_H
#define EMS_OPERATIONS_H

#include "constants.h"
#include "parser.h"

// Struct to store all the seats made in a reservation from one thread.
struct Coord { 
  size_t xs[MAX_RESERVATION_SIZE];  // All the X's coordenates.
  size_t ys[MAX_RESERVATION_SIZE];  // All the Y's coordenates.
};

// Struct to store all the information that is necessary to a thread.
struct ThreadInfo {
    int thread_id;                  // Thread ID.
    enum Command command;           // Instruction to know what function the thread will perform.
    int output_fd;                  // Output file descriptor.
    int input_fd;                   // Input file descriptor.
    int invalid_command;            // Bollean to know if the command is valid.
    int barrier;                    // Boolean to know if the commad line is BARRIER.
    int is_active;                  // Boolean to know if the thread is active.
    unsigned int event_id;          // COMMAND CREATE/RESERVE/SHOW: Event ID.
    size_t num_rows;                // COMMAND CREATE: Number of rows of the event that is being created.
    size_t num_columns;             // COMMAND CREATE: Number of columns of the event that is being created.
    size_t num_coords;              // COMMAND RESERVE: Number of seats that are being reserved.
    struct Coord coord;             // COMMAND SHOW: Struct to store all the seats made in a reservation
    int has_to_wait;                // COMMAND WAIT: Boolean to know if the thread has to wait.
    unsigned int thread_id_wait;    // COMMAND WAIT: Integer to know which thread has to wait.
    unsigned int delay;             // COMMAND WAIT: Integer to know how long the thread has to wait.
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////// MANIPULATION OF EVENTS ////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/// @brief Initializes the EMS state.
/// @param delay_ms State access delay in milliseconds.
/// @return 0 if the EMS state was initialized successfully, 1 otherwise.
int ems_init(unsigned int delay_ms);

/// @brief Destroys the EMS state.
/// @return 0 if the EMS state was terminated successfully, 1 otherwise.
int ems_terminate();

/// @brief Creates a new event with the given id and dimensions.
/// @param event_id Id of the event to be created.
/// @param num_rows Number of rows of the event to be created.
/// @param num_cols Number of columns of the event to be created.
/// @param output_fd File descriptor of the output file.
/// @return 0 if the event was created successfully, 1 otherwise.
int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols);

/// @brief Creates a new reservation for the given event.
/// @param event_id Id of the event to create a reservation for.
/// @param num_seats Number of seats to reserve.
/// @param xs Array of rows of the seats to reserve.
/// @param ys Array of columns of the seats to reserve.
/// @param output_fd File descriptor of the output file.
/// @return 0 if the reservation was created successfully, 1 otherwise.
int ems_reserve(unsigned int event_id, size_t num_seats, size_t *xs, size_t *ys);

/// @brief Prints the given event.
/// @param event_id Id of the event to print.
/// @param output_fd File descriptor of the output file.
/// @return 0 if the event was printed successfully, 1 otherwise.
int ems_show(unsigned int event_id, int output_fd);

/// @brief Prints all the events.
/// @param output_fd File descriptor of the output file.
/// @return 0 if the events were printed successfully, 1 otherwise.
int ems_list_events(int output_fd);

/// @brief Waits for a given amount of time.
/// @param delay_us Delay in milliseconds.
void ems_wait(unsigned int delay_ms);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// PROCESSES ALL THE COMMANDS THAT EXIST IN THE INPUT FILE ///////////////////////////////
////////////////////////////////////////////////// TO THE OUTPUT FILE /////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief Process of a line by one Thread.
/// @param arg All the arguments that the struct ThreadInfo contains for this specific Thread.
void *ems_process_command(void *arg);

/// @brief Parses each line and stores the needed variables to the struct ThreadInfo for this specific Thread.
/// @param arg All the arguments that the struct ThreadInfo will receive for this specific Thread.
/// @return 1 if the command is EOC, 0 otherwise.
int parse_command(void *arg);

/// @brief Creation of all necessary threads to process the input file.
/// @param input_fd File descriptor of the input file
/// @param output_fd File descriptor of the output file.
/// @param max_threads Maximum number of threads in parallel for the same file.
void ems_create_thread(int input_fd, int output_fd, int max_threads);


#endif  // EMS_OPERATIONS_H