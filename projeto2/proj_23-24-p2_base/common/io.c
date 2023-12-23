#include "io.h"
#include "constants.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int parse_uint(int fd, unsigned int *value, char *next) {
  char buf[16];

  int i = 0;
  while (1) {
    ssize_t read_bytes = read(fd, buf + i, 1);
    if (read_bytes == -1) {
      return 1;
    } else if (read_bytes == 0) {
      *next = '\0';
      break;
    }

    *next = buf[i];

    if (buf[i] > '9' || buf[i] < '0') {
      buf[i] = '\0';
      break;
    }

    i++;
  }

  unsigned long ul = strtoul(buf, NULL, 10);

  if (ul > UINT_MAX) {
    return 1;
  }

  *value = (unsigned int)ul;

  return 0;
}

int print_uint(int fd, unsigned int value) {
  char buffer[16];
  size_t i = 16;

  for (; value > 0; value /= 10) {
    buffer[--i] = '0' + (char)(value % 10);
  }

  if (i == 16) {
    buffer[--i] = '0';
  }

  while (i < 16) {
    ssize_t written = write(fd, buffer + i, 16 - i);
    if (written == -1) {
      return 1;
    }

    i += (size_t)written;
  }

  return 0;
}

int print_str(int fd, const char *str) {
  size_t len = strlen(str);
  while (len > 0) {
    ssize_t written = write(fd, str, len);
    if (written == -1) {
      return 1;
    }

    str += (size_t)written;
    len -= (size_t)written;
  }

  return 0;
}

/* void build_string(char **finalstring, const char **strings, int n_strings, const size_t *string_sizes, size_t msg_size) {
  size_t acumulator = 0;
  // Alocation of the string.
  *finalstring = (char *)malloc((msg_size+1) * sizeof(char));
  if (*finalstring == NULL) {
    fprintf(stdout, "ERR: Unable to allocate memory.\n");
    return;
  }
  for(int i = 0; i < n_strings; i++) {
    memcpy(*finalstring+acumulator, strings[i], string_sizes[i]);
    acumulator += string_sizes[i];
  }
  finalstring[msg_size] = '\0';
} */

void fill_string(const char *input_string, char output_string[PIPENAME_SIZE]) {
  // Copia a string original para a nova string
  memset(output_string, '\0', PIPENAME_SIZE);
  memcpy(output_string, input_string, strlen(input_string));  

}

/* void send_msg(int tx, char const *str, size_t message_size) {
  ssize_t ret = write(tx, str, message_size);
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    exit(EXIT_FAILURE);
  }
} */