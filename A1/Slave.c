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
int terminated = 0; // to terminate writers
int n = 0; // number read from master
int received = 0; // number of writers that have written to buffer

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void display_buffer() 
{
	int i;
	printf("Buffer:\n|");
	for(i=0;i<BUF_SIZE;i++)
		printf(" %d\t|",buffer[i]);
	
	puts("");
}

void *writer(void *arg)
{

	while(!terminated) 
	{
		int target=rand()%BUF_SIZE;

		// update buffer[target]
		pthread_mutex_lock(&m);

			while (!received)
				pthread_cond_wait(&cond, &m);
			received--;
			buffer[target] += n;
			printf("Summing %d, updated buffer[%d] to %d\n", n, target, buffer[target]);
		
		pthread_mutex_unlock(&m);		
	
	}
	puts("Writer terminated. Bye!");

	pthread_exit(NULL);
}

void *read_from_fifo(void *arg) 
{
	int fd_terminated = open("/tmp/terminated", O_RDONLY);	
	if(fd_terminated == -1) 
	{
		perror("Error opening named pipe");
		exit (1);
	}

	while (!terminated)
	{
		if(read(fd_terminated, &terminated, sizeof(int)) == -1) 
		{
			perror("Error reading from pipe");
			exit (1);
		}
	}
	
	close(fd_terminated);

	pthread_exit(NULL);	
}

int main(void) 
{
	// set up variables
	pthread_t tid[N_WRITERS];
	pthread_t tid_read;

	// sanity check
	if(N_WRITERS<1) 
	{
		puts("Bye!");
		exit(1);
	}

	puts("!!!Hello World - I'm the slave!!!");

	// set up seed
	srand(100);

	// set up communication with master
	printf("Waiting for master to be ready...\n");

	int fd = open("/tmp/new_number", O_RDONLY);	
	if(fd == -1) 
	{
		perror("Error opening named pipe");
		exit (1);
	}

	printf("Opened named pipe, master's ready\n");

	// create threads
	for(int i = 0; i < N_WRITERS; i++) 
	{
		if (pthread_create(&tid[i],NULL,writer,NULL) != 0) 
		{
			perror("Error creating thread");
			exit(1);
		}
	}

	// create thread to read from fifo
	
	if (pthread_create(&tid_read,NULL,read_from_fifo,NULL) != 0) 
	{
		perror("Error creating thread");
		exit(1);
	}
	
	// enter wait loop
	while(!terminated) 
	{
		// read number from master
		if(read(fd, &n, sizeof(int)) == -1) 
		{
			perror("Error reading from pipe");
			exit (1);
		}

		// if has read a number, let all threads update the buffer
		received = N_WRITERS;
		printf("Received %d\n", n);
		pthread_cond_broadcast(&cond);
	
		// if instead there are no more numbers to read, terminate
		if(terminated)
		{
			puts("All done. Bye!");
		}
	}

	// tidy up
	for(int i = 0; i < N_WRITERS; i++) 
	{
		if (pthread_join(tid[i],NULL) != 0) 
		{
			perror("Error joining thread");
			exit(1);
		}
	}

	if (pthread_join(tid_read,NULL) != 0) 
	{
		perror("Error joining thread");
		exit(1);
	}

	// destroy all

	close(fd); // close fifo
	pthread_mutex_destroy(&m); // destroy mutex
	pthread_cond_destroy(&cond); // destroy condition variable


	// display content of buffer
	display_buffer();

	return EXIT_SUCCESS;
}


