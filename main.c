#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <math.h> // -lm on linking
#include <netinet/in.h>
#include <pthread.h> // -lpthread on linking
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sysmacro.h"       // no C file needed
#include "unboundedqueue.h" // C file linked in Makefile

#define MAX_FILENAME_SIZE 256
#define MAX_PATH_SIZE 1024
#define BUF_SIZE MAX_FILENAME_SIZE + MAX_PATH_SIZE + 256

// Calculate amount, avg and std.dev of the numbers in FILE, copy them in DEST
static void evaluate_file(char *dest, char *file)
{
   FILE *fd;
   // open file
   EXIT_ERROR((fd = fopen(file, "r")) == NULL, "Client: Failed to open file");
   int count = 0, check;
   float num, sum = 0, sum_squares = 0;
   // save count and sums for every integer/float number
   while ((check = fscanf(fd, "%f", &num)) == 1)
   {
      count++;
      sum += num;
      sum_squares += num * num;
   }
   EXIT_ERROR(!feof(fd), "Client: Failed to read data")
   EXIT_ERROR(fclose(fd) == EOF, "Client: Failed to close file");

   float n = (float)count;
   float avg = 0;
   int squared_sum = sum * sum;
   if (count > 0)
      avg = sum / n;

   // standard deviation = sqrt((sum of squared data - squared sum of data/n)/n)
   float std_dev = 0;
   if (count > 1)
      std_dev = sqrt((sum_squares - (squared_sum / n)) / n);
   char buf[strlen(file) + 256];
   memset(buf, 0, sizeof(buf)); // clean buf
   int len = sprintf(buf, "%d\t%.2f\t%.2f\t%s", count, avg, std_dev, file);
   strncpy(dest, buf, len + 1);
   return;
}

// Search recursively in DIR every .dat file and push pathnames into Q
static void add_files(char *dir, Queue_t *q)
{
   DIR *d;
   struct dirent *e;
   char path[MAX_PATH_SIZE + MAX_FILENAME_SIZE];
   // open DIR and iterate files
   EXIT_ERROR((d = opendir(dir)) == NULL, "Master: Failed to open directory");
   while ((e = readdir(d)) != NULL)
   {
      if (e->d_name[0] == '.') // . and .. are skipped
         continue;
      EXIT_ERROR(sprintf(path, "%s/%s", dir, e->d_name) < 0, "Master: Failed to use path buffer"); // combine DIR and name
      if (e->d_type == DT_DIR)
         add_files(path, q); // nested directories
      else if (e->d_type == DT_REG)
      {
         if (strlen(e->d_name) > 4 && strcmp(e->d_name + strlen(e->d_name) - 4, ".dat") == 0) // only .dat files
         {
            char *file = (char *)malloc((strlen(path) + 1) * sizeof(char));
            strncpy(file, path, strlen(path) + 1);
            IFERROR(push(q, file), "Master: Failed to push"); // push DIR/name.dat in Q
         }
      }
   }
   IFERROR(closedir(d), "Master: Failed to close directory");
}

// Every time a fd gets closed, returns max{fd<=MAX} in SET
static int update_set_max(int max, fd_set *set)
{
   while (!FD_ISSET(max, set))
   {
      max--;
   }
   return max;
}

// Empty handler to override default action of SIGUSR1
static void sigusr1_handler(int signum) {}

// Handler function for client threads
void *client(void *arg)
{
   // cast args
   Queue_t *q = (Queue_t *)arg;

   // create connection socket
   int server;
   IFERROR(server = socket(AF_INET, SOCK_STREAM, 0), "Client: Failed to create socket");
   struct sockaddr_in sin;
   sin.sin_family = AF_INET;
   sin.sin_port = htons(19052);
   sin.sin_addr.s_addr = inet_addr("127.0.0.1");

   IFERROR(connect(server, (struct sockaddr *)&sin, sizeof(sin)), "Client: Failed to connect to server");
   while (1)
   {
      char *file = (char *)pop(q); // pathname of file from queue
      int len = strlen(file);
      char line[len + 256];
      memset(line, 0, sizeof(len)); // clean line
      // take termination string from queue and send it to collector
      if (strcmp(file, "Ended") == 0)
      {
         IFERROR(write(server, file, 6 * sizeof(char)), "Client: Failed to write Ended");
         IFERROR(read(server, line, 3), "Client: Failed to get answer"); // delivery receipt from collector
         free(file);
         break;
      }
      evaluate_file(line, file);
      free(file);
      // send line to collector and wait for delivery receipt
      IFERROR(write(server, line, (strlen(line) + 1) * sizeof(char)), "Client: Failed to write line");
      IFERROR(read(server, line, 3), "Client: Failed to get answer");
   }
   IFERROR(close(server), "Client: Failed to close server");
   return NULL;
}

