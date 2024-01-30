
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>

// library for calculating wait time
#include <time.h>

// CONSTANTS AND MACROS
// for readability
#define N_THREADS 15
#define FOREVER for(;;)
#define BUFFER_SIZE 30 // buffer size
#define MAX_VSIZE 10 // max size of vectors (possible sizes: 3, 5, 10)
#define MAX_ITERATIONS 200
#define WAIT_LOOPS 10
#define MIN_LOOPS 5

//define policies
 #define FVF
// #define SVF
// #define LVF

//#define TEST // uncomment to test wait time

//one of the policies must be defined
#if !defined(FVF) && !defined(SVF) && !defined(LVF)
    #error "One of the policies must be defined"
#endif

//only one of the policies must be defined
#if defined(FVF) && defined(SVF)
    #error "Only one of the policies must be defined"
#endif
#if defined(FVF) && defined(LVF)
    #error "Only one of the policies must be defined"
#endif
#if defined(SVF) && defined(LVF)
    #error "Only one of the policies must be defined"
#endif


// for simplicity, vectors are always size 10 and matrices are 10x10
// then, part of the space will be unused, no big deal
typedef struct vector_t {
	int size;
	int data[MAX_VSIZE];
} vector_t;

typedef struct matrix_t {
	int n, m;
	int data[MAX_VSIZE][MAX_VSIZE];
} matrix_t;

// DEFINITIONS OF NEW DATA TYPES
// for readability
typedef char thread_name_t[10];

// acronyms for policies
typedef enum boolean {FALSE, TRUE} boolean;

// monitor also defined as a new data types
typedef struct monitor_t {
    // shared data to manage
    int buffer[BUFFER_SIZE];
    int in, out;
    // the following integers are for better readability
    int next_size; // the size of the next vector; 0 if empty buffer
    int capacity; // the size of the longest V that can be uploaded to the buffer

    // synchronization variables and states common to all policies
    pthread_mutex_t mutex;
    pthread_cond_t can_download3;
    pthread_cond_t can_download5;
    pthread_cond_t can_download10;
    int n_d3, n_d5, n_d10; // number of threads in the corresponding condition variable (dowload)

    #ifndef FVF
	// synchronization variables and states for SVF and for LVF
    pthread_cond_t can_upload3;
    pthread_cond_t can_upload5;
    pthread_cond_t can_upload10;
    int n_u3, n_u5, n_u10; // number of threads in the corresponding condition variable (upload)
    #endif

    #ifdef FVF
    // synchronization variables and states for FVF
    pthread_cond_t can_upload[N_THREADS];
    boolean turn[N_THREADS];
    int index_in, index_served, n_u; // index of the next thread to upload
    #endif

    #ifdef TEST
    // variable for calculating wait time
    double wait_time;
    int num_wait;
    int num_iter;
    int max_wait;
    #endif

} monitor_t;

// GLOBAL VARIABLES
// the monitor should be defined as a global variable
monitor_t mon;
int next_value=1;

//  MONITOR API
void download(monitor_t *mon, int k, vector_t *V);
void upload(monitor_t *mon, vector_t *V);
void monitor_init(monitor_t *mon);
void monitor_destroy(monitor_t *mon);

// OTHER FUNCTION DECLARATIONS
// functions corresponding to thread entry points
void *thread(void *arg);

// function for control thread
void *print_wait(void *arg);

// spend_some_time could be useful to waste an unknown amount of CPU cycles, up to a given top 
double spend_some_time(int);


// the following functions must only be called inside critical sections
// returns the size of the next vector that can be downloaded from the buffer
int next_size(monitor_t *mon) {
	return mon->next_size;
}

// returns the max length of a buffer that can be uploaded to the vector
int capacity(monitor_t *mon) {
	return mon->capacity;
}

// returns the size of a vector
int size_of(vector_t *V) {
	return V->size+1;
}

// puts a vector in the buffer; assumes that there is enough capacity
void to_buffer(monitor_t *mon, vector_t *V) {
	int i;
	// if buffer is empty, set next_size to size of V's data
	if(mon->next_size==0) 
		mon->next_size=V->size;
	// subtract from the buffer's capacity the size of V
	mon->capacity-=size_of(V);
	// first slot is size of V's data
	mon->buffer[mon->in]=V->size;
	// then copy each value to buffer
	for(i=0;i<V->size;i++) {
		mon->in=(mon->in+1)%BUFFER_SIZE;
		mon->buffer[mon->in]=V->data[i];
	}
	// set in to next empty slot in buffer
	mon->in=(mon->in+1)%BUFFER_SIZE;
	printf("produced V_%d\n", V->size);
}

