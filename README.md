Write a master-slave application that receives inputs from a user (via the terminal) and behaves in the following way:

the master process waits to receive a signal from the user; when signaled, it reacts by sending a random integer number between 0 and 19 to the slave and then goes back to waiting for a signal from the user. The master repeats this until the total sum of values sent to the slave exceeds 100, then it terminates;
the slave process manages a buffer of 10 integer variables (initially all 0) in the following way: it first creates 5 "writer" threads, and then waits to receive a number (N) from the master. When the slave receives N, it activates all the writers. Each writer then adds N to one of the variables in the buffer chosen at random. After every writer has updated the chosen variable in the buffer, the slave returns to waiting for another number from the master;
after the master terminates, the slave displays the content of the buffer and then terminates



N_THREADS threads use a buffer to support concurrent operations on vectors of integers.

In particular, each thread is associated with a matrix M of size m x k. The matrix defines the size of the vector that the thread can operate on: so the thread will receive in input a vector of size at most k, and output a vector of size m. Threads fetch the input vector to process from the buffer and then store the result back to the buffer.

The cyclic behaviour of a thread is:
 fetch a vector of size at most k, Vk, from the buffer, using method download(k,Vin)
 produce a vector Vm of size m, using a method multiply(M, Vin, Vout)
 copy Vm to the buffer, using method upload(Vout)
 The buffer is managed as a circular bounded buffer. Vectors can be of three different sizes: 3, 5, and 10
 
Synchronization is as follows:
If a thread requests a vector of max size k, and the next vector in the buffer is longer than k, or the buffer is empty, the thread is blocked until the next there is a suitable vector
If a thread produces a vector of size m, and there is not enough space in the buffer for that vector, the thread is blocked until there is enough space
If more than a thread is waiting to download, threads waiting to process longer vectors have priority
As far as the prioritization of uploads, there are three possible policies:

SVF: if more than a thread is waiting to upload, threads uploading the shortest output vector have priority 
LVF: if more than a thread is waiting to upload, threads uploading the longest output vector have priority
FVF: if more than a thread is waiting to upload, threads are services FCFS
 
Analyze the problem. Use the template provided below to implement the upload and download monitor methods, as described above, as well as the methods to initialize and destroy the monitor. Be sure to avoid any race condition (in other words, the buffer must function correctly). Moreover, concurrency should be preserved: threads should proceed concurrently whenever possible and not block unnecessarily.

Experiment with three different policies, and determine if there are differences among them, in terms of efficiency, starvation or deadlock (you are free to carry out the analysis in the way you consider appropriate)

## Author

  - **Andrea Alboni, Emanuele Monsellato, Giorgio Medico** 
