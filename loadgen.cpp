/* run using: ./load_gen localhost <server port> <number of concurrent users>
   <think time (in s)> <test duration (in s)> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>

#include <pthread.h>
#include <sys/time.h>

int time_up;
FILE *log_file;

struct hostent *server;
struct sockaddr_in server_addr;

// user info struct
struct user_info
{
  // user id
  int id;

  // socket info
  int portno;
  char *hostname;
  float think_time;

  // user metrics
  int total_overall;
  int total_count;
  float total_rtt;
  float total_rtt_u;
};

// error handling function
void error(char *msg)
{
  perror(msg);
  // exit(0);
}

// time diff in seconds
float time_diff(struct timeval *t2, struct timeval *t1)
{
  return (t2->tv_sec - t1->tv_sec) + ((double)t2->tv_usec - t1->tv_usec) / 1e6;
  // return (t2->tv_usec - t1->tv_usec);
}

float time_diff_u(struct timeval *t2, struct timeval *t1)
{
  // return (t2->tv_sec - t1->tv_sec) + (t2->tv_usec - t1->tv_usec) / 1e6;
  return (t2->tv_usec - t1->tv_usec);
}

// user thread function
void *user_function(void *arg)
{
  struct user_info *info = (struct user_info *)arg;
  int sockfd, n;
  char buffer[256];
  struct timeval start, end;

  while (1)
  {

    try
    {
      gettimeofday(&start, NULL);
      info->total_overall += 1;
      // printf("\nBefore socket creation\n");
      int sockfd = socket(AF_INET, SOCK_STREAM, 0);
      // printf("sockfd: %d", sockfd);
      if (sockfd < 0)
      {
        printf("Not able to create socket");
        if (time_up)
        {
          break;
        }

        continue;
      }

      if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
      {
        printf("Not able to connect to the server");
        close(sockfd);
        // continue;
      }
      // printf("hetre");
      char inbuf[256];
      char buf[256] = "GET /apart1/ HTTP/1.0\r\n\r\n";
      bzero(inbuf, 256);
      write(sockfd, buf, strlen(buf));
      // printf("\nBefore read");
      int n = read(sockfd, inbuf, 255);

      // printf("inbuf %s", inbuf);
      // printf("\nAfter read");
      close(sockfd);
      gettimeofday(&end, NULL);
      if (n > 0)
      {
        info->total_count += 1;
      }
      info->total_rtt += time_diff(&end, &start);
      info->total_rtt_u += time_diff_u(&end, &start);
      if (time_up)
        break;
      usleep(info->think_time * 1e6);
    }
    catch(...){
      if(sockfd > 0){
        close(sockfd);
      }
    }

  }
  pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
  int user_count, portno, test_duration;
  float think_time;
  char *hostname;

  if (argc != 6)
  {
    fprintf(stderr,
            "Usage: %s <hostname> <server port> <number of concurrent users> "
            "<think time (in s)> <test duration (in s)>\n",
            argv[0]);
    exit(0);
  }

  hostname = argv[1];
  portno = atoi(argv[2]);
  user_count = atoi(argv[3]);
  think_time = atof(argv[4]);
  test_duration = atoi(argv[5]);

  printf("Hostname: %s\n", hostname);
  printf("Port: %d\n", portno);
  printf("User Count: %d\n", user_count);
  printf("Think Time: %f s\n", think_time);
  printf("Test Duration: %d s\n", test_duration);

  /* open log file */
  log_file = fopen("load_gen.log", "w");

  pthread_t threads[user_count];
  struct user_info info[user_count];
  struct timeval start, end;

  /* start timer */
  gettimeofday(&start, NULL);
  time_up = 0;
  server = gethostbyname(hostname);

  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(portno);
  printf("PORT : %d",server_addr.sin_port);
  fflush(stdout);
  bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);
  for (int i = 0; i < user_count; i++)
  {
    /* TODO: initialize user info */
    info[i].id = i;
    info[i].portno = portno;
    info[i].hostname = hostname;
    info[i].think_time = think_time;
    info[i].total_count = 0;
    info[i].total_overall = 0;
    info[i].total_rtt = 0;
    info[i].total_rtt_u = 0;

    /* TODO: create user thread */
    int rc = pthread_create(&threads[i], NULL, user_function, &info[i]);

    fprintf(log_file, "Created thread %d\n", i);
  }

  /* TODO: wait for test duration */
  sleep(test_duration);

  fprintf(log_file, "Woke up\n");

  /* end timer */
  time_up = 1;
  gettimeofday(&end, NULL);

  /* TODO: wait for all threads to finish */
  int total_requests = 0;
  double total_rtt = 0;
  double total_rtt_u = 0;
  int total_overall = 0;
  for (int i = 0; i < user_count; ++i)
  {
    /* TODO: create user thread */
    int rc = pthread_join(threads[i], NULL);
    fprintf(log_file, "total_overall<%d>: %d\n", i, info[i].total_overall);
    fprintf(log_file, "total_requests<%d>: %d\n", i, info[i].total_count);
    fprintf(log_file, "total_rtt<%d>: %f\n", i, info[i].total_rtt);
    fprintf(log_file, "total_rtt_u<%d>: %f\n", i, info[i].total_rtt_u);
    total_requests += info[i].total_count;
    total_overall += info[i].total_overall;
    total_rtt += info[i].total_rtt;
    total_rtt_u += info[i].total_rtt_u;
  }
  fprintf(log_file, "All threades done.\n");

  /* TODO: print results */

  printf("total_overall: %d\n", total_overall);
  printf("total_requests: %d\n", total_requests);
  printf("total_rtt: %f\n", total_rtt);
  // printf("total_rtt_u: %f\n", total_rtt_u);

  fprintf(log_file, "total_overall: %d\n", total_overall);
  fprintf(log_file, "total_requests: %d\n", total_requests);
  fprintf(log_file, "total_rtt: %f\n", total_rtt);
  fprintf(log_file, "total_rtt_u: %f\n", total_rtt_u);

  /* close log file */
  fclose(log_file);

  return 0;
}