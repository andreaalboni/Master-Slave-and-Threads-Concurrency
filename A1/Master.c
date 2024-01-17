/*
 ============================================================================
 Name        : A1-Master.c
 Author      : Andrea Alboni
 Version     : 1
 Copyright   : For personal use only
 Description : Template for A1's master program
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
#include <sys/sem.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#define TRUE 1
#define MAX 20
#define THRESHOLD 100

int terminated; // flag to signal termination

// semaphores
sem_t *sem_sig;
sem_t *sem_parent_done;

// signal handler
void handler(int signo) 
{
	if(signo==SIGUSR1)
	{
		// send signal to child
		sem_post(sem_sig);
	}
	else 
	{
		// abort
		perror("Wrong signal number");
		abort();
	}
}

int main(void) 
{
	// create fifo for write
	if(mkfifo("/tmp/new_number", 0666) == -1) 
	{
		if (errno != EEXIST) 
		{
			perror("Error creating named pipe");
			exit (1);
		}
	}

	if(mkfifo("/tmp/terminated", 0666) == -1) 
	{
		if (errno != EEXIST) 
		{
			perror("Error creating named pipe");
			exit (1);
		}
	}

	// set up semaphores
	sem_sig = sem_open("/sem_sig", O_CREAT, 0644, 0);
	sem_parent_done = sem_open("/sem_parent_done", O_CREAT, 0644, 0);

	// set up signal handler
	signal(SIGUSR1, handler);

	puts("!!!Hello World - I'm the master!!!");

	// set up seed
	srand(42);

	pid_t pid = fork();
	if(pid==-1) 
	{
		perror("Error creating child");
		exit (1);
	}
	else if(pid==0) 
	{

		sem_wait(sem_parent_done); // wait until the parent is done

		printf("Opening FIFO, waiting for slave to be ready...\n");

		int fd = open("/tmp/new_number", O_WRONLY);
		if(fd == -1) 
		{
			perror("Error opening named pipe");
			exit (1);
		}

		int fd_terminated = open("/tmp/terminated", O_WRONLY);
		if(fd_terminated == -1) 
		{
			perror("Error opening named pipe");
			exit (1);
		}

		printf("Opened named pipe, slave's ready\n");

		int payload, // the payload is the number to send to slave
			accumulator=0; // to keep track of the total sum of sent numbers

		while(!terminated) 
		{
			payload = rand()%MAX;
			accumulator+=payload;

			sem_wait(sem_sig); // wait for signal from kill command

			printf("payload: %d; accumulator: %d\n", payload, accumulator);

			if (write(fd, &payload, 1) == -1)
			{ // send payload to slave
				perror("Error writing to pipe");
				exit (1);
			}

			if(accumulator>THRESHOLD) 
			{
				terminated = TRUE; // terminate successfully
				write(fd_terminated, &terminated, sizeof(int));
			}
		}

		close(fd); // close fifo
		close(fd_terminated);
		puts("All done. Bye!");
	}
	else 
	{
		printf("Type <kill -%d %d> to send payload\n", SIGUSR1, pid);

		sem_post(sem_parent_done); // signal that the parent is done

		wait(NULL); // wait for child to terminate

		puts("Child terminated. Bye!");
	}

	// tidy up semaphores
	sem_close(sem_sig);
	sem_close(sem_parent_done);
	sem_unlink("/sem_sig");
	sem_unlink("/sem_child_ready");
	sem_unlink("/sem_parent_done");
	sem_unlink("/sem_child_done");

	return EXIT_SUCCESS;
}

