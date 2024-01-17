/*

1. Entities:
    - resources to be managed by this particular monitor: N_THREADS chopsticks. They are distinguished based on their position at the table.
    - threads using such resources: N_THREADS philosophers. They are distinguished based on where they sit.
2. Monitor’s API
    - void pick_up(monitor_t *mon, int philo)
        acquires chopsticks
    - void put_down(monitor_t *mon, int philo)
        releases chopsticks
3. Identify the priorities in the problem
    - no priorities among threads
    - no priorities among requests
4. Define the queues needed to manage the operation of the monitor
    - if chopsticks not available, need to queue
    - could be one queue overall, where all philosophers can wait
    - however, could optimize number of signals sent if just one queue per philosopher
    - since queues are independent and there will be at most one philosopher per queue, no policy needed
5. Define the data needed to model the state of the monitor
    - enum state[N_THREADS], one per philosopher, to understand if a philosopher is THINKING, HUNGRY, or EATING
        - HUNGRY iff blocked on own condvar
        - EATING iff acquired chopsticks and hasn't released them yet
        - THIKING iff not HUNGRY and not EATING
6. Provide a detailed design of methods
    6.1. Identify alternative cases within method
        - all philosophers functionally identical, but differ in terms of position <-> the chopsticks they need
    6.2. Identify conditions preventing resource acquisition
        - right philo is EATING or left philo is EATING
    6.3. Define signaling/broadcasting upon resource release
        - if right philo is HUNGRY, signal right
        - if left philo is HUNGRY, signal left
    6.4. Define signaling/broadcasting upon resource acquisition
        - no need to signal or broadcast upon resource acquisition
    6.5. Define what the initial state of the monitor should be (initialization)
        - initially state is THINKING for all philosophers
7. Implement the monitor
    7.1. Start from generic template with the basic #includes, main loop, etc. (see this template), such as:
        7.1.1. Data types for readability (e.g.: typedef thread_name_t char[10], ...)
            - One data type will be the monitor itself (monitor_t)
            - A field of monitor_t will be a mutex lock (pthread_mutex_t), to be used to guard access to the monitor's methods
            - The monitor should also include one condvar per each queue
        7.1.2. Constants, defined for readability
            - e.g.: #define N_THREADS 5
        7.1.3. Global data structures (such as the monitor itself)
        7.1.4. Thread vectors, for the purpose of animating the application and validating the monitor implementation 
            - e.g.: thread_name_t names[N_THREADS], pthread_t my_thread_names[N_THREADS]
    7.2. Declare the monitor APIs. Typically:
        - int init(monitor_t*)
        - int destroy(monitor_t*)
        - int acquire(monitor_t*, ...)
        - int release(monitor_t*, ...)
    7.3. Set up the environment for animating the application and validating the correct behavior of the monitor
        7.3.1. Define the main() function. The typical sequence is:
            - initialize the monitor
            - loop to create N_THREADS threads
            - loop to join N_THREADS threads
            - destroy the monitor
        7.3.2. Define the code associated with each thread type. The typical sequence is:
            FOREVER
            {
                - acquire resource using a monitor method
                - do something with the resource (or waste some time, in a simulated environment)
                - release resource using a monitor method
                - do something without the resource (or waste some more time, in a simulated environment)
            }
        7.3.3. Define all other methods and auxiliary functions (e.g. left(i), right(i), display_state(), eat(), …)
        
*/

// TEMPLATE FOR STEP 7.0 //

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>

// CONSTANTS AND MACROS 
// for readability
#define N_THREADS 5
#define FOREVER for(;;)

// DEFINITIONS OF NEW DATA TYPES
// for readability
typedef char thread_name_t[10];
typedef enum {THINKING, HUNGRY, EATING} state_t;
 // monitor also defined as a new data type
typedef struct {
    // monitor lock (one mutex lock per monitor)
    pthread_mutex_t m;
    // condvars (one condvar per queue)
    // best if give meaningful names instead of cv_1, cv_2, ...
    pthread_cond_t chopsticks_available[N_THREADS];
    // state variables
    state_t state[N_THREADS];
} monitor_t;

// GLOBAL VARIABLES
// the monitor should be defined as a global variable
monitor_t mon;


//  MONITOR API
void pick_up(monitor_t *mon, int philo);
void put_down(monitor_t *mon, int philo);
void monitor_init(monitor_t *mon);
void monitor_destroy(monitor_t *mon);

