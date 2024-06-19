#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <number>\n", argv[0]);
        return 1;
    }
    int delay = atoi(argv[1]);
    if (delay <= 0) {
        printf("Invalid number: %s\n", argv[1]);
        return 1;
    }

    time_t start_time;
    time(&start_time); // Get current time
    
    for (int i = 0; i < delay; i++) {
        sleep(1);
        //printf("$");
        //printf("Start time: %s", ctime(&start_time)); // Print starting time
        fflush(stdout);
    }
    printf("\n");
    return 0;
}