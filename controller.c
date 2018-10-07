// Copyright 2018 Adam Robinson

// Permission is hereby granted, free of charge, to any person obtaining a copy of 
// this software and associated documentation files (the "Software"), to deal in the 
// Software without restriction, including without limitation the rights to use, copy, 
// modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, 
// and to permit persons to whom the Software is furnished to do so, subject to the 
// following conditions:

// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF 
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
// OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "mpi_controller.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define MSG_COUNT 1000000
#define MSG_LENGTH 2048


int main(int argc, char ** argv) {
	struct MPIController * inst = createControllerInstance("test_controller", "-n 4 ./primary_slave.o");

	printf("Controller Process Started\n");
	printf("Beginning Benchmark\n");

	char * message = malloc(sizeof(char) * MSG_LENGTH);
	memset(message, 1, sizeof(char) * MSG_LENGTH);


	time_t start = time(NULL);
	for (int i = 0; i < MSG_COUNT; ++i) {
		sendMessage(inst, message, sizeof(char) * MSG_LENGTH, MSG_TYPE_STRING);
	}
	time_t end = time(NULL);

	printf("Summary:\n");
	printf("\tSent %d messages of length %d in %ld seconds\n", MSG_COUNT, MSG_LENGTH, end - start);
	printf("\tData transfer rate: %f Mb/Sec\n", ((MSG_COUNT * 2.0) / (float)(end - start)) / 1000.0);
	printf("\tCall rate: %f calls/Sec\n", MSG_COUNT / (float)(end - start));
	printf("\tCall Latency: %f ms\n", 1000.0 / (MSG_COUNT / (float)(end - start)));
	

	sleep(5);

	destroyInstance(inst);

	printf("Parent process exiting\n");

	return 0;
}