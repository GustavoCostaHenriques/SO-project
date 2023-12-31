#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

static int fd_server;

static worker_client_t *clients;
static char *server_pipe;
static pthread_mutex_t server_pipe_lock;
static pthread_mutex_t free_client_lock;
static pthread_cond_t cond_to_wait;

bool all_clients_are_busy = false;

static void sig_handler(int sig) {
  if (sig == SIGUSR1) {
    print_events();
  }
}

void initialize_client(worker_client_t *client) {

  ssize_t ret = read(fd_server, client->req_client_pipe, PIPENAME_SIZE);
  if (ret < 0) {
    fprintf(stdout, "ERROR read failed\n");
    close(fd_server);
    exit(EXIT_FAILURE);
  }
  ret = read(fd_server, client->resp_client_pipe, PIPENAME_SIZE);  
  if (ret < 0) {
    fprintf(stdout, "ERROR read failed\n");
    close(fd_server);
    exit(EXIT_FAILURE);
  }

  close(fd_server);
  // Open response client pipe for reading
  // This waits for someone to open it for writing
  fd_server = open(server_pipe, O_WRONLY);
  if (fd_server == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  
  ret = write(fd_server, &client->session_id, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERROR write failed\n");
    close(fd_server);
    exit(EXIT_FAILURE);
  }
  close(fd_server);

  if(pthread_mutex_unlock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }

  if(pthread_mutex_lock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }

  // Open request client pipe for reading
  // This waits for someone to open it for writing
  do{
    client->fd_req = open(client->req_client_pipe, O_RDONLY);
  } while (client->fd_req == -1);
  if (client->fd_req == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    client->opcode = 2;
  }

  // Open response client pipe for reading
  // This waits for someone to open it for writing
  do {
    client->fd_resp = open(client->resp_client_pipe, O_WRONLY);
  } while(client->fd_resp == -1);
  if (client->fd_resp == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    client->opcode = 2;
  }

  char op_code;
  ret = read(client->fd_req, &op_code, sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    client->opcode = 2;
  }
  else
    client->opcode = op_code;


  if(pthread_mutex_unlock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }

  if (pthread_mutex_unlock(&server_pipe_lock) != 0) {
    exit(EXIT_FAILURE);
  }

}

void create_event(int fd_req, int fd_resp, worker_client_t *client) {
  unsigned int event_id;
  size_t num_rows;
  size_t num_cols;

  if(pthread_mutex_lock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }
  ssize_t ret = read(fd_req, &event_id, sizeof(unsigned int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    client->opcode = 2;
  }

  ret = read(fd_req, &num_rows, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    client->opcode = 2;
  }

  ret = read(fd_req, &num_cols, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    client->opcode = 2;
  }

  int success = ems_create(event_id, num_rows, num_cols);
  ret = write(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    client->opcode = 2;
  }

  char op_code;
  ret = read(fd_req, &op_code, sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    client->opcode = 2;
  }
  else
    client->opcode = op_code;

  if(pthread_mutex_unlock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }
}

void reserve_seats(int fd_req, int fd_resp, worker_client_t *client) {
  unsigned int event_id;
  size_t num_seats;

  if(pthread_mutex_lock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }

  ssize_t ret = read(fd_req, &event_id, sizeof(unsigned int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    client->opcode = 2;
  }

  ret = read(fd_req, &num_seats, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    client->opcode = 2;
  }

  size_t* xs = malloc(sizeof(size_t) * num_seats);

  if (xs == NULL) {
    fprintf(stdout, "ERR: failed to allocate memory\n");
    client->opcode = 2;
  }

  ret = read(fd_req, xs, sizeof(size_t[num_seats]));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    free(xs);
    client->opcode = 2;
  }

  size_t* ys = malloc(sizeof(size_t) * num_seats);

  if (ys == NULL) {
    fprintf(stdout, "ERR: failed to allocate memory\n");
    client->opcode = 2;
  }

  ret = read(fd_req, ys, sizeof(size_t[num_seats]));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    free(xs);
    free(ys);
    client->opcode = 2;
  }

  int success = ems_reserve(event_id, num_seats, xs, ys);
  ret = write(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    free(xs);
    free(ys);
    client->opcode = 2;
  }

  free(ys);
  free(xs);

  char op_code;
  ret = read(fd_req, &op_code, sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    client->opcode = 2;
  }
  else
    client->opcode = op_code;

  if(pthread_mutex_unlock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }
}

void show_event(int fd_req, int fd_resp, worker_client_t *client) {
  unsigned int event_id;

  if(pthread_mutex_lock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }

  ssize_t ret = read(fd_req, &event_id, sizeof(unsigned int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    client->opcode = 2;
  }

  ems_show(fd_resp, event_id);

  char op_code;
  ret = read(fd_req, &op_code, sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    client->opcode = 2;
  }
  else
    client->opcode = op_code;

  if(pthread_mutex_unlock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }
}

void list_events(worker_client_t *client) {

  if(pthread_mutex_lock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }

  ems_list_events(client->fd_resp, client->fd_req, client);
  

  if(pthread_mutex_unlock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }

}

void close_client(worker_client_t *client) {

  if(pthread_mutex_lock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }

  close(client->fd_req);
  close(client->fd_resp);

  if(pthread_mutex_unlock(&client->lock) != 0) {
    exit(EXIT_FAILURE);
  }

}

void *handle_messages(void *args) {
  
  worker_client_t *client =   (worker_client_t *)args;

  if(client->client_was_used) {
    if(pthread_mutex_unlock(&client->lock) != 0)
      return NULL;
  }

  if(pthread_mutex_lock(&client->lock) != 0)
    return NULL;

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0) {
    fprintf(stdout, "ERROR %s\n", "Failed to mask thread");
    return NULL;
  }

  while (!client->to_execute) {
    if (pthread_cond_wait(&client->cond, &client->lock) != 0) {
      fprintf(stdout, "ERROR %s\n", "Failed to wait for condition variable");
      return NULL;
    }
  }

  int client_ended = 0;
  client->client_was_used = true;
  while (1) {
    switch (client->opcode)
    {
    case OP_CODE_CLIENT:
      initialize_client(client);
      break;
    case OP_CODE_QUIT:
      close_client(client);
      client_ended = 1;
      break;
    case OP_CODE_CREATE:
      create_event(client->fd_req , client->fd_resp, client);
      break;
    case OP_CODE_RESERVE:
      reserve_seats(client->fd_req , client->fd_resp, client);
      break;
    case OP_CODE_SHOW:
      show_event(client->fd_req , client->fd_resp, client);
      break;
    case OP_CODE_LIST:
      list_events(client);
      break;
    default:
      break;
    
    }
    if(client_ended)break; 
  }
  client->to_execute = false;

  if(all_clients_are_busy) {
    if (pthread_mutex_lock(&free_client_lock) != 0) {
      return NULL;
    } 

    all_clients_are_busy = false;
    if (pthread_cond_broadcast(&cond_to_wait) != 0) {
      fprintf(stdout, "ERROR %s\n", "Couldn't signal client");
      return NULL;
    }

    if (pthread_mutex_unlock(&free_client_lock) != 0) {
      return NULL;
    }

  }

  handle_messages((void *)&clients[client->session_id]);
  if (pthread_mutex_unlock(&client->lock) != 0) {
    return NULL;
  }

  return 0;
}

int init_server() {
  clients = malloc(sizeof(worker_client_t)*MAX_SESSION_COUNT);

  struct sigaction sa_sigusr1;
  memset(&sa_sigusr1, 0, sizeof(sa_sigusr1));
  sa_sigusr1.sa_handler = sig_handler;
  sigaction(SIGUSR1, &sa_sigusr1, NULL);
  
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    clients[i].session_id = i;
    clients[i].to_execute = false;
    clients[i].client_was_used = false;
    if (pthread_mutex_init(&clients[i].lock, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&clients[i].cond, NULL) != 0) {
        return -1;
    }
    if (pthread_create(&clients[i].tid, NULL, handle_messages, &clients[i]) != 0) {
        return -1;
    }
  }
  if (pthread_mutex_init(&server_pipe_lock, NULL) != 0) {
    return -1;
  }
  if (pthread_mutex_init(&free_client_lock, NULL) != 0) {
    return -1;
  }
  if (pthread_cond_init(&cond_to_wait, NULL) != 0) {
    return -1;
  }
  return 0;
}