// OTHER FUNCTION DECLARATIONS
// functions corresponding to thread entry points
void *philo(void *arg); 
// spend_some_time could be useful to waste an unknown amount of CPU cycles, up to a given top 
double spend_some_time(int);
// simulate_action_* to be replaced by function names according to application: e.g., pick_up, put_down, ...
pthread_mutex_t om=PTHREAD_MUTEX_INITIALIZER; // mutex lock for atomic message display
void output(int philo, char * s); // displays a message "atomically"
void eat(int philo); // simulates eating
void think(int philo); // simulates thinking
int left(int philo); // left of philo
int right(int philo); // right of philo

// IMPLEMENTATION OF MONITOR API
void pick_up(monitor_t *mon, int philo) {
    pthread_mutex_lock(&mon->m);
        mon->state[philo]=HUNGRY;
        while(!( mon->state[left(philo)]!=EATING && mon->state[right(philo)]!=EATING )) {
            pthread_cond_wait(&mon->chopsticks_available[philo],&mon->m);
        }
        mon->state[philo]=EATING;
	pthread_mutex_unlock(&mon->m);
}

void put_down(monitor_t *mon, int philo) {
	pthread_mutex_lock(&mon->m);
        mon->state[philo]=THINKING;
        if(mon->state[left(philo)]==HUNGRY)
            pthread_cond_signal(&mon->chopsticks_available[left(philo)]);
        if(mon->state[right(philo)]==HUNGRY)
            pthread_cond_signal(&mon->chopsticks_available[right(philo)]);
	pthread_mutex_unlock(&mon->m);
}

void monitor_init(monitor_t *mon) {
	// set initial value of monitor data structures, state variables, mutexes, counters, etc.
    // typically can use default attributes for monitor mutex and condvars
    int i;
    pthread_mutex_init(&mon->m,NULL);
    for(i=0;i<N_THREADS;i++) {
        pthread_cond_init(&mon->chopsticks_available[i],NULL);  // initialize all condvars
    // set all condvar counters to 0
        mon->state[i]=THINKING;
    }
    // initialize whatever other structures
}

void monitor_destroy(monitor_t *mon) {
    int i;
    // set initial value of monitor data structures, state variables, mutexes, counters, etc.
    for(i=0;i<N_THREADS;i++) {
        pthread_cond_destroy(&mon->chopsticks_available[i]);
    }
    pthread_mutex_destroy(&mon->m);
}

// MAIN FUNCTION
int main(void) {
    // thread management data structures
    pthread_t my_threads[N_THREADS];
    thread_name_t my_thread_names[N_THREADS];
    int i;

    // initialize monitor data strcture before creating the threads
	monitor_init(&mon);

    for (i=0;i<N_THREADS;i++) {
     	sprintf(my_thread_names[i],"t%d",i);
        // create N_THREADS thread with same entry point 
        // these threads are distinguishable thanks to their argument (their name: "t1", "t2", ...)
        // thread names can also be used inside threads to show output messages
        pthread_create(&my_threads[i], NULL, philo, my_thread_names[i]);
    }

    for (i=0;i<N_THREADS;i++) {
        pthread_join(my_threads[i], NULL);
    }

    // free OS resources occupied by the monitor after creating the threads
    monitor_destroy(&mon);

    return EXIT_SUCCESS;
}

// TYPE 1 THREAD LOOP
void *philo(void *arg) {
	// local variables definition and initialization
	int philo = atoi(arg+sizeof(char)); // thread_name = f(arg)
	FOREVER { // or any number of times
		think(philo);
		pick_up(&mon, philo);
		eat(philo);
		put_down(&mon, philo);
	}
	pthread_exit(NULL);
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

int left(int philo) {return (philo+N_THREADS-1)%N_THREADS;}
int right(int philo) {return (philo+1)%N_THREADS;}
void output(int philo, char * s) {
    int i;
    pthread_mutex_lock(&om);
        for(i=0;i<philo;i++) 
            putchar('\t');
        printf("philosopher %d %s\n", philo, s);
    pthread_mutex_unlock(&om);
}
void eat(int philo) { 
    output(philo,"started eating");
    spend_some_time(10); 
    output(philo,"finished eating");
}
void think(int philo) { 
    output(philo,"started thinking");
    spend_some_time(20); 
    output(philo,"finished thiking");
}
