gcc -o controller.o controller.c -lpthread -lrt
mpicc -o primary_slave.o primary_slave.c -lpthread -lrt