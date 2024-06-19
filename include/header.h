#include <stdio.h>
#include <sys/types.h> /* sockets */
#include <sys/socket.h> /* sockets */
#include <netinet/in.h> /* internet sockets */
#include <unistd.h> /* read, write, close */
#include <netdb.h> /* get host by addr */
#include <stdlib.h> /* exit */
#include <string.h> /* strlen */
#include <sys/wait.h> /* sockets */
#include <ctype.h> /* toupper */
#include <signal.h> /* signal */
#include <arpa/inet.h> /* htons, htonl, ntohl */
#include <pthread.h>


// job struct
typedef struct {
    int job_id;
    char *job;
    int clientSocket;
} Job;

// buffer 
typedef struct {
    Job *jobs;
    int capacity;
    int count;
    int front;
    int back;
    int concurrency;
    pthread_mutex_t mutex;
    pthread_cond_t notFull;
    pthread_cond_t notEmpty;
    pthread_cond_t activity;
    int active_workers;
} Buffer;

// neccessary arguments used for threads 
typedef struct {
    int newsock;
    Buffer *buffer;
    int *job_id_counter;
    pthread_mutex_t *job_mutex; // * might not be necessary
} Thread_arg;

//function used for deleting jobs from the buffer
int delete_job(Buffer *buffer, int job_id);

//function used for poll command
char *poll_jobs(Buffer *buffer);