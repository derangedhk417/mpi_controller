gcc -o controller controller.c -lpthread -lrt
mpicc -o primary_slave primary_slave.c -lpthread -lrt