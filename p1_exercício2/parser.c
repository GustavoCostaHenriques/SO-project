#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>


#include "constants.h"
#include "operations.h"

static int read_uint(int fd, unsigned int *value, char *next) {
  char buf[16];

  int i = 0;
  while (1) {
    if (read(fd, buf + i, 1) == 0) {
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

static void cleanup(int fd) {
  char ch;
  while (read(fd, &ch, 1) == 1 && ch != '\n')
    ;
}

enum Command get_next(int fd) {
  char buf[16];
  if (read(fd, buf, 1) != 1) {
    return EOC;
  }

  switch (buf[0]) {
    case 'C':
      if (read(fd, buf + 1, 6) != 6 || strncmp(buf, "CREATE ", 7) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_CREATE;

    case 'R':
      if (read(fd, buf + 1, 7) != 7 || strncmp(buf, "RESERVE ", 8) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_RESERVE;

    case 'S':
      if (read(fd, buf + 1, 4) != 4 || strncmp(buf, "SHOW ", 5) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_SHOW;

    case 'L':
      if (read(fd, buf + 1, 3) != 3 || strncmp(buf, "LIST", 4) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      if (read(fd, buf + 4, 1) != 0 && buf[4] != '\n') {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_LIST_EVENTS;

    case 'B':
      if (read(fd, buf + 1, 6) != 6 || strncmp(buf, "BARRIER", 7) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      if (read(fd, buf + 7, 1) != 0 && buf[7] != '\n') {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_BARRIER;

    case 'W':
      if (read(fd, buf + 1, 4) != 4 || strncmp(buf, "WAIT ", 5) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_WAIT;

    case 'H':
      if (read(fd, buf + 1, 3) != 3 || strncmp(buf, "HELP", 4) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }

      if (read(fd, buf + 4, 1) != 0 && buf[4] != '\n') {
        cleanup(fd);
        return CMD_INVALID;
      }

      return CMD_HELP;

    case '#':
      cleanup(fd);
      return CMD_EMPTY;

    case '\n':
      return CMD_EMPTY;

    default:
      cleanup(fd);
      return CMD_INVALID;
  }
}

int parse_create(int fd, unsigned int *event_id, size_t *num_rows, size_t *num_cols) {
  char ch;

  if (read_uint(fd, event_id, &ch) != 0 || ch != ' ') {
    cleanup(fd);
    return 1;
  }

  unsigned int u_num_rows;
  if (read_uint(fd, &u_num_rows, &ch) != 0 || ch != ' ') {
    cleanup(fd);
    return 1;
  }
  *num_rows = (size_t)u_num_rows;

  unsigned int u_num_cols;
  if (read_uint(fd, &u_num_cols, &ch) != 0 || (ch != '\n' && ch != '\0')) {
    cleanup(fd);
    return 1;
  }
  *num_cols = (size_t)u_num_cols;

  return 0;
}

size_t parse_reserve(int fd, size_t max, unsigned int *event_id, size_t *xs, size_t *ys) {
  char ch;

  if (read_uint(fd, event_id, &ch) != 0 || ch != ' ') {
    cleanup(fd);
    return 0;
  }

  if (read(fd, &ch, 1) != 1 || ch != '[') {
    cleanup(fd);
    return 0;
  }

  size_t num_coords = 0;
  while (num_coords < max) {
    if (read(fd, &ch, 1) != 1 || ch != '(') {
      cleanup(fd);
      return 0;
    }

    unsigned int x;
    if (read_uint(fd, &x, &ch) != 0 || ch != ',') {
      cleanup(fd);
      return 0;
    }
    xs[num_coords] = (size_t)x;

    unsigned int y;
    if (read_uint(fd, &y, &ch) != 0 || ch != ')') {
      cleanup(fd);
      return 0;
    }
    ys[num_coords] = (size_t)y;

    num_coords++;

    if (read(fd, &ch, 1) != 1 || (ch != ' ' && ch != ']')) {
      cleanup(fd);
      return 0;
    }

    if (ch == ']') {
      break;
    }
  }

  if (num_coords == max) {
    cleanup(fd);
    return 0;
  }

  if (read(fd, &ch, 1) != 1 || (ch != '\n' && ch != '\0')) {
    cleanup(fd);
    return 0;
  }

  return num_coords;
}

int parse_show(int fd, unsigned int *event_id) {
  char ch;

  if (read_uint(fd, event_id, &ch) != 0 || (ch != '\n' && ch != '\0')) {
    cleanup(fd);
    return 1;
  }

  return 0;
}

int parse_wait(int fd, unsigned int *delay, unsigned int *thread_id) {
  char ch;

  if (read_uint(fd, delay, &ch) != 0) {
    cleanup(fd);
    return -1;
  }

  if (ch == ' ') {
    if (thread_id == NULL) {
      cleanup(fd);
      return 0;
    }

    if (read_uint(fd, thread_id, &ch) != 0 || (ch != '\n' && ch != '\0')) {
      cleanup(fd);
      return -1;
    }

    return 1;
  } else if (ch == '\n' || ch == '\0') {
    return 0;
  } else {
    cleanup(fd);
    return -1;
  }

}

void build_string(int output_fd, const char **strings, int n_strings) {
    size_t total_length = 0;
    size_t acumulator = 0;
    for (int i = 0; i < n_strings; ++i) {
        total_length += strlen(strings[i]);
    }
    total_length += (size_t) n_strings; // Count spaces between strings plus the \n 

    char *result = (char *)malloc(total_length * sizeof(char));
    if (result == NULL) {
        write(output_fd, "Error allocating memory\n", 25);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n_strings; ++i) {
        memcpy(result+acumulator, strings[i], strlen(strings[i]));
        acumulator += strlen(strings[i]);
    }

    memcpy(result+acumulator, "\n", 1);
    write(output_fd, result, acumulator);

    free(result);
}

void int_to_str(unsigned int value, char *str) {
        // Lidar com caso especial de zero
    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    // Converter cada dígito individual
    int index = 0;
    while (value > 0) {
        // Usar unsigned int para evitar warnings de conversão de sinal
        unsigned int digit = value % 10;
        str[index++] = (char)(digit + '0');
        value /= 10;
    }

    // Inverter a string
    int start = 0;
    int end = index - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    // Adicionar terminador nulo
    str[index] = '\0';
}

void process_jobs_directory(const char *directory, int max_processes, unsigned int delay_ms) {
    DIR *dir;

    // Abre o diretório
    dir = opendir(directory);
    if (dir == NULL) {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    int active_children = 0;
    // Lista para armazenar os PIDs dos processos filhos
    pid_t child_pids[max_processes];

    // Processa cada entrada no diretório
    while ((entry = readdir(dir)) != NULL) {
        char filename[256];  // Ajuste o tamanho conforme necessário
        // TODO: tentar pôr as 3 linhas abaixo no build_string
        strcpy(filename, directory);
        strcat(filename, "/");
        strcat(filename, entry->d_name);
        struct stat file_stat;
        if (stat(filename, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
            // Verifica se é um arquivo regular e se a extensão é .jobs
            if (strcmp(strrchr(entry->d_name, '.'), ".jobs") == 0) {
                // Processa o arquivo em paralelo
                if (active_children < max_processes) {
                    // Cria um novo processo filho para processar o arquivo
                    int fd = open(filename, O_RDONLY);
                    if (fd == -1) {
                        perror("Error opening file");
                        exit(EXIT_FAILURE);
                    }

                    // Creation of the file ".out"
                    size_t filename_len = strlen(filename);
                    char *out_filename = malloc(filename_len + 5);  // ".out" has 4 characters plus one for '/' and another for '\0'
                    if (out_filename == NULL) {
                        perror("Error allocating memory for output filename");
                        close(fd);
                        exit(EXIT_FAILURE);
                    }

                    const char *dot = strrchr(filename, '.');

                    if (dot != NULL) {
                        // Se houver um ponto, copiar o nome do arquivo até o ponto para out_filename
                        size_t prefix_len = strlen(filename) - strlen(dot);
                        strncpy(out_filename, filename, prefix_len);
                        out_filename[prefix_len] = '\0';
                    } else {
                        // Se não houver ponto, copiar o nome do arquivo inteiro para out_filename
                        strcpy(out_filename, filename);
                    }

                    // Adicionar a extensão ".out"
                    strcat(out_filename, ".out");

                    int out_fd = open(out_filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                    if (out_fd == -1) {
                        free(out_filename);
                        perror("Error creating output file");
                        close(fd);
                        exit(EXIT_FAILURE);
                    }
                    pid_t pid = fork();
                    if (pid == -1) {
                        perror("Error forking process");
                        free(out_filename);
                        closedir(dir);
                        exit(EXIT_FAILURE);
                    } else if (pid == 0) {
                        // Código do processo filho
                        //if(!strcmp(out_filename, "jobs/b.out"))
                          //sleep(5);
                        if (ems_init(out_fd, delay_ms)) {
                            write(out_fd, "Failed to initialize EMS\n", 25);
                            free(out_filename);
                            close(fd);
                            close(out_fd);
                            exit(EXIT_FAILURE);
                        }
                        ems_process_command(fd, out_fd);
                        free(out_filename);
                        close(fd);
                        close(out_fd);
                        exit(EXIT_SUCCESS);
                    } else {
                        free(out_filename);
                        close(fd);
                        close(out_fd);
                        active_children++;
                        // Armazena o PID do processo filho na lista
                        child_pids[active_children - 1] = pid;
                    }
                } 
                else {
                    // Espera pelo menos um processo filho terminar antes de iniciar um novo
                    int status;
                    pid_t child_pid = waitpid(-1, &status, 0);
                    if (WIFEXITED(status)) {
                        printf("Processo filho %d terminou normalmente com código de saída %d\n", child_pid, WEXITSTATUS(status));
                    } else if (WIFSIGNALED(status)) {
                        printf("Processo filho %d terminou devido a um sinal com código %d\n", child_pid, WTERMSIG(status));
                    }
                    active_children--;

                    // Substitui o PID do processo filho que terminou com o novo processo filho
                    int fd = open(filename, O_RDONLY);
                    if (fd == -1) {
                        perror("Error opening file");
                        exit(EXIT_FAILURE);
                    }

                    size_t filename_len = strlen(filename);
                    char *out_filename = malloc(filename_len + 5);
                    if (out_filename == NULL) {
                        perror("Error allocating memory for output filename");
                        close(fd);
                        exit(EXIT_FAILURE);
                    }

                    const char *dot = strrchr(filename, '.');

                    if (dot != NULL) {
                        size_t prefix_len = strlen(filename) - strlen(dot);
                        strncpy(out_filename, filename, prefix_len);
                        out_filename[prefix_len] = '\0';
                    } else {
                        strcpy(out_filename, filename);
                    }

                    strcat(out_filename, ".out");

                    int out_fd = open(out_filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                    if (out_fd == -1) {
                        free(out_filename);
                        perror("Error creating output file");
                        close(fd);
                        exit(EXIT_FAILURE);
                    }

                    pid_t pid = fork();
                    if (pid == -1) {
                        perror("Error forking process");
                        free(out_filename);
                        closedir(dir);
                        exit(EXIT_FAILURE);
                    } else if (pid == 0) {
                        if (ems_init(out_fd, delay_ms)) {
                            write(out_fd, "Failed to initialize EMS\n", 25);
                            free(out_filename);
                            close(fd);
                            close(out_fd);
                            exit(EXIT_FAILURE);
                        }
                        ems_process_command(fd, out_fd);
                        free(out_filename);
                        close(fd);
                        close(out_fd);
                        exit(EXIT_SUCCESS);
                    } else {
                        free(out_filename);
                        close(fd);
                        close(out_fd);
                        active_children++;
                        // Armazena o PID do novo processo filho na lista
                        child_pids[active_children - 1] = pid;
                    }
                }
            }
        }
    }

    // Espera pelos processos filhos restantes
    for (int i = 0; i < max_processes; i++) {
        int status;
        pid_t child_pid = waitpid(child_pids[i], &status, 0);
        if (WIFEXITED(status)) {
            printf("Processo filho %d terminou normalmente com código de saída %d\n", child_pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
          printf("Processo filho %d terminou devido a um sinal com código %d\n", child_pid, WTERMSIG(status));
        }
        active_children--;
    }
    // Fecha o diretório
    closedir(dir);
}