// takes a vector from the buffer; assumes that the buffer is not empty
void from_buffer(monitor_t *mon, vector_t *V) {
	int i;
	// initialize size with first integer to be read from buffer
	V->size=mon->buffer[mon->out];
	// copy one by one all values from buffer to V
	for(i=0;i<V->size;i++) {
		mon->out=(mon->out+1)%BUFFER_SIZE;
		V->data[i]=mon->buffer[mon->out];
	}
	// set out to next position in buffer
	mon->out=(mon->out+1)%BUFFER_SIZE;
	// increase the buffer's capacity by the size of V
	mon->capacity+=size_of(V);
	// if the buffer is empty, set next_size to 0
	if(mon->capacity==BUFFER_SIZE)
		mon->next_size=0;
	// otherwise, set next_size to the size of the next vector in the buffer
	else
		mon->next_size=mon->buffer[mon->out];
	printf("consumed V_%d\n", V->size);
}

// generate a random vector size
int rand_size() {
	int r;
	r = rand()%3;
	if(r==0) return 3;
	else if(r==1) return 5;
	else return 10;
}

// initialize a vector with consecutive numbers
void init_vector(vector_t *V) {
	int i, size;

	size=rand_size();
	V->size=size;
	for(i=0;i<size;i++)
		V->data[i]=next_value++;
}

// initialize a matrix, more or less randomly
void init_matrix(matrix_t *M, int n, int m) {
	int i,j;
	M->n=n;
	M->m=m;
	for(i=0;i<m;i++) {
		for(j=0;j<n;j++)
			M->data[i][j]=rand()%2?-1:1;
		while(j++<MAX_VSIZE)
			M->data[i][j]=0;
	}
	while(i++<MAX_VSIZE) {
		for(j=0;j<MAX_VSIZE;j++)
			M->data[i][j]=0;
	}
}


void show_vector(vector_t *V) {
	int i;
	printf("vector of size %d:\n", V->size);
	for(i=0;i<V->size;i++)
		printf("%d\t", V->data[i]);
	puts("");
}


void show_matrix(matrix_t *M) {
	int i,j;
	printf("matrix %dx%d:\n", M->m, M->n);
	for(i=0;i<M->m;i++) {
		for(j=0;j<M->n;j++)
			printf("%d\t", M->data[i][j]);
		puts("");
	}
}

// not quite a multiplication, but something similar to it
void multiply(matrix_t *M, vector_t *Vin, vector_t *Vout) {
	int i,j,max=0;
	Vout->size=M->m;
	for(i=0;i<Vout->size;i++)
		Vout->data[i]=0;
	for(i=0;i<M->m;i++) {
		for(j=0;j<M->n;j++)
			Vout->data[i]+=M->data[i][j]*Vin->data[j];
		if(Vout->data[i]>max)
			max=Vout->data[i];
	}
	max = max/2;
	if(max)
		for(i=0;i<M->m;i++)
			Vout->data[i]=(int)(Vout->data[i]/max);
}

// display the state of the monitor
void show_buffer(monitor_t *mon) {
	int i=BUFFER_SIZE-mon->capacity, j=mon->out;
	printf("Remaining capacity: %d (%.0f%%)\nContent:\n", mon->capacity, (double)100*mon->capacity/BUFFER_SIZE);
	while(i>0) {
		printf("%d\t",mon->buffer[j]);
		j=(j+1)%BUFFER_SIZE;
		i--;
	}
	puts("");
}


boolean sanity_check(monitor_t *mon) {
	int steps, index, skip;
	boolean result=TRUE;
	if(mon->next_size==0) {
		if(mon->capacity!=BUFFER_SIZE)
			result = FALSE;
	}
	else {
		steps = BUFFER_SIZE-mon->capacity;
		index = mon->out;
		while(steps>0) {
			printf("sanity_check: position %d", index);
			skip = mon->buffer[index];
			printf(" d_%d\n", skip);
			if(( skip!=3 )&& (skip !=5 )&&( skip != 10) )
				result = FALSE;
			else {
				steps-=(skip+1);
				index=(index+skip+1)%BUFFER_SIZE;
			}
		}
		if(steps!=0)
			result = FALSE;
	}
	return result;
}

