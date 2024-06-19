#include "header.h"

void perror_exit(char *message);
void child_server(int newsock);
void sigchld_handler(int sig);
void* worker_thread(void *arg);
void* controller_thread(void *arg);

int concurrency_level = 1;      // initial concurrency
int exitFlag = 0;               // flag used when exit command is sent
int threadPoolSize;

// initializing mutexes and cond variables
pthread_mutex_t job_id_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t concurrency_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t exit_cond = PTHREAD_COND_INITIALIZER;
pthread_t *workers;

// initialize buffer
void initBuffer(Buffer *buffer, int capacity) {
    buffer->front = 0;
    buffer->back = -1;
    buffer->count = 0;
    buffer->capacity = capacity;
    buffer->concurrency = concurrency_level;
    buffer->active_workers = 0;
    buffer->jobs = malloc(capacity * sizeof(Job)); 
    if (buffer->jobs == NULL) {
        perror("Failed to allocate buffer");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->notEmpty, NULL);
    pthread_cond_init(&buffer->notFull, NULL);
    pthread_cond_init(&buffer->activity, NULL);
}

// destroy buffer
void destroyBuffer(Buffer *buffer) {
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->notEmpty);
    pthread_cond_destroy(&buffer->notFull);
    pthread_cond_destroy(&buffer->activity);
    free(buffer->jobs);
}

// add jobs in the buffer
void add_job(Buffer *buffer, Job job) {
    pthread_mutex_lock(&buffer->mutex);
    while (buffer->count == buffer->capacity) {
        pthread_cond_wait(&buffer->notFull, &buffer->mutex);
    }
    buffer->back = (buffer->back + 1) % buffer->capacity;
    buffer->jobs[buffer->back] = job;
    buffer->count++;
    pthread_cond_signal(&buffer->notEmpty);
    pthread_mutex_unlock(&buffer->mutex);
}

// remove jobs from the buffer
Job remove_job(Buffer *buffer) {
    pthread_mutex_lock(&buffer->mutex);
    while (buffer->count <= 0 && exitFlag == 0) { 
        pthread_cond_wait(&buffer->notEmpty, &buffer->mutex);
    }
    Job job;
    if (buffer->count > 0 && exitFlag == 0 ) {
        while((buffer->active_workers >= buffer->concurrency && exitFlag == 0)) {
            pthread_cond_wait(&buffer->activity, &buffer->mutex);
        }
        job = buffer->jobs[buffer->front];
        buffer->front = (buffer->front + 1) % buffer->capacity;
        buffer->count--;
        if(buffer-> count < 0) {
            buffer->back = (buffer->back + 1) % buffer->capacity;
            buffer->count = 0;
        }
        pthread_cond_signal(&buffer->notFull);
    }
    pthread_mutex_unlock(&buffer->mutex);
    return job;
}



