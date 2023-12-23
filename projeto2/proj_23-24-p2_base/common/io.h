#ifndef COMMON_IO_H
#define COMMON_IO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "constants.h"

enum {
    OP_CODE_QUIT = '2',
    OP_CODE_CREATE = '3',
    OP_CODE_RESERVE = '4',
    OP_CODE_SHOW = '5',
    OP_CODE_LIST = '6',
};

/// Parses an unsigned integer from the given file descriptor.
/// @param fd The file descriptor to read from.
/// @param value Pointer to the variable to store the value in.
/// @param next Pointer to the variable to store the next character in.
/// @return 0 if the integer was read successfully, 1 otherwise.
int parse_uint(int fd, unsigned int *value, char *next);

/// Prints an unsigned integer to the given file descriptor.
/// @param fd The file descriptor to write to.
/// @param value The value to write.
/// @return 0 if the integer was written successfully, 1 otherwise.
int print_uint(int fd, unsigned int value);

/// Writes a string to the given file descriptor.
/// @param fd The file descriptor to write to.
/// @param str The string to write.
/// @return 0 if the string was written successfully, 1 otherwise.
int print_str(int fd, const char *str);

//void build_string(char **finalstring, const char **strings, int n_strings, const size_t *string_sizes, size_t msg_size);

void fill_string(const char *input_string, char output_string[PIPENAME_SIZE]);

//void send_msg(int tx, char const *str, size_t message_size);

#endif  // COMMON_IO_H