int main(int argc, char *argv[]) // arg1 dirname, arg2 W
{
   printf("n\tavg\tstd\tfile\n");
   printf("--------------------------------------------------------------------------\n");

   // check right args
   if (argc < 3)
   {
      printf("Not enough arguments, format: '%s directory W'\n", argv[0]);
      exit(EXIT_FAILURE);
   }

   // fork processes: master and collector
   pid_t f;
   IFERROR(f = fork(), "Failed to fork");
   if (f != 0)
   {
      // master process

      // sigmask for SIGUSR1: set handler and block until sigwait
      sigset_t set;
      sigfillset(&set);
      IFERROR(pthread_sigmask(SIG_SETMASK, &set, NULL), "Master: Failed to mask signals");
      struct sigaction sa;
      memset(&sa, 0, sizeof(sa)); // clean sa
      sa.sa_flags = 0;
      sa.sa_handler = sigusr1_handler;
      IFERROR(sigaction(SIGUSR1, &sa, NULL), "Master: Failed to change action of SIGUSR1");
      IFERROR(pthread_sigmask(SIG_UNBLOCK, &set, NULL), "Master: Failed to unmask signals");
      sigemptyset(&set);
      sigaddset(&set, SIGUSR1);
      IFERROR(pthread_sigmask(SIG_BLOCK, &set, NULL), "Master: Failed to mask SIGUSR1");

      // save args
      char *dir = (char *)malloc((strlen(argv[1]) + 1) * sizeof(char));
      strcpy(dir, argv[1]);
      int W = atoi(argv[2]);

      // create temporary socket
      int temp_skt;
      IFERROR(temp_skt = socket(AF_INET, SOCK_STREAM, 0), "Master: Failed to create W socket");
      struct sockaddr_in sin;
      sin.sin_family = AF_INET;
      sin.sin_port = htons(19052);
      sin.sin_addr.s_addr = inet_addr("127.0.0.1");

      // wait for SIGUSR1 from collector (when socket has been created)
      int status;
      if (sigwait(&set, &status) != 0)
      {
         perror("Master: Wrong signal received");
         exit(EXIT_FAILURE);
      }

      // connect to server
      IFERROR(connect(temp_skt, (struct sockaddr *)&sin, sizeof(sin)), "Master: Failed to connect W socket");

      // send W to collector and close temporary socket
      IFERROR(write(temp_skt, &W, sizeof(int)), "Master: Failed to write W");
      close(temp_skt);

      // create queue for threads
      Queue_t *q;
      EXIT_ERROR((q = initQueue()) == NULL, "Master: Failed to init queue");

      // create W threads
      pthread_t tid[W];
      for (int i = 0; i < W; i++)
      {
         pthread_create(&tid[i], NULL, client, q);
      }

      // file search and push in queue
      add_files(dir, q);

      // create W terminating strings "Ended" and push in queue
      for (int i = 0; i < W; i++)
      {
         char *end = (char *)malloc(6 * sizeof(char));
         strcpy(end, "Ended");
         IFERROR(push(q, end), "Master: Failed to push in queue");
      }

      // join threads and free allocated memory
      for (int i = 0; i < W; i++)
      {
         pthread_join(tid[i], NULL);
      }
      deleteQueue(q);
      free(dir);
   }
   else
   {
      // server collector

      // create server socket
      int server;
      IFERROR(server = socket(AF_INET, SOCK_STREAM, 0), "Server: Failed to create socket");
      struct sockaddr_in sin;
      sin.sin_family = AF_INET;
      sin.sin_port = htons(19052);
      sin.sin_addr.s_addr = inet_addr("127.0.0.1");

      // set port and address as reusable
      int optval = 1;
      setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

      // bind socket to port and listen
      IFERROR(bind(server, (struct sockaddr *)&sin, sizeof(sin)), "Server: Failed to bind");
      IFERROR(listen(server, SOMAXCONN), "Server: Failed to listen");
      // send SIGUSR1 to parent process
      IFERROR(kill(getppid(), SIGUSR1), "Server: Failed to send signal to parent");

      // receive temporary socket and read W
      int temp_skt, W;
      IFERROR(temp_skt = accept(server, NULL, NULL), "Server: Failed to accept W");
      IFERROR(read(temp_skt, &W, sizeof(int)), "Server: Failed to read W");
      close(temp_skt);

      // create sets of file descriptors for clients
      fd_set all_fds, read_fds;
      FD_ZERO(&all_fds);
      FD_SET(server, &all_fds);
      int fd_max = server, current_max, client;
      int closed_fds = 0;
      char buf[BUF_SIZE];
      memset(buf, 0, sizeof(buf)); // clean buf

      while (closed_fds < W)
      {
         read_fds = all_fds;   // reset select fds
         current_max = fd_max; // reset max fd
         char ris[3];          // delivery receipt "Ok"
         strncpy(ris, "Ok", 3);
         IFERROR(select(current_max + 1, &read_fds, NULL, NULL, NULL), "Server: Failed to select"); // select active fds
         for (int i = 0; i <= current_max; i++)
         {
            if (FD_ISSET(i, &read_fds))
            {
               if (i == server) // accept new clients
               {
                  IFERROR(client = accept(server, NULL, NULL), "Server: Failed to accept");
                  FD_SET(client, &all_fds);
                  if (client > fd_max)
                     fd_max = client;
               }
               else // read from old clients
               {
                  int r;
                  IFERROR(r = read(i, buf, sizeof(buf)), "Server: Failed to read");
                  if (r > 0)
                  {
                     IFERROR(write(i, ris, strlen(ris)), "Server: Failed to answer to client"); // send delivery receipt to client
                     if (strcmp(buf, "Ended") == 0)                                             // termination string
                     {
                        FD_CLR(i, &all_fds);
                        close(i);
                        fd_max = update_set_max(fd_max, &all_fds);
                        closed_fds++;
                        continue;
                     }
                     printf("%s\n", buf); // print evaluated line
                  }
                  memset(buf, 0, strlen(buf) * sizeof(char)); // reset buf
               }
            }
         }
      }
      IFERROR(close(server), "Client: Failed to close server");
   }
}
