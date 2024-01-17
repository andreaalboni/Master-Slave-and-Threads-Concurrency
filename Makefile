all:
	rm  -f *.o Slave Master
	g++ Master.c -o Master -lpthread
	g++ Slave.c -o Slave -lpthread

prepare1:
	g++ Master.c -o Master -lpthread

run1:
	./Master

prepare2:
	g++ Slave.c -o Slave -lpthread 

run2:
	./Slave

clean:
	rm  -f *.o Slave Master