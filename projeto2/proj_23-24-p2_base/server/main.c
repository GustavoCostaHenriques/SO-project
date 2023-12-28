#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

static int fd_server;
static worker_client_t *clients;
static char *server_pipe;
static pthread_mutex_t server_pipe_lock;
bool server_will_close = false;
bool server_is_lock = false;

void destroy_server(int status) {

  close(fd_server);
  if (unlink(server_pipe) != 0 && errno != ENOENT) {
      fprintf(stdout, "ERROR %s\n", "Failed to delete pipe");
      exit(EXIT_FAILURE);
  }
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    pthread_mutex_lock(&clients[i].lock);

    // Sinaliza para as threads que é hora de sair
    clients[i].to_execute = true;
    server_will_close = true;
    pthread_cond_broadcast(&clients[i].cond);

    pthread_mutex_unlock(&clients[i].lock);

    if(pthread_cond_destroy(&clients[i].cond) != 0)
      exit(EXIT_FAILURE);

  }  
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if(pthread_join(clients[i].tid, NULL) != 0)
      exit(EXIT_FAILURE);
  }

  // Agora, após as threads terminarem, você pode destruir os mutexes
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if(!clients[i].client_was_used) {
      pthread_mutex_unlock(&clients[i].lock);
    }

    int k = pthread_mutex_destroy(&clients[i].lock);
      //exit(EXIT_FAILURE);
    printf("\n%d\n", k);
  
  }
  free(clients);
  if(server_is_lock) {
    if(pthread_mutex_unlock(&server_pipe_lock) != 0) {
      exit(EXIT_FAILURE);
    }
  }
  if (pthread_mutex_destroy(&server_pipe_lock) != 0) {
    exit(EXIT_FAILURE);
  } 
  ems_terminate();
  printf("\nSUCCESSFULLY ENDED THE SERVER.\n");
  exit(status);
}