void function(int parser_fn(worker_client_t*), char op_code) {

  int session_id = -1;
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if (!clients[i].to_execute) {
      session_id = i;
      break;
    }
  }
  if(session_id < 0) {
    if (pthread_mutex_lock(&free_client_lock) != 0) {
      exit(EXIT_FAILURE);
    }
    all_clients_are_busy = true;
    while(all_clients_are_busy) {
      if (pthread_cond_wait(&cond_to_wait, &free_client_lock) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to wait for condition variable");
        exit(EXIT_FAILURE);
      }
    }
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if (!clients[i].to_execute) {
        session_id = i;
        break;
      }
    }
    if (pthread_mutex_unlock(&free_client_lock) != 0) {
      exit(EXIT_FAILURE);
    }
  }
  
  worker_client_t *client = &clients[session_id];
  
  pthread_mutex_lock(&client->lock);

  client->opcode = op_code;
  int result = 0;
  if (parser_fn != NULL) {
      result = parser_fn(client);
  }
  
  if (pthread_mutex_lock(&server_pipe_lock) != 0) {
    exit(EXIT_FAILURE);
  }

  if (result == 0) {
    client->to_execute = true;
    if (pthread_cond_signal(&client->cond) != 0) {
      fprintf(stdout, "ERROR %s\n", "Couldn't signal client");
      exit(EXIT_FAILURE);
    }
  } else {
    fprintf(stdout, "ERROR %s\n", "Failed to execute client");
    exit(EXIT_FAILURE);
  }
  pthread_mutex_unlock(&client->lock);
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
      fprintf(stderr, "ERROR invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "ERROR failed to initialize EMS\n");
    return 1;
  }

  if (init_server() != 0) {
    fprintf(stdout, "ERROR %s\n", "Failed to init server\n");
    return 1;
  }

  //TODO: Intialize server, create worker threads
  server_pipe = argv[1];

  // Remove server pipe if it does exist
  if (unlink(server_pipe) != 0 && errno != ENOENT) {
    fprintf(stdout, "ERROR unlink(%s) failed:\n", server_pipe);
    return 1;
  }

  // Create server pipe
  if (mkfifo(server_pipe, 0666) != 0) {
    fprintf(stdout, "ERROR mkfifo failed for pipe %s\n", server_pipe);
    return 1;
  }

  char op_code;
  ssize_t ret;
  while(1) {

    // Open server pipe for reading
    // This waits for someone to open it for writing
    do {
      fd_server = open(server_pipe, O_RDONLY);
    } while (fd_server == -1 && errno == EINTR);

    do {
      ret = read(fd_server, &op_code, sizeof(char));
    } while (ret < 0 && errno == EINTR);

    while(ret > 0) {
      function(NULL, op_code);
      if (pthread_mutex_lock(&server_pipe_lock) != 0) {
        return 1;
      }

      // Open server pipe for reading
      // This waits for someone to open it for writing
      do {
        fd_server = open(server_pipe, O_RDONLY);
      } while (fd_server == -1 && errno == EINTR);
      
      do {
        ret = read(fd_server, &op_code, sizeof(char));
      } while (ret < 0 && errno == EINTR);

      if (pthread_mutex_unlock(&server_pipe_lock) != 0) {
        return 1;
      }
    }
    if (ret < 0) {
      fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
      return 1;
    }
    
    if (close(fd_server) < 0) {
      fprintf(stdout, "ERROR failed to close pipe\n");
      return 1;
    }
    
  }

  ems_terminate();
  return 0;
}