int main(int argc, char *argv[]) {
    int job_id = 0;
    int port, sock, newsock, secret;
    struct sockaddr_in server, client;
    socklen_t clientlen;
    struct sockaddr *serverptr = (struct sockaddr *)&server;
    struct sockaddr *clientptr = (struct sockaddr *)&client;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <arg2> <threadPoolSize>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);
    threadPoolSize = atoi(argv[3]);
    int bufferSize = atoi(argv[2]);

    Buffer buffer;
    initBuffer(&buffer,bufferSize);

    workers = malloc(threadPoolSize * sizeof(pthread_t));

    /* Create socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror_exit("socket");
    }

    server.sin_family = AF_INET; /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port); /* The given port */

    /* Bind socket to address */
    if (bind(sock, serverptr, sizeof(server)) < 0) {
        perror_exit("bind");
    }

    /* Listen for connections */
    if (listen(sock, 1000) < 0) {
        perror_exit("listen");
    }

    printf("Listening for connections to port %d\n", port);



    /* Create worker threads */
    for (int i = 0; i < threadPoolSize; i++) {
        if (pthread_create(&workers[i], NULL, &worker_thread, (void *)&buffer) != 0) {
            perror_exit("pthread_create");
        }
    }
    while (1) {
        clientlen = sizeof(client);
        /* accept connection */
        if ((newsock = accept(sock, clientptr, &clientlen)) < 0) {
            perror_exit("accept");
        }

        if (read(newsock, &secret, sizeof(secret)) < 0) {
            perror_exit("read size");
        }

        printf("Accepted connection\n");

        Thread_arg *args = malloc(sizeof(Thread_arg));
        args->newsock = newsock;
        args->buffer = &buffer;
        args->job_id_counter = &job_id;
        args->job_mutex = &job_id_mutex;

        pthread_t controller;
        if (pthread_create(&controller, NULL, &controller_thread, (void *)args) != 0) {
            perror_exit("pthread_create");
        }

        pthread_detach(controller);


        if( secret == 1 ) {
           break;
        }
    }

    for (int i = 0; i < threadPoolSize; i++) {
        if( pthread_join(workers[i], NULL) != 0) {
            perror("failed to join thread\n");
        }
    }
    // handle remaining jobs if they exist ( weren't in the buffer when it terminated)
    char special_response[] = "SERVER TERMINATED BEFORE EXECUTION\n";

    int special_response_size = strlen(special_response) + 1; 
       

    pthread_mutex_lock(&buffer.mutex);
    if (buffer.count > 0) {
        // Calculate the total size needed for the response string
        for (int i = 0; i < buffer.capacity; i++) {
            Job job = buffer.jobs[(buffer.back + i) % buffer.capacity];
            

            if( write(job.clientSocket, &special_response_size, sizeof(int)) < 0 ) {
                perror_exit(" exit write response size in server");
            }  
            if (write(job.clientSocket, special_response, sizeof(special_response)) < 0 ) {
                perror_exit(" exit write response in server");
            }
        }

    }
    pthread_mutex_unlock(&buffer.mutex);


    destroyBuffer(&buffer);
    pthread_exit(0);
    return 0;
}


void *worker_thread(void *arg) {
    Buffer *buffer = (Buffer *)arg;
    // constantly waiting for jobs to execute
    while (1) {
        Job job = remove_job(buffer);
        pthread_mutex_lock(&buffer->mutex);
        buffer->active_workers++;
        pthread_cond_signal(&buffer->activity);
        pthread_mutex_unlock(&buffer->mutex);
        if (exitFlag) {
            break;
        }
        // if id = -1 then the job was deleted so send the message and continue to the next one
        if ( job.job_id == -1 ) {
            char* end_buffer = "Unfortunately job got deleted";
            uint32_t end_buffer_size = strlen(end_buffer) + 1;
            

            write(job.clientSocket, &end_buffer_size, sizeof(end_buffer_size));
            write(job.clientSocket, end_buffer, strlen(end_buffer));

            continue;
        }

        // file handling and executing
        pid_t pid = fork();
        if (pid == 0) { // Child process
            char outputFileName[256];
            snprintf(outputFileName, sizeof(outputFileName), "%d.output", getpid());
            FILE *outputFile = fopen(outputFileName, "w");
            dup2(fileno(outputFile), STDOUT_FILENO);
            fclose(outputFile);

            char *args[] = {"/bin/sh", "-c", job.job, NULL};
            if (execvp(*args, args) < 0) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        } else if (pid > 0) { // Parent process
            if (waitpid(pid, NULL, 0) > 0) {
                pthread_mutex_lock(&buffer->mutex);
                buffer->active_workers--;
                pthread_cond_signal(&buffer->activity);
                pthread_mutex_unlock(&buffer->mutex);
            }

            // Determine the size of the output file
            char outputFileName[256];
            snprintf(outputFileName, sizeof(outputFileName), "%d.output", pid);
            FILE *outputFile = fopen(outputFileName, "r");

            if (outputFile) {
                fseek(outputFile, 0, SEEK_END);
                long fileSize = ftell(outputFile);
                fseek(outputFile, 0, SEEK_SET);

        
                char *fileContent = (char *)malloc(fileSize + 1);
                if (fileContent) {
                    fread(fileContent, 1, fileSize, outputFile);
                    fileContent[fileSize] = '\0'; 
                    uint32_t size = htonl(fileSize + 1);

                    char *buffer = (char *)malloc(150 + size + 1);
                    uint32_t buffer_size = strlen(buffer) + 1;
                    sprintf(buffer, "----- %d output start-----\n%s----- %d output end-----\n",job.job_id, fileContent, job.job_id);

                    write(job.clientSocket, &buffer_size, sizeof(buffer_size));
                    write(job.clientSocket, buffer, strlen(buffer));

                    free(fileContent);
                    free(buffer);
                }

                fclose(outputFile);
                remove(outputFileName);
            }

            close(job.clientSocket);
        } else {
            perror("fork");
        }
    }

    return NULL;
}


