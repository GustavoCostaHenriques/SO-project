#include "api.h"
#include "common/io.h"
#include "common/constants.h"

int fd_resp;
int fd_req;
char req_client_pipe[PIPENAME_SIZE];
char resp_client_pipe[PIPENAME_SIZE];

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  strcpy(req_client_pipe, req_pipe_path);
  strcpy(resp_client_pipe, resp_pipe_path);

  // Remove req_pipe_path if it does exist
  if (unlink(req_pipe_path) != 0 && errno != ENOENT) {
    fprintf(stdout, "ERROR unlink(%s) failed:\n", req_pipe_path);
    return 1;
  }

  // Remove resp_pipe_path if it does exist
  if (unlink(resp_pipe_path) != 0 && errno != ENOENT) {
    fprintf(stdout, "ERROR unlink(%s) failed:\n", resp_pipe_path);
    return 1;
  }

  // Create req_pipe_path
  if (mkfifo(req_pipe_path, 0666) != 0) {
    fprintf(stdout, "ERROR mkfifo failed for pipe %s\n", req_pipe_path);
    return 1;
  }
  printf("REQUEST PIPE CREATED\n");

  // Create resp_pipe_path
  if (mkfifo(resp_pipe_path, 0666) != 0) {
    fprintf(stdout, "ERROR mkfifo failed for pipe %s\n", resp_pipe_path);
    return 1;
  }
  printf("RESPONSE PIPE CREATED\n"); 

  // Open server pipe for writing
  // This waits for someone to open it for reading
  int fd_server = open(server_pipe_path, O_WRONLY);
  if (fd_server == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    return 1;
  }
  printf("SERVER PIPE WAITING TO WRITE\n");


  char req_pipe_path_fixed[PIPENAME_SIZE];

  fill_string(req_pipe_path, req_pipe_path_fixed);

  char resp_pipe_path_fixed[PIPENAME_SIZE];

  fill_string(resp_pipe_path, resp_pipe_path_fixed);

  char msg_to_server[(2*PIPENAME_SIZE)+1];

  for(size_t i = 0; i < (PIPENAME_SIZE * 2)+1; i++) {
    if(i == 0) 
      msg_to_server[i] = '1';
    else if(i <= PIPENAME_SIZE)
      msg_to_server[i] = req_pipe_path_fixed[i-1];
    else
      msg_to_server[i] = resp_pipe_path_fixed[i-(PIPENAME_SIZE+1)];
  }

  ssize_t ret = write(fd_server, msg_to_server, (2*PIPENAME_SIZE)+1);
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE %s\n", msg_to_server);

  close(fd_server);
  // Open server pipe for reading
  // This waits for someone to open it for writing
  fd_server = open(server_pipe_path, O_RDONLY);
  if (fd_server == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    return 1;
  }
  printf("SERVER PIPE WAITING TO READ\n");

  int session_id;
  ret = read(fd_server, &session_id, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    return 1;
  }
  printf("SESSION_ID: %d\n", session_id);

  close(fd_server);

  if(!(session_id >= 0 && session_id < MAX_SESSION_COUNT)){
    // Remove req_pipe_path if it does exist
    if (unlink(req_pipe_path) != 0 && errno != ENOENT) {
      fprintf(stdout, "ERROR unlink(%s) failed:\n", req_pipe_path);
      return 1;
    }

    // Remove resp_pipe_path if it does exist
    if (unlink(resp_pipe_path) != 0 && errno != ENOENT) {
      fprintf(stdout, "ERROR unlink(%s) failed:\n", resp_pipe_path);
      return 1;
    }
    return 1;
  }

  // Open request client pipe for writing
  // This waits for someone to open it for reading
  fd_req = open(req_client_pipe, O_WRONLY);
  if (fd_req == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    return 1;
  }
  printf("REQUEST PIPE WAITING TO WRITE\n");

  // Open request client pipe for reading
  // This waits for someone to open it for writing
  fd_resp = open(resp_client_pipe, O_RDONLY);
  if (fd_resp == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    return 1;
  }
  printf("RESPONSE PIPE WAITING TO READ\n");

  return 0;
}

