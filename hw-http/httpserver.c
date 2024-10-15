#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue; // Only used by poolserver
int num_threads; // Only used by poolserver
int server_port; // Default value: 8000
char* server_files_directory;
char* server_proxy_hostname;
int server_proxy_port;

/*
 * This function will write size amount of data from src_fd to dst_fd
 * */
#define BUF_SIZE_MAX 8192
void relay_byte_stream(int src_fd, int dst_fd, int size)
{
  char* buf = (char*)malloc(sizeof(char) * BUF_SIZE_MAX);
  int bytes_read, bytes_written;
  
  while((bytes_read = read(src_fd, buf, size)) > 0)
  {
    //printf("This is thread %ld and I just read %d bytes from fd %d.\n", pthread_self(), bytes_read, src_fd);
    //fflush(stdout);
    char* ptr = buf;
    int bytes_remain = bytes_read;

    while((bytes_written = write(dst_fd, ptr, bytes_remain)) > 0)
    {
      if (bytes_written == bytes_read)
        break;

      bytes_remain -= bytes_written;
      ptr += bytes_written;
    }
    //printf("This is thread %ld and I just write %d bytes to fd %d.\n", pthread_self(), bytes_written, dst_fd);
    //fflush(stdout);
  }
  free(buf);
}

/*
 * Serves the contents the file stored at `path` to the client socket `fd`.
 * It is the caller's reponsibility to ensure that the file stored at `path` exists.
 */
void serve_file(int fd, char* path, int size) {

  /* TODO: PART 2 */
  /* PART 2 BEGIN */
  char* ptr = (char*)malloc(10000000); // 10Mb
  snprintf(ptr, 1000000, "%d", size); 

  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(path));
  http_send_header(fd, "Content-Length", ptr); // TODO: change this line too
  /* Send a blank line (CRLF by itself)*/
  http_end_headers(fd); // Otherwise curl won't recognize the body
  
  /* Start transmitting the file itself*/
  int file_fd = open(path, O_RDONLY);
  //int count = 0;
  //char* buf = (char*)malloc(sizeof(char)*size);

  //while (count != size)
  //{
  //  int i = 0;
  //  i = read(file_fd, (void*)buf, size);
  //  write(fd, (void*)buf, i);
  //  count += i;
  //}
  relay_byte_stream(file_fd, fd, size);

  /* PART 2 END */
}

void serve_directory(int fd, char* path) {
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
  http_end_headers(fd);

  /* TODO: PART 3 */
  /* PART 3 BEGIN */

  // TODO: Open the directory (Hint: opendir() may be useful here)
  DIR* dir = opendir(path);
  struct dirent *entry;
#ifdef DEBUG
    printf("The path is: %s\n", path);
#endif
  while ((entry = readdir(dir)) != NULL)
  {
    char* ptr = (char*)malloc(sizeof(char) * 1000);
    http_format_href(ptr, path, entry->d_name);
    write(fd, ptr, strlen(ptr));
#ifdef DEBUG
    printf("The buffer is filled with: %s\n", ptr);
#endif
  }

  /**
   * TODO: For each entry in the directory (Hint: look at the usage of readdir() ),
   * send a string containing a properly formatted HTML. (Hint: the http_format_href()
   * function in libhttp.c may be useful here)
   */

  /* PART 3 END */
}

/*
 * Reads an HTTP request from client socket (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 *
 *   Closes the client socket (fd) when finished.
 */