// IMPLEMENTATION OF MONITOR API
// download copies a vector of size up to k to V
void download(monitor_t *mon, int k, vector_t *V)
{
    pthread_mutex_lock(&mon->mutex);

    while(mon->next_size == 0 || mon->next_size > k)
    {
        if (k == 3)
        {
            mon->n_d3++;
            pthread_cond_wait(&mon->can_download3, &mon->mutex);
            mon->n_d3--;
        }
        else if (k == 5)
        {
            mon->n_d5++;
            pthread_cond_wait(&mon->can_download5, &mon->mutex);
            mon->n_d5--;
        }
        else if (k == 10)
        {
            mon->n_d10++;
            pthread_cond_wait(&mon->can_download10, &mon->mutex);
            mon->n_d10--;
        }

    }
    from_buffer(mon, V);

    #ifdef SVF

        // Shortest Vector First to upload
        if (mon->n_u3 > 0 && mon->capacity >= 3)
        {
            pthread_cond_broadcast(&mon->can_upload3);
        }
        else if (mon->n_u5 > 0 && mon->capacity >= 5)
        {
            pthread_cond_broadcast(&mon->can_upload5);
        }
        else if (mon->n_u10 > 0  && mon->capacity >= 10)
        {
            pthread_cond_broadcast(&mon->can_upload10);
        }

    #endif

    #ifdef LVF
    
        // Longest Vector First to upload
        if (mon->n_u10 > 0 && mon->capacity >= 10)
        {
            pthread_cond_broadcast(&mon->can_upload10);
        }
        else if (mon->n_u5 > 0 && mon->capacity >= 5)
        {
            pthread_cond_broadcast(&mon->can_upload5);
        }
        else if (mon->n_u3 > 0 && mon->capacity >= 3)
        {
            pthread_cond_broadcast(&mon->can_upload3);
        }

    #endif

    #ifdef FVF

        // First Come First Served to upload
        if (mon->n_u > 0 && mon->capacity >= 10)
        {
            mon->turn[mon->index_served] = TRUE;
            pthread_cond_signal(&mon->can_upload[mon->index_served]);
            mon->index_served = (mon->index_served + 1) % N_THREADS;
        }

    #endif

    pthread_mutex_unlock(&mon->mutex);
}