void* controller_thread(void *arg) {
    char* parameter;
    char* token;
    int size;
    Thread_arg *args = (Thread_arg *)arg;
    int newsock = args->newsock;
    Buffer *buffer = args->buffer;
    int *job_id_counter = args->job_id_counter;
    pthread_mutex_t *job_id_mutex = args->job_mutex;

    if (read(newsock, &size, sizeof(size)) < 0) {
        perror_exit("reade size in server");
    }
    char *command = (char*)malloc(size * sizeof(char));     
    if( read(newsock, command, size) < 0 ) {
        perror_exit(" read command in server");
    }
    // handle commands
    token = strtok(command, " ");

    if (strcmp(token, "issueJob") == 0) {
        parameter = strtok(NULL, "");
        if (parameter == NULL)
            printf("\"issuejob\" : missing argument\n");
        else {
            pthread_mutex_lock(job_id_mutex);
            int job_id = (*job_id_counter)++;
            pthread_mutex_unlock(job_id_mutex);
            Job job;
            job.job_id = job_id;
            job.job = malloc(strlen(parameter) + 1);
            strcpy(job.job, parameter); // Extract the job from the command
            job.clientSocket = newsock;
            add_job(buffer, job);

             // Send response to client
            char response[256];
            snprintf(response, sizeof(response), "JOB %d %s SUBMITTED\n", job_id, job.job);

            int response_size = strlen(response) + 1;
            if( write(newsock, &response_size, sizeof(int)) < 0 ) {
                perror_exit(" issuejob write response size in server");
            }  
            if( write(newsock, response, strlen(response) + 1) < 0 ) {
                perror_exit(" issuejob write response in server");
            }            
        }
    }

    if (strcmp(token, "setConcurrency") == 0) {
        parameter = strtok(NULL, "");
        if (parameter == NULL) {
            printf("\"setConcurrency\" : missing argument\n");
        } else {
            int new_concurrency = atoi(parameter);

            pthread_mutex_lock(&buffer->mutex);
            buffer->concurrency = new_concurrency;
            // if its bigger than threadpool too then broadcast else just send a signle signal
            if( buffer->concurrency > buffer->active_workers) {          
                pthread_cond_signal(&buffer->activity);
            }
            if( buffer->concurrency > threadPoolSize) {           
                pthread_cond_broadcast(&buffer->activity);
            }
            pthread_mutex_unlock(&buffer->mutex);

            // Send response to client
            char response[] = "Concurrency level set successfully\n";
            int response_size = strlen(response) + 1;
            if (write(newsock, &response_size, sizeof(int)) < 0) {
                perror_exit("setConcurrency write response size in server");
            }
            if (write(newsock, response, response_size) < 0) {
                perror_exit("setConcurrency write response in server");
            }
        }
    }

    if (strcmp(token, "stop") == 0) {
        parameter = strtok(NULL, "");
        if (parameter == NULL) {
            printf("\"stop\" : missing argument\n");
        } else {
            int job_id = atoi(parameter);
            int result = delete_job(buffer, job_id);

            // Send response to client
            char response[256];
            if (result) {
                snprintf(response, sizeof(response), "JOB %d REMOVED\n", job_id);
            } else {
                snprintf(response, sizeof(response), "JOB %d NOTFOUND\n", job_id);
            }

            int response_size = strlen(response) + 1;
            if (write(newsock, &response_size, sizeof(int)) < 0) {
                perror_exit("stop write response size in server");
            }
            if (write(newsock, response, response_size) < 0) {
                perror_exit("stop write response in server");
            }
        }
    }

    if (strcmp(token, "poll") == 0) {
        char *poll_response = poll_jobs(buffer);

        // Send response to client
        int response_size = strlen(poll_response) + 1;
        if (write(newsock, &response_size, sizeof(int)) < 0) {
            perror_exit("poll write response size in server");
        }
        if (write(newsock, poll_response, response_size) < 0) {
            perror_exit("poll write response in server");
        }
        free(poll_response);
    }

    if (strcmp(token, "exit") == 0) {
        pthread_mutex_lock(&buffer->mutex);
        exitFlag = 1;
        pthread_cond_broadcast(&buffer->activity);
        pthread_cond_broadcast(&buffer->notEmpty); // Wake up all worker threads
        pthread_mutex_unlock(&buffer->mutex);
        // Notify client
        char response[] = "SERVER TERMINATED\n";

        int response_size = strlen(response) + 1;

        char special_response[] = "SERVER TERMINATED BEFORE EXECUTION\n";

        int special_response_size = strlen(special_response) + 1; 
       
        pthread_mutex_lock(&buffer->mutex);
        if (buffer->count > 0) {
            // send the response to every job that wasn't executed and was in the buffer
            for (int i = 0; i < buffer->capacity; i++) {
                Job job = buffer->jobs[(buffer->back + i) % buffer->capacity];
                

                if( write(job.clientSocket, &special_response_size, sizeof(int)) < 0 ) {
                    perror_exit(" exit write response size in server");
                }  
                if (write(job.clientSocket, special_response, sizeof(special_response)) < 0 ) {
                    perror_exit(" exit write response in server");
                }
            }

        }
        pthread_mutex_unlock(&buffer->mutex);

        if( write(newsock, &response_size, sizeof(int)) < 0 ) {
            perror_exit(" exit write response size in server");
        }  
        if (write(newsock, response, sizeof(response)) < 0 ) {
            perror_exit(" exit write response in server");
        }
    }
   
    return NULL;
}


