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


int main(int argc, char ** argv) {
	struct MPIController * inst = createControllerInstance("test_controller", "-n 4 ./primary_slave");

	char * message = malloc(sizeof(char) * 100);
	memset(message, 0, sizeof(char) * 100);
	strcpy(message, "Hello Child");

	printf("[PARENT] Sending message: %s\n", message);

	sendMessage(inst, message, sizeof(char) * 100, MSG_TYPE_STRING);

	sleep(5);

	destroyInstance(inst);

	printf("Parent process exiting\n");

	return 0;
}