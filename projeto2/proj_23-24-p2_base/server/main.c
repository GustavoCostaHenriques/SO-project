#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

void initialize_client(const char *server_pipe) {
  // Open response client pipe for reading
  // This waits for someone to open it for writing
  int fd_server = open(server_pipe, O_WRONLY);
  if (fd_server == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  printf("SERVER PIPE WAITING TO WRITE\n");
  int zero = 0;
  ssize_t ret = write(fd_server, &zero, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    exit(EXIT_FAILURE);
  }
  close(fd_server);
}

void create_event(int fd_req, int fd_resp) {
  unsigned int event_id;
  size_t num_rows;
  size_t num_cols;

  ssize_t ret = read(fd_req, &event_id, sizeof(unsigned int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received event_id: %u\n", event_id);

  ret = read(fd_req, &num_rows, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received num_rows: %lu\n", num_rows);

  ret = read(fd_req, &num_cols, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received num_cols: %lu\n", num_cols);

  int success = ems_create(event_id, num_rows, num_cols);
  printf("SUCCESS: %d\n", success);
  ret = write(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    exit(EXIT_FAILURE);
  }
}

void reserve_seats(int fd_req, int fd_resp) {
  unsigned int event_id;
  size_t num_seats;

  ssize_t ret = read(fd_req, &event_id, sizeof(unsigned int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received event_id: %u\n", event_id);

  ret = read(fd_req, &num_seats, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received num_seats: %lu\n", num_seats);

  size_t* xs = malloc(sizeof(size_t) * num_seats);

  if (xs == NULL) {
    fprintf(stdout, "ERR: failed to allocate memory\n");
    exit(EXIT_FAILURE);
  }

  ret = read(fd_req, xs, sizeof(size_t[num_seats]));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  for(size_t i = 0; i < num_seats; i++) {
    fprintf(stdout, "Received xs coordenate: %lu\n", xs[i]);
  }

  size_t* ys = malloc(sizeof(size_t) * num_seats);

  if (ys == NULL) {
    fprintf(stdout, "ERR: failed to allocate memory\n");
    exit(EXIT_FAILURE);
  }

  ret = read(fd_req, ys, sizeof(size_t[num_seats]));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  for(size_t i = 0; i < num_seats; i++) {
    fprintf(stdout, "Received ys coordenate: %lu\n", ys[i]);
  }

  int success = ems_reserve(event_id, num_seats, xs, ys);
  printf("SUCCESS: %d\n", success);
  ret = write(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    exit(EXIT_FAILURE);
  }

  free(ys);
  free(xs);
}

void show_event(int fd_req, int fd_resp) {
  unsigned int event_id;
  size_t num_rows;
  size_t num_cols;
  unsigned int *seats;

  ssize_t ret = read(fd_req, &event_id, sizeof(unsigned int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received event_id: %u\n", event_id);

  int success = ems_show(num_rows, num_cols, seats, event_id);
  printf("ROWS: %ld COLS: %ld\n", num_rows, num_cols);
  
  printf("SUCCESS: %d\n", success);
  ret = write(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    exit(EXIT_FAILURE);
  }
  printf("WROTE success\n");

  printf("NUM_ROWS: %ld\n", num_rows);
  ret = write(fd_resp, &num_rows, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    exit(EXIT_FAILURE);
  }
  printf("WROTE num_rows\n");

  printf("NUM_COLS: %ld\n", num_cols);
  ret = write(fd_resp, &num_cols, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    exit(EXIT_FAILURE);
  }
  printf("WROTE num_cols\n");

  ret = write(fd_resp, seats, sizeof(unsigned int[num_rows*num_cols]));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    exit(EXIT_FAILURE);
  }
  printf("WROTE seats\n");

  free(seats);
}


int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  //TODO: Intialize server, create worker threads
  static char *server_pipe;
  char req_client_pipe[PIPENAME_SIZE];
  char resp_client_pipe[PIPENAME_SIZE];
  server_pipe = argv[1];

  // Remove server pipe if it does exist
  if (unlink(server_pipe) != 0 && errno != ENOENT) {
    fprintf(stdout, "ERROR unlink(%s) failed:\n", server_pipe);
    exit(EXIT_FAILURE);
  }

  // Create server pipe
  if (mkfifo(server_pipe, 0666) != 0) {
    fprintf(stdout, "ERROR mkfifo failed for pipe %s\n", server_pipe);
    exit(EXIT_FAILURE);
  }
  printf("SERVER PIPE CREATED\n");

  // Open server pipe for reading
  // This waits for someone to open it for writing
  int fd_server = open(server_pipe, O_RDONLY);
  if (fd_server == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  printf("SERVER PIPE WAITING TO READ\n");

  char msg_received_init[OP_CODE+1];
  ssize_t ret = read(fd_server, msg_received_init, OP_CODE);
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  msg_received_init[1] = '\0';

  ret = read(fd_server, req_client_pipe, PIPENAME_SIZE);
  if (ret < PIPENAME_SIZE) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }

  ret = read(fd_server, resp_client_pipe, PIPENAME_SIZE);
  if (ret < PIPENAME_SIZE) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received Message: %s\n", msg_received_init);
  printf("REQ PIPE: %s\n",req_client_pipe);
  printf("RESP PIPE: %s\n",resp_client_pipe);
  close(fd_server);

  if(msg_received_init[0] == '1')
    initialize_client(server_pipe);

  int i = 0;
  // Open request client pipe for reading
  // This waits for someone to open it for writing
  int fd_req = open(req_client_pipe, O_RDONLY);
  if (fd_req == -1) {
    fprintf(stderr, "[ERR]: 1open failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  printf("REQUEST CLIENT PIPE WAITING TO READ\n");

  // Open response client pipe for reading
  // This waits for someone to open it for writing
  int fd_resp = open(resp_client_pipe, O_WRONLY);
  if (fd_resp == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  printf("SERVER PIPE WAITING TO WRITE\n");

  while (1) {
    //TODO: Read from pipe - LER DA REQ PIPE E FAZER AGORA O SWITCH CASE

    char op_code[OP_CODE+1];
    ret = read(fd_req, op_code, OP_CODE);
    if (ret < 0) {
      fprintf(stdout, "ERR: read failed\n");
      exit(EXIT_FAILURE);
    }
    op_code[1] = '\0';

    // Imprima a mensagem recebida
    fprintf(stdout, "[INFO]: received %zd B\n", ret);
    fprintf(stdout, "Received OP_CODE: %s\n", op_code);
    switch (op_code[0])
    {
    case OP_CODE_QUIT:
    case OP_CODE_CREATE:
      create_event(fd_req, fd_resp);
      break;
    case OP_CODE_RESERVE:
      reserve_seats(fd_req, fd_resp);
      break;
    case OP_CODE_SHOW:
      show_event(fd_req, fd_resp);
      break;
    case OP_CODE_LIST:
    default:
      break;
    
    }
    i++;
    if(i == 3)break;
    //TODO: Write new client to the producer-consumer buffer
  }

  //TODO: Close Server

  ems_terminate();
}