int ems_quit(void) { 
  //TODO: close pipes
  ssize_t ret = write(fd_req, "2", sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE 2\n");

  close(fd_req);
  printf("CLOSE REQUEST PIPE\n");

  close(fd_resp);
  printf("CLOSE RESPONSE PIPE\n");


  if (unlink(req_client_pipe) != 0 && errno != ENOENT) {
    fprintf(stdout, "ERROR unlink(%s) failed:\n", req_client_pipe);
    return 1;
  }
  printf("ERASED REQUEST PIPE\n");

  // Remove resp_pipe_path if it does exist
  if (unlink(resp_client_pipe) != 0 && errno != ENOENT) {
    fprintf(stdout, "ERROR unlink(%s) failed:\n", resp_client_pipe);
    return 1;
  }
  printf("ERASED RESPONSE PIPE\n");

  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)

  ssize_t ret = write(fd_req, "3", sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE 3\n");

  ret = write(fd_req, &event_id, sizeof(event_id));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE event_id\n");
  
  ret = write(fd_req, &num_rows, sizeof(num_rows));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE num_rows\n");

  ret = write(fd_req, &num_cols, sizeof(num_cols));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE num_cols\n");

  int success;
  ret = read(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    return 1;
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received sucess: %d\n", success);

  return success;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)

  ssize_t ret = write(fd_req, "4", sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE 4\n");

  ret = write(fd_req, &event_id, sizeof(event_id));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE event_id\n");
  
  ret = write(fd_req, &num_seats, sizeof(num_seats));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE num_seats\n");
  
  ret = write(fd_req, xs, sizeof(size_t[num_seats]));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE xs\n");
  for(size_t i = 0; i < num_seats; i++) {
    fprintf(stdout, "Send xs coordenate: %lu\n", xs[i]);
  }
  
  ret = write(fd_req, ys, sizeof(size_t[num_seats]));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE ys\n");
  for(size_t i = 0; i < num_seats; i++) {
    fprintf(stdout, "Send ys coordenate: %lu\n", ys[i]);
  }

  int success;
  ret = read(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    return 1;
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received sucess: %d\n", success);

  return success;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  int success;
  size_t num_rows;
  size_t num_cols;

  ssize_t ret = write(fd_req, "5", sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE 5\n");

  ret = write(fd_req, &event_id, sizeof(event_id));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE event_id\n");

  ret = read(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    return 1;
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received sucess: %d\n", success);

  if(success){
    char error_msg[ERROR_SIZE_SHOW+1];
    ret = read(fd_resp, error_msg, ERROR_SIZE_SHOW);
    if (ret < 0) {
      fprintf(stdout, "ERR: read failed\n");
      return 1;
    }
    error_msg[ERROR_SIZE_SHOW] = '\0';
    fprintf(stdout, "[INFO]: received %zd B\n", ret);
    fprintf(stdout, "Received error_msg: %s", error_msg);
    return success;
  }

  ret = read(fd_resp, &num_rows, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    return 1;
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received num_rows: %ld\n", num_rows);

  ret = read(fd_resp, &num_cols, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    return 1;
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received num_cols: %ld\n", num_cols);

  unsigned int *seats = malloc(sizeof(unsigned int[num_rows*num_cols]));

  if (seats == NULL) {
    fprintf(stdout, "ERR: failed to allocate memory\n");
    return 1;
  }

  ret = read(fd_resp, seats, sizeof(unsigned int[num_rows*num_cols]));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "[INFO]: received seats\n");

  int aux = 0;
  for(size_t i = 1; i <= num_rows; i++) {
    for(size_t j = 1; j <= num_cols; j++){
      char buffer[16];
      sprintf(buffer, "%u", seats[aux]);
      if(print_str(out_fd, buffer)) {
        fprintf(stdout, "Error writing to file descriptor\n");
        free(seats);
        return 1;
      }
      aux++;

      if (j < num_cols) {
        if(print_str(out_fd, " ")) {
          fprintf(stdout, "Error writing to file descriptor\n");
            free(seats);
          return 1;
        }      
      }
    }
    if(print_str(out_fd, "\n")) {
      fprintf(stdout, "Error writing to file descriptor");
      free(seats);
      return 1;
    }  
  }
  free(seats);
  return success;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  int success;
  size_t num_events;

  ssize_t ret = write(fd_req, "6", sizeof(char));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  printf("WROTE 6\n");

  ret = read(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    return 1;
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received sucess: %d\n", success);

  if(success){
    char error_msg[ERROR_SIZE_LIST+1];
    ret = read(fd_resp, error_msg, ERROR_SIZE_LIST);
    if (ret < 0) {
      fprintf(stdout, "ERR: read failed\n");
      return 1;
    }
    error_msg[ERROR_SIZE_LIST] = '\0';
    fprintf(stdout, "[INFO]: received %zd B\n", ret);
    fprintf(stdout, "Received error_msg: %s", error_msg);
    return success;
  }

  ret = read(fd_resp, &num_events, sizeof(size_t));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    return 1;
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "Received num_events: %ld\n", num_events);

  unsigned int ids[num_events];

  ret = read(fd_resp, ids, sizeof(unsigned int[num_events]));
  if (ret < 0) {
    fprintf(stdout, "ERR: write failed\n");
    return 1;
  }
  fprintf(stdout, "[INFO]: received %zd B\n", ret);
  fprintf(stdout, "[INFO]: received ids\n");

  for(size_t i = 0; i < num_events; i++) {
    char buff[] = "Event: ";
    if (print_str(out_fd, buff)) {
      fprintf(stdout, "Error writing to file descriptor");
      return 1;
    }

    char id[16];
    sprintf(id, "%u\n", ids[i]);
    if (print_str(out_fd, id)) {
      fprintf(stdout, "Error writing to file descriptor");
      return 1;
    }
  }

  return success;
}