void handle_files_request(int fd) {

  struct http_request* request = http_request_parse(fd);

  if (request == NULL || request->path[0] != '/') {
    http_start_response(fd, 400);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }

  if (strstr(request->path, "..") != NULL) {
    http_start_response(fd, 403);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return; }

  /* Remove beginning `./` */
  char* path = malloc(2 + strlen(request->path) + 1);
  path[0] = '.';
  path[1] = '/';
  memcpy(path + 2, request->path, strlen(request->path) + 1);

  /*
   * TODO: PART 2 is to serve files. If the file given by `path` exists,
   * call serve_file() on it. Else, serve a 404 Not Found error below.
   * The `stat()` syscall will be useful here.
   *
   * TODO: PART 3 is to serve both files and directories. You will need to
   * determine when to call serve_file() or serve_directory() depending
   * on `path`. Make your edits below here in this function.
   */

  /* PART 2 & 3 BEGIN */
  struct stat buffer;
  (void)stat(path, &buffer);  // Get the stat struct for specific file

  if (S_ISREG(buffer.st_mode)) // It is a regular file
  {
    serve_file(fd, path, buffer.st_size);
  }
  else if (S_ISDIR(buffer.st_mode)) // It is a directory
  {
    /* Assemble the index file path*/
    char* index_file = (char*)malloc(2+strlen(request->path)+11+1); // Null terminate + /index.html
    memcpy(index_file, path, strlen(request->path) + 2); // only copy the ./ without the ending null
    memcpy(index_file + strlen(request->path) + 2, "/index.html", 11);

    /* See if the index file exist in the dir*/
    (void)stat(index_file, &buffer);
#ifdef DEBUG
    printf("the path is : %s\n", path);
    printf("the index_path is : %s\n", index_file);
#endif
    if (S_ISREG(buffer.st_mode))
    {
      serve_file(fd, index_file, buffer.st_size);
    }
    else
    {
      serve_directory(fd, path);
    }
  }
  else
  {
    /*Serve a 404 Not Found response*/
    http_start_response(fd, 404);
    http_end_headers(fd);
  }

  /* PART 2 & 3 END */

  close(fd);
  return;
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
typedef struct _thread_arg {
  int server_fd;
  int client_fd;
} THREAD_ARG;

#define REQUEST_MAX_SIZE 8192
static void client_to_server(void* arg)
{
  THREAD_ARG* ptr = arg;
  //printf("This is cli to srv thread %ld.\n", pthread_self());
  //fflush(stdout);
  relay_byte_stream(ptr->client_fd, ptr->server_fd, REQUEST_MAX_SIZE);
  //char* read_buffer = malloc(REQUEST_MAX_SIZE + 1);
  //int num = 0;
  //int count = 0;
  //do
  //{
  //  num = read(ptr->client_fd, read_buffer, REQUEST_MAX_SIZE);
  //  count = 0;
  //  while (count != num)
  //  {
  //    count += write(ptr->server_fd, read_buffer, num-count);
  //  }
  //}while(num > 0);
}

static void server_to_client(void* arg)
{
  THREAD_ARG* ptr = arg;
  //printf("This is srv to cli thread %ld.\n", pthread_self());
  //fflush(stdout);
  relay_byte_stream(ptr->server_fd, ptr->client_fd, REQUEST_MAX_SIZE);

  //char* read_buffer = malloc(REQUEST_MAX_SIZE + 1);
  //int num = 0;
  //int count = 0;
  //do
  //{
  //  num = read(ptr->server_fd, read_buffer, REQUEST_MAX_SIZE); 
  //  count = 0;
  //  while (count != num)
  //  {
  //    count += write(ptr->client_fd, read_buffer, num-count);
  //  }
  //}while(num > 0);
}

/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target_fd. HTTP requests from the client (fd) should be sent to the
 * proxy target (target_fd), and HTTP responses from the proxy target (target_fd)
 * should be sent to the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 *
 *   Closes client socket (fd) and proxy target fd (target_fd) when finished.
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and
  * opens a connection to it. Please do not modify.
  */
  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);

  // Use DNS to resolve the proxy target's IP address
  struct hostent* target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  // Create an IPv4 TCP socket to communicate with the proxy target.
  int target_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (target_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    close(fd);
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    close(target_fd);
    close(fd);
    exit(ENXIO);
  }

  char* dns_address = target_dns_entry->h_addr_list[0];
#ifdef DEBUG
  //printf("dns_address is %d\n", target_address.sin_addr.s_addr);
  int i = 0;
  while(target_dns_entry->h_addr_list[i] != 0)
  {
    printf("dns_address is %d, i is %d\n", target_dns_entry->h_addr_list[i], i);
    i++;
  }
#endif

  // Connect to the proxy target.
  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  printf("dns_address is %s\n", inet_ntoa(target_address.sin_addr));
  int connection_status =
      connect(target_fd, (struct sockaddr*)&target_address, sizeof(target_address));

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(target_fd);
    close(fd);
    fprintf(stdout, "Connection failed!");
    return;
  }

  /* TODO: PART 4 */
  /* PART 4 BEGIN */
  fprintf(stdout, "Part 4 started\n");
  fprintf(stdout, "server_fd is %d\n", fd);
  fprintf(stdout, "client_fd is %d\n", target_fd);
  fflush(stdout);
  pthread_t thread1, thread2;
  THREAD_ARG arg;
  arg.server_fd = fd;
  arg.client_fd = target_fd;
  pthread_create(&thread1, NULL, client_to_server, &arg);
  pthread_create(&thread2, NULL, server_to_client, &arg);

  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);