void upload(monitor_t *mon, vector_t *V) 
{
    pthread_mutex_lock(&mon->mutex);

    #ifndef FVF
        // Shortest Vector First or Longest Vector First 
        while(mon->capacity < size_of(V))
        {   
            #ifdef TEST
            struct timespec start, end;
            clock_gettime(CLOCK_REALTIME, &start);
            #endif

            if (V->size == 10)
            {
                mon->n_u10++;
                pthread_cond_wait(&mon->can_upload10, &mon->mutex);
                mon->n_u10--;
            }
            else if (V->size == 5)
            {
                mon->n_u5++;
                pthread_cond_wait(&mon->can_upload5, &mon->mutex);
                mon->n_u5--;
            }
            else if (V->size == 3)
            {
                mon->n_u3++;
                pthread_cond_wait(&mon->can_upload3, &mon->mutex);
                mon->n_u3--;
            }

            #ifdef TEST
            clock_gettime(CLOCK_REALTIME, &end);
            mon->wait_time += (end.tv_nsec - start.tv_nsec);
            mon->num_wait ++;
            // implement max wait time
            if ((end.tv_nsec - start.tv_nsec) > mon->max_wait)
            {
                mon->max_wait = (end.tv_nsec - start.tv_nsec);
            }
            printf("####################################### Thread %lu waited %lu nanoseconds to upload\n", pthread_self(), (end.tv_nsec - start.tv_nsec));
            printf("\n");
            printf("####################################### Average wait time: %f nanoseconds in %d wait iteration, %d total iteration\n", mon->wait_time / mon->num_wait, mon->num_wait, mon->num_iter);
            printf("\n");
            printf("####################################### Max wait time: %d nanoseconds\n", mon->max_wait);
            #endif
        }
        #ifdef TEST
        mon->num_iter ++;
        #endif
        to_buffer(mon, V);
    
    #endif

    #ifdef FVF

        // First Come First Served
        while( (mon->n_u > 0 && !mon->turn[(mon->index_served-1)%N_THREADS]) || mon->capacity < size_of(V))
        {
            #ifdef TEST
            struct timespec start, end;
            clock_gettime(CLOCK_REALTIME, &start);
            #endif
            
            mon->n_u ++;
            mon->index_in = (mon->index_in + 1) % N_THREADS;
            mon->turn[mon->index_in] = FALSE;
            pthread_cond_wait(&mon->can_upload[mon->index_in], &mon->mutex);
            mon->n_u --;

            #ifdef TEST
            clock_gettime(CLOCK_REALTIME, &end);
            mon->wait_time += (end.tv_nsec - start.tv_nsec);
            mon->num_wait ++;
            // implement max wait time
            if ((end.tv_nsec - start.tv_nsec) > mon->max_wait)
            {
                mon->max_wait = (end.tv_nsec - start.tv_nsec);
            }
            printf("####################################### Thread %lu waited %lu nanoseconds to upload\n", pthread_self(), (end.tv_nsec - start.tv_nsec));
            printf("\n");
            printf("####################################### Average wait time: %f nanoseconds in %d wait iteration, %d total iteration\n", mon->wait_time / mon->num_wait, mon->num_wait, mon->num_iter);
            printf("\n");
            printf("####################################### Max wait time: %d nanoseconds\n", mon->max_wait);
            #endif
        }
        #ifdef TEST
        mon->num_iter ++;
        #endif
        to_buffer(mon, V);
        mon->turn[mon->index_served] = FALSE;
    
    #endif

    // signal the threads that can download
    if (mon->n_d10 > 0 && mon->next_size == 10)
    {
        pthread_cond_signal(&mon->can_download10);
    }
    else if (mon->n_d5 > 0 && mon->next_size == 5)
    {
        pthread_cond_signal(&mon->can_download5);
    }
    else if (mon->n_d3 > 0 && mon->next_size == 3)
    {
        pthread_cond_signal(&mon->can_download3);
    }

    pthread_mutex_unlock(&mon->mutex);
}

void monitor_init(monitor_t *mon)
{
    // initialization of tools commmon to all policies
    pthread_mutex_init(&mon->mutex, NULL);
    pthread_cond_init(&mon->can_download3, NULL);
    pthread_cond_init(&mon->can_download5, NULL);
    pthread_cond_init(&mon->can_download10, NULL);
    mon->n_d3 = 0;
    mon->n_d5 = 0;
    mon->n_d10 = 0;

    #ifndef FVF
    // for SVF and LVF
    pthread_cond_init(&mon->can_upload3, NULL);
    pthread_cond_init(&mon->can_upload5, NULL);
    pthread_cond_init(&mon->can_upload10, NULL);
    mon->n_u3 = 0;
    mon->n_u5 = 0;
    mon->n_u10 = 0;

    #endif

    #ifdef FVF
    // for FVF
    for (int i = 0; i < N_THREADS; i++)
    {
        pthread_cond_init(&mon->can_upload[i], NULL);
        mon->turn[i] = FALSE;
    }
    mon->index_served = 0;
    mon->index_in = -1;
    mon->n_u = 0;

    #endif

    mon->in = 0;
    mon->out = 0;
    mon->next_size = 0;
    mon->capacity = BUFFER_SIZE;

    #ifdef TEST
    // initialize variable for calculating wait time
    mon->wait_time = 0;
    mon->num_wait = 0;
    mon->num_iter = 0;
    mon->max_wait = 0;
    #endif
}


void monitor_destroy(monitor_t *mon) 
{
    pthread_mutex_destroy(&mon->mutex);
    
    #ifndef FVF
    // for SVF and LVF
    pthread_cond_destroy(&mon->can_upload3);
    pthread_cond_destroy(&mon->can_upload5);
    pthread_cond_destroy(&mon->can_upload10);
    pthread_cond_destroy(&mon->can_download3);
    pthread_cond_destroy(&mon->can_download5);
    pthread_cond_destroy(&mon->can_download10);
    #endif

    #ifdef FVF
    // for FVF
    pthread_cond_destroy(&mon->can_upload[N_THREADS]);
    pthread_cond_destroy(&mon->can_download3);
    pthread_cond_destroy(&mon->can_download5);
    pthread_cond_destroy(&mon->can_download10);  
    #endif
}