static void sig_handler(int sig) {
  if (sig == SIGINT) {
    destroy_server(EXIT_SUCCESS);
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

  printf("REQ CLIENT PIPE: %s\n", client->req_client_pipe);
  printf("RESP CLIENT PIPE: %s\n", client->resp_client_pipe);

  close(fd_server);
  // Open response client pipe for reading
  // This waits for someone to open it for writing
  fd_server = open(server_pipe, O_WRONLY);
  if (fd_server == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  printf("SERVER PIPE WAITING TO WRITE\n");
  
  ret = write(fd_server, &client->session_id, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERROR write failed\n");
    close(fd_server);
    exit(EXIT_FAILURE);
  }
  close(fd_server);

  if (pthread_mutex_unlock(&server_pipe_lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }
  server_is_lock = false;

  if(pthread_mutex_unlock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }

  if(pthread_mutex_lock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }

  // Open request client pipe for reading
  // This waits for someone to open it for writing
  client->fd_req = open(client->req_client_pipe, O_RDONLY);
  if (client->fd_req == -1) {
    fprintf(stderr, "[ERR]: 1->open failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  printf("REQUEST CLIENT PIPE WAITING TO READ\n");

  // Open response client pipe for reading
  // This waits for someone to open it for writing
  client->fd_resp = open(client->resp_client_pipe, O_WRONLY);
  if (client->fd_resp == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  printf("RESPONSE CLIENT PIPE WAITING TO WRITE\n");

  char op_code;
  ret = read(client->fd_req, &op_code, sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  client->opcode = op_code;

  if(pthread_mutex_unlock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }

}

void create_event(int fd_req, int fd_resp, worker_client_t *client) {
  unsigned int event_id;
  size_t num_rows;
  size_t num_cols;

  if(pthread_mutex_lock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }
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

  char op_code;
  ret = read(fd_req, &op_code, sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  client->opcode = op_code;

  if(pthread_mutex_unlock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }
}

void reserve_seats(int fd_req, int fd_resp, worker_client_t *client) {
  unsigned int event_id;
  size_t num_seats;

  if(pthread_mutex_lock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }

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
    free(xs);
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
    free(xs);
    free(ys);
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
    free(xs);
    free(ys);
    exit(EXIT_FAILURE);
  }

  free(ys);
  free(xs);

  char op_code;
  ret = read(fd_req, &op_code, sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  client->opcode = op_code;

  if(pthread_mutex_unlock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }
}

void show_event(int fd_req, int fd_resp, worker_client_t *client) {
  unsigned int event_id;

  if(pthread_mutex_lock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }

  ssize_t ret = read(fd_req, &event_id, sizeof(unsigned int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received event_id: %u\n", event_id);

  ems_show(fd_resp, event_id);

  char op_code;
  ret = read(fd_req, &op_code, sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    exit(EXIT_FAILURE);
  }
  client->opcode = op_code;

  if(pthread_mutex_unlock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }
}

void list_events(worker_client_t *client) {

  if(pthread_mutex_lock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }

  ems_list_events(client->fd_resp, client->fd_req, client);
  

  if(pthread_mutex_unlock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }

}

void close_client(worker_client_t *client) {

  if(pthread_mutex_lock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }

  close(client->fd_req);
  printf("CLOSE REQUEST CLIENT PIPE\n");
  close(client->fd_resp);
  printf("CLOSE RESPONSE CLIENT PIPE\n");

  if(pthread_mutex_unlock(&client->lock) != 0) {
    destroy_server(EXIT_FAILURE);
  }

}

void *handle_messages(void *args) {
  
  worker_client_t *client =   (worker_client_t *)args;
  if(pthread_mutex_lock(&client->lock) != 0)
    return NULL;

  fprintf(stdout, "CLIENT %d WAITING TO EXECUTE\n", client->session_id);
  while (!client->to_execute) {
    fprintf(stdout, "CLIENT %d HAS FALSE IN EXECUTE\n", client->session_id);
    if (pthread_cond_wait(&client->cond, &client->lock) != 0) {
      fprintf(stdout, "ERROR %s\n", "Failed to wait for condition variable");
      return NULL;
    }
  }
  if(server_will_close)
    return NULL;
  fprintf(stdout, "CLIENT %d IS NO LONGER WAITING\n", client->session_id);

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
  if (pthread_mutex_unlock(&client->lock) != 0) {
    return NULL;
  }
  fprintf(stdout, "GOT OUT OF CLIENT\n");
  handle_messages((void *)&clients[client->session_id]);
  return 0;
}

int init_server() {
  clients = malloc(sizeof(worker_client_t)*MAX_SESSION_COUNT);
  
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
  return 0;
}

void function(int parser_fn(worker_client_t*), char op_code) {

  int session_id = -1;
  while(session_id == -1) {
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
      if (!clients[i].to_execute) {
        session_id = i;
        break;
      }
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
    destroy_server(EXIT_FAILURE);
  }
  server_is_lock = true;    
  if (result == 0) {
    client->to_execute = true;
    fprintf(stdout, "CLIENT %d WILL START TO EXECUTE\n", client->session_id);
    if (pthread_cond_signal(&client->cond) != 0) {
      fprintf(stdout, "ERROR %s\n", "Couldn't signal client");
    }
  } else {
    fprintf(stdout, "ERROR %s\n", "Failed to execute client");
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
  fprintf(stdout, "INITIALIZATION OF ALL CLIENT WORKER THREADS SUCCESSFULL\n");

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
  printf("SERVER PIPE CREATED\n");

  signal(SIGINT, sig_handler);  

  char op_code;
  ssize_t ret;
  while(1) {

    // Open server pipe for reading
    // This waits for someone to open it for writing
    fd_server = open(server_pipe, O_RDONLY);
    if (fd_server == -1) {
      fprintf(stderr, "ERROR open failed: %s\n", strerror(errno));
      return 1;
    }
    printf("SERVER PIPE WAITING TO READ\n");

    do {
      ret = read(fd_server, &op_code, sizeof(char));
    } while (ret < 0 && errno == EINTR);

    fprintf(stdout, "[INFO]: received %zd\n", ret);
    fprintf(stdout, "Received Message: OPCODE\n");



    while(ret > 0) {
      function(NULL, op_code);
      fprintf(stdout, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
      if (pthread_mutex_lock(&server_pipe_lock) != 0) {
        destroy_server(EXIT_FAILURE);
      }
      server_is_lock = true;
      // Open server pipe for reading
      // This waits for someone to open it for writing
      fd_server = open(server_pipe, O_RDONLY);
      if (fd_server == -1) {
        fprintf(stderr, "ERROR open failed: %s\n", strerror(errno));
        return 1;
      }
      printf("SERVER PIPE WAITING TO READ\n");      
      do {
        ret = read(fd_server, &op_code, sizeof(char));
      } while (ret < 0 && errno == EINTR);
      fprintf(stdout, "[INFO]1: received %zd B\n", ret);
      putchar(op_code);
      fprintf(stdout, " <-Received OPCODE\n");


      if (pthread_mutex_unlock(&server_pipe_lock) != 0) {
          destroy_server(EXIT_FAILURE);
      }
      server_is_lock = false;
    }
    if (ret < 0) {
      fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
      destroy_server(EXIT_FAILURE);
    }
    
    if (close(fd_server) < 0) {
      fprintf(stdout, "ERROR failed to close pipe\n");
      destroy_server(EXIT_FAILURE);
    }
  
  }

  ems_terminate();
  return 0;
}