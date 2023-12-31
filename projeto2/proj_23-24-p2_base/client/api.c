#include "api.h"
#include "common/io.h"
#include "common/constants.h"

int fd_resp;
int fd_req;
char req_client_pipe[PIPENAME_SIZE];
char resp_client_pipe[PIPENAME_SIZE];

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
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

  // Create resp_pipe_path
  if (mkfifo(resp_pipe_path, 0666) != 0) {
    fprintf(stdout, "ERROR mkfifo failed for pipe %s\n", resp_pipe_path);
    return 1;
  }

  // Open server pipe for writing
  // This waits for someone to open it for reading
  int fd_server = open(server_pipe_path, O_WRONLY);
  if (fd_server == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    return 1;
  }


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

  close(fd_server);
  // Open server pipe for reading
  // This waits for someone to open it for writing
  fd_server = open(server_pipe_path, O_RDONLY);
  if (fd_server == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    return 1;
  }

  int session_id;
  ret = read(fd_server, &session_id, sizeof(int));
  if (ret < 0) {
    fprintf(stdout, "ERR: read failed\n");
    return 1;
  }

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

  // Open request client pipe for reading
  // This waits for someone to open it for writing
  fd_resp = open(resp_client_pipe, O_RDONLY);
  if (fd_resp == -1) {
    fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
    return 1;
  }

  return 0;
}

int ems_quit(void) { 
  ssize_t ret = write(fd_req, "2", sizeof(char));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  close(fd_req);
  close(fd_resp);


  if (unlink(req_client_pipe) != 0 && errno != ENOENT) {
    fprintf(stdout, "ERROR unlink(%s) failed:\n", req_client_pipe);
    return 1;
  }

  // Remove resp_pipe_path if it does exist
  if (unlink(resp_client_pipe) != 0 && errno != ENOENT) {
    fprintf(stdout, "ERROR unlink(%s) failed:\n", resp_client_pipe);
    return 1;
  }

  return 0;
}

int ems_destroy_client(void) { 

  close(fd_req);
  close(fd_resp);

  if (unlink(req_client_pipe) != 0 && errno != ENOENT) {
    fprintf(stdout, "ERROR unlink(%s) failed:\n", req_client_pipe);
    return 1;
  }

  // Remove resp_pipe_path if it does exist
  if (unlink(resp_client_pipe) != 0 && errno != ENOENT) {
    fprintf(stdout, "ERROR unlink(%s) failed:\n", resp_client_pipe);
    return 1;
  }

  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {

  ssize_t ret = write(fd_req, "3", sizeof(char));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  ret = write(fd_req, &event_id, sizeof(event_id));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }
  
  ret = write(fd_req, &num_rows, sizeof(num_rows));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  ret = write(fd_req, &num_cols, sizeof(num_cols));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  int success;
  ret = read(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  return success;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {

  ssize_t ret = write(fd_req, "4", sizeof(char));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  ret = write(fd_req, &event_id, sizeof(event_id));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }
  
  ret = write(fd_req, &num_seats, sizeof(num_seats));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }
  
  ret = write(fd_req, xs, sizeof(size_t[num_seats]));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }
  
  ret = write(fd_req, ys, sizeof(size_t[num_seats]));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  int success;
  ret = read(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  return success;
}

int ems_show(int out_fd, unsigned int event_id) {
  int success;
  size_t num_rows;
  size_t num_cols;

  ssize_t ret = write(fd_req, "5", sizeof(char));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  ret = write(fd_req, &event_id, sizeof(event_id));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  ret = read(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  if(success){
    print_str(out_fd, "Event not found\n");
    return success;
  }

  ret = read(fd_resp, &num_rows, sizeof(size_t));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  ret = read(fd_resp, &num_cols, sizeof(size_t));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  unsigned int *seats = malloc(sizeof(unsigned int[num_rows*num_cols]));

  if (seats == NULL) {
    fprintf(stdout, "ERR: failed to allocate memory\n");
    return 1;
  }

  ret = read(fd_resp, seats, sizeof(unsigned int[num_rows*num_cols]));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

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
  int success;
  size_t num_events;

  ssize_t ret = write(fd_req, "6", sizeof(char));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  ret = read(fd_resp, &success, sizeof(int));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  if(success){
    return success;
  }

  ret = read(fd_resp, &num_events, sizeof(size_t));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

  if(num_events == (size_t)0) {
    print_str(out_fd,"No events\n");
    return success;
  }

  unsigned int ids[num_events];

  ret = read(fd_resp, ids, sizeof(unsigned int[num_events]));
  if (ret < 0) {
    ems_destroy_client();
    return 1;
  }

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
