#ifndef EMS_PARSER_H
#define EMS_PARSER_H

#include "constants.h"

enum Command {
  CMD_CREATE,          // CREATE command.
  CMD_RESERVE,         // RESERVE command.
  CMD_SHOW,            // SHOW command.
  CMD_LIST_EVENTS,     // LIST_EVENTS command.
  CMD_BARRIER,         // BARRIER command.
  CMD_WAIT,            // WAIT command.
  CMD_HELP,            // HELP command.
  CMD_EMPTY,           // Empty command.
  CMD_INVALID,         // Invalid command.
  EOC                  // End of commands.
};

// Structure to store a list of PIDs.
typedef struct {
    pid_t *pids;       // Array of PIDs.
    size_t size;       // Current size of the list.
    size_t capacity;   // Maximum capacity of the list.
} PIDList;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// FUNCTIONS FOR SYNTAX ANALYSIS AND COMMAND MANIPULATION //////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief a line and returns the corresponding command.
/// @param fd File descriptor to read from.
/// @return The command read.
enum Command get_next(int fd);

/// @brief Parses a CREATE command.
/// @param fd File descriptor to read from.
/// @param event_id Pointer to the variable to store the event ID in.
/// @param num_rows Pointer to the variable to store the number of rows in.
/// @param num_cols Pointer to the variable to store the number of columns in.
/// @return 0 if the command was parsed successfully, 1 otherwise.
int parse_create(int fd, unsigned int *event_id, size_t *num_rows, size_t *num_cols);

/// @brief Parses a RESERVE command.
/// @param fd File descriptor to read from.
/// @param max Maximum number of coordinates to read.
/// @param event_id Pointer to the variable to store the event ID in.
/// @param xs Pointer to the array to store the X coordinates in.
/// @param ys Pointer to the array to store the Y coordinates in.
/// @return Number of coordinates read. 0 on failure.
size_t parse_reserve(int fd, size_t max, unsigned int *event_id, size_t *xs, size_t *ys);

/// @brief Parses a SHOW command.
/// @param fd File descriptor to read from.
/// @param event_id Pointer to the variable to store the event ID in.
/// @return 0 if the command was parsed successfully, 1 otherwise.
int parse_show(int fd, unsigned int *event_id);

/// @brief Parses a WAIT command.
/// @param fd File descriptor to read from.
/// @param delay Pointer to the variable to store the wait delay in.
/// @param thread_id Pointer to the variable to store the thread ID in. May not be set.
/// @return 0 if no thread was specified, 1 if a thread was specified, -1 on error.
int parse_wait(int fd, unsigned int *delay, unsigned int *thread_id);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////// FUNCTIONS FOR PIDList MANIPULATION /////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief Initialize the PIDList with the given capacity.
/// @param list Pointer to the PIDList.
/// @param capacity Initial capacity of the list.
void init_pid_list(PIDList *list, size_t capacity);

/// @brief Add a PID to the PIDList.
/// @param list Pointer to the PIDList.
/// @param pid PID to be added.
void add_pid(PIDList *list, pid_t pid);

/// @brief Remove a PID from the PIDList by shifting elements to fill the gap.
/// @param list Pointer to the PIDList.
/// @param pid PID to be removed.
void remove_pid(PIDList *list, pid_t pid);

/// @brief Free the memory allocated for the PIDList.
/// @param list Pointer to the PIDList.
void free_pid_list(PIDList *list);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////// AUXILIARY FUNCTIONS /////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief Build a string from an array of strings and write it to a file.
/// @param output_fd File descriptor to write to.
/// @param strings Array of strings to build the string.
/// @param n_strings Number of strings in the array.
void build_string(int output_fd, const char **strings, int n_strings);

/// @brief Convert an unsigned integer value to a string.
/// @param value Unsigned integer value to be converted.
/// @param str Pointer to the resulting string.
void int_to_str(unsigned int value, char *str);

/// @brief Count the number of files with the ".jobs" extension in a directory.
/// @param directory Path of the directory.
/// @return Number of files with the ".jobs" extension.
size_t count_files(const char *directory);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// FUNCTIONS TO PROCESS FILES IN PARALLEL //////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief Processes a file, executing each line in parallel.
/// @param filename Name of the file to be processed.
/// @param delay_ms Delay in milliseconds between commands.
/// @param active_children List of PIDs of active processes.
/// @param max_threads Maximum number of threads in parallel for the same file.
void process_file(const char *filename, PIDList *active_children, int max_threads, unsigned int delay_ms);

/// @brief Process the files in the directory, executing commands in parallel.
/// @param directory Path of the directory.
/// @param max_processes Maximum number of processes in parallel.
/// @param max_threads Maximum number of threads in parallel for the same file.
/// @param delay_ms Delay in milliseconds.
void process_directory(const char *directory, int max_processes, int max_threads, unsigned int delay_ms);

#endif  // EMS_PARSER_H