int delete_job(Buffer *buffer, int job_id) {
    pthread_mutex_lock(&buffer->mutex);

    // Find the job in the buffer
    int found = 0;
    for (int i = 0; i < buffer->count; ++i) {
        int pos = (buffer->back + i) % buffer->capacity;
        if (buffer->jobs[pos].job_id == job_id) {
            found = 1;
            buffer->jobs[pos].job_id = -1;
            break;
        }
    }
    pthread_mutex_unlock(&buffer->mutex);
    return found;
}




char* poll_jobs(Buffer *buffer) {
    pthread_mutex_lock(&buffer->mutex);


    int response_size = 0;
    size_t job_info_size = 0;
    char* bad_response = "BUFFER IS EMPTY NOTHING TO SEE HERE ";

    if (buffer->count > 0) {
        // Calculate the total size needed for the response string
        for (int i = 0; i < buffer->count; ++i) {
            response_size += snprintf(NULL, 0, "JOB %d: %s\n", buffer->jobs[(buffer->front + i) % buffer->capacity].job_id, buffer->jobs[(buffer->front + i) % buffer->capacity].job);
        }

        // Allocate memory for the response string
        char *response = malloc(response_size + 1); 
        if (response == NULL) {
            perror("malloc failed in poll_jobs");
            exit(EXIT_FAILURE);
        }

        // Fill the response string with job details
        response[0] = '\0'; // Initialize as an empty string
        for (int i = 0; i < buffer->count; ++i) {
            Job job = buffer->jobs[(buffer->front + i) % buffer->capacity];
            job_info_size = snprintf(NULL, 0, "JOB %d: %s\n", job.job_id, job.job);
            char *job_info = malloc(job_info_size + 1);
            if (job_info == NULL) {
                perror("malloc failed for job_info");
                free(response); // Free the previously allocated response string
                exit(EXIT_FAILURE);
            }
            snprintf(job_info, job_info_size + 1, "JOB %d: %s\n", job.job_id, job.job);
            strcat(response, job_info);
            free(job_info); // Free the allocated job_info memory
        }

        pthread_mutex_unlock(&buffer->mutex);
        return response;
    } else {
        pthread_mutex_unlock(&buffer->mutex);
        return bad_response;
    }
}


void perror_exit(char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

