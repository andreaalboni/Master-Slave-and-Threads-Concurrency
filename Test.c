/*
 ============================================================================
 Name        : A1-Slave.c
 Author      : Andrea Alboni
 Version     : 1
 Copyright   : For personal use only
 Description : Template for A1's slave program
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>

#define TRUE 1
#define N_WRITERS 5
#define BUF_SIZE 10

int buffer[BUF_SIZE]; // threads will have to write here
int terminated; // to terminate writers
int n = 0; // number read from master
int received = 0; // number of writers that have written to buffer

pthread_cond_t cond;
pthread_mutex_t m;

void display_buffer() {
	int i;
	printf("Buffer:\n|");
	for(i=0;i<BUF_SIZE;i++) {
		printf(" %d\t|",buffer[i]);
	}
	puts("");
}

void *writer(void *arg) {

	while(!terminated) {
		int target=rand()%BUF_SIZE;

		// update buffer[target]
		pthread_mutex_lock(&m);

			while (received == 0){ 
				//printf("Waiting for signal\n");
				pthread_cond_wait(&cond, &m); }
			received--;
			buffer[target] += n;
			printf("Summing %d, updated buffer[%d] to %d\n", n, target, buffer[target]);
			//display_buffer();
		
		pthread_mutex_unlock(&m);		
	
	}
	puts("Writer terminated. Bye!");

	pthread_exit(NULL);
}

int main(void) {
	// set up mutex and condition variable
	pthread_mutex_init(&m, NULL);
	pthread_cond_init(&cond, NULL);

	pthread_t tid[N_WRITERS];
	int i, accumulator=0;

	// sanity check
	if(N_WRITERS<1) {
		puts("Bye!");
		exit(1);
	}

	puts("!!!Hello World - I'm the slave!!!"); /* prints !!!Hello World!!! */

	// set up seed
	srand(100);

	// set up communication with master
	printf("Opening FIFO, waiting for master to be ready...\n");

	int fd = open("/tmp/named_pipe", O_RDONLY);
	if(fd==-1) {
		perror("Error opening named pipe");
		exit (1);
	}
	
	printf("Opened named pipe, master's ready\n");

	// create threads
	for(i=0;i<N_WRITERS;i++) {
		if (pthread_create(&tid[i],NULL,writer,NULL) != 0) {
			perror("Error creating thread");
			exit(1);
		}
	}
	
	// enter wait loop
	while(!terminated) {
		// read number from master
		if(read(fd, &n, sizeof(int)) == -1) {
			perror("Error reading from pipe");
			exit (1);
		}

		// if has read a number, let all threads update the buffer
		received = N_WRITERS;
		printf("Received %d\n", n);
		pthread_cond_broadcast(&cond);
	
		// if instead there are no more numbers to read, terminate
		accumulator += n;
		
		if (accumulator >= 100){
			terminated=TRUE;
			puts("All done. Bye!");
		}
	}

	// tidy up
	for(i=0;i<N_WRITERS;i++) {
		if (pthread_join(tid[i],NULL) != 0) {
			perror("Error joining thread");
			exit(1);
		}
	}

	close(fd); // close fifo
	pthread_mutex_destroy(&m); // destroy mutex
	pthread_cond_destroy(&cond); // destroy condition variable


	// display content of buffer
	display_buffer();

	return EXIT_SUCCESS;
}