#ifdef DEBUG
    printf("Part 4 ended");
#endif

  /* PART 4 END */
}

#ifdef POOLSERVER
/*
 * All worker threads will run this function until the server shutsdown.
 * Each thread should block until a new request has been received.
 * When the server accepts a new connection, a thread should be dispatched
 * to send a response to the client.
 */
void* handle_clients(void* void_request_handler) {
  void (*request_handler)(int) = (void (*)(int))void_request_handler;
  /* (Valgrind) Detach so thread frees its memory on completion, since we won't
   * be joining on it. */
  pthread_detach(pthread_self());

  /* TODO: PART 7 */
  /* PART 7 BEGIN */

  /* PART 7 END */
}

/*
 * Creates `num_threads` amount of threads. Initializes the work queue.
 */
void init_thread_pool(int num_threads, void (*request_handler)(int)) {

  /* TODO: PART 7 */
  /* PART 7 BEGIN */

  /* PART 7 END */
}
#endif

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int* socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  // Creates a socket for IPv4 and TCP.
  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option, sizeof(socket_option)) ==
      -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  // Setup arguments for bind()
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  /*
   * TODO: PART 1
   *
   * Given the socket created above, call bind() to give it
   * an address and a port. Then, call listen() with the socket.
   * An appropriate size of the backlog is 1024, though you may
   * play around with this value during performance testing.
   */

  /* PART 1 BEGIN */
  bind(*socket_number, (struct sockaddr*) &server_address, sizeof(server_address));
  printf("Binded connection from %s on port %d\n", inet_ntoa(server_address.sin_addr),
           server_address.sin_port);
  listen(*socket_number, 1024);

  /* PART 1 END */
  printf("Listening on port %d...\n", server_port);

#ifdef POOLSERVER
  /*
   * The thread pool is initialized *before* the server
   * begins accepting client connections.
   */
  init_thread_pool(num_threads, request_handler);
#endif

  while (1) {
    client_socket_number = accept(*socket_number, (struct sockaddr*)&client_address,
                                  (socklen_t*)&client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n", inet_ntoa(client_address.sin_addr),
           client_address.sin_port);

#ifdef BASICSERVER
    /*
     * This is a single-process, single-threaded HTTP server.
     * When a client connection has been accepted, the main
     * process sends a response to the client. During this
     * time, the server does not listen and accept connections.
     * Only after a response has been sent to the client can
     * the server accept a new connection.
     */
    request_handler(client_socket_number);

#elif FORKSERVER
    /*
     * TODO: PART 5
     *
     * When a client connection has been accepted, a new
     * process is spawned. This child process will send
     * a response to the client. Afterwards, the child
     * process should exit. During this time, the parent
     * process should continue listening and accepting
     * connections.
     */

    /* PART 5 BEGIN */

    /* PART 5 END */

#elif THREADSERVER
    /*
     * TODO: PART 6
     *
     * When a client connection has been accepted, a new
     * thread is created. This thread will send a response
     * to the client. The main thread should continue
     * listening and accepting connections. The main
     * thread will NOT be joining with the new thread.
     */

    /* PART 6 BEGIN */

    /* PART 6 END */
#elif POOLSERVER
    /*
     * TODO: PART 7
     *
     * When a client connection has been accepted, add the
     * client's socket number to the work queue. A thread
     * in the thread pool will send a response to the client.
     */

    /* PART 7 BEGIN */

    /* PART 7 END */
#endif
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0)
    perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char* USAGE =
    "Usage: ./httpserver --files some_directory/ [--port 8000 --num-threads 5]\n"
    "       ./httpserver --proxy inst.eecs.berkeley.edu:80 [--port 8000 --num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {
  signal(SIGINT, signal_callback_handler);
  signal(SIGPIPE, SIG_IGN);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char* proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char* colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char* server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char* num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

#ifdef POOLSERVER
  if (num_threads < 1) {
    fprintf(stderr, "Please specify \"--num-threads [N]\"\n");
    exit_with_usage();
  }
#endif

  chdir(server_files_directory);
  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