// MAIN FUNCTION
int main(void) {
    // thread management data structures
    pthread_t my_threads[N_THREADS];
    thread_name_t my_thread_names[N_THREADS];
    int i;

    // initialize monitor data structure before creating the threads
    srand(42);
	monitor_init(&mon);
	printf("Monitor sanity checked %s\n", sanity_check(&mon)?"passed":"failed");

    //fill the buffer with some vectors
    printf("Filling the buffer...\n");
    while(mon.capacity>BUFFER_SIZE/2) {
        vector_t V;
        init_vector(&V);
        to_buffer(&mon,&V);
    }
    printf("Buffer iniziliazed.\n");

	show_buffer(&mon);

	printf("Creating %d threads...\n", N_THREADS);

    // control threads creation
    pthread_t control_thread;
    //pthread_create(&control_thread, NULL, print_wait, NULL);

    for (i=0;i<N_THREADS;i++) {
     	sprintf(my_thread_names[i],"t%d",i);
        // create N_THREADS thread with same entry point 
        // these threads are distinguishable thanks to their argument (their name: "t1", "t2", ...)
        // thread names can also be used inside threads to show output messages
        pthread_create(&my_threads[i], NULL, thread, my_thread_names[i]);
    }

    for (i=0;i<N_THREADS;i++) {
        pthread_join(my_threads[i], NULL);
    }
	printf("Monitor sanity checked %s\n", sanity_check(&mon)?"passed":"failed");


    // free OS resources occupied by the monitor after creating the threads
    monitor_destroy(&mon);

    return EXIT_SUCCESS;
}

// THREAD LOOP
void *thread(void *arg) {
	// local variables definition and initialization
	char *name=(char *)arg;
	// int iterations_left=MAX_ITERATIONS;
	vector_t Vin, Vout; // working vector
	int k=rand_size(),o=rand_size();
	matrix_t M;

	init_matrix(&M,k,o); // initialize matrix, with k rows and o columns
	// show_matrix(&M);

	printf("Thread %s started.\n", name);
	FOREVER { // or any number of times
		download(&mon,k,&Vin);
		// printf("Thread %s downloaded ", name); show_vector(&Vin);

        // //input to go on with the program
        // printf("Press enter to continue\n");
        // getchar();

		multiply(&M,&Vin,&Vout);
		// printf("Thread %s obtained ", name); show_vector(&Vout);

        // //input to go on with the program
        // printf("Press enter to continue\n");
        // getchar();

		upload(&mon,&Vout);
		// printf("Thread %s updated buffer. ", name);
		// printf("Monitor sanity checked %s\n", sanity_check(&mon)?"passed":"failed");
		show_buffer(&mon);
            
        // //input to go on with the program
        // printf("Press enter to continue\n");
        // getchar();

		spend_some_time(MIN_LOOPS+rand()%(WAIT_LOOPS+1)); // optionally, to add some randomness and slow down output
	}
	printf("Thread %s finished.\n", name);

	pthread_exit(NULL);
}

// CONTROL THREAD
void *print_wait(void *arg)
{
    char *name=(char *)arg;
    FOREVER
    {
        #ifdef FVF
        printf("Number of thread in upload waiting queue: %d\n", mon.n_u);
        printf("Number of thread in download waiting queue: list_3= %d list_5= %d list_10=%d\n", mon.n_d3, mon.n_d5, mon.n_d10);
        //show_buffer(&mon);
        #endif

        #ifndef FVF
        printf("Number of thread in upload waiting queue: list_3= %d list_5= %d list_10=%d\n", mon.n_u3, mon.n_u5, mon.n_u10);
        printf("Number of thread in download waiting queue: list_3= %d list_5= %d list_10=%d\n", mon.n_d3, mon.n_d5, mon.n_d10);
        show_buffer(&mon);
        printf("\n");
        #endif

        spend_some_time(MIN_LOOPS+rand()%(WAIT_LOOPS+1));
    }
}

// AUXILIARY FUNCTIONS
double spend_some_time(int max_steps) {
    double x, sum=0.0, step;
    long i, N_STEPS=rand()%(max_steps*1000000);
    step = 1/(double)N_STEPS;
    for(i=0; i<N_STEPS; i++) {
        x = (i+0.5)*step;
        sum+=4.0/(1.0+x*x);
    }
    return step*sum;
}
