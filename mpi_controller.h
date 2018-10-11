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

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <malloc.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>

#define MSG_TYPE_INT    1
#define MSG_TYPE_FLOAT  2
#define MSG_TYPE_DOUBLE 3
#define MSG_TYPE_STRING 4

// Stores all state information for communication between
// a controller process and an MPI world.
struct MPIController {
	char * system_name; // Stores a name, specified when initializing 
	                    // an instance. Should be unique. Used as the
	                    // prefix to the names of the four semaphores
	                    // used for communication.

	bool is_controller;   // TRUE when initialized as controller,
	                      // FALSE when initialized as child.

	sem_t * controllerSent; // Waited on by the child and triggered 
	                        // by the controller when a message is sent.
	sem_t * childReceived;  // Waited on by the controller after a 
	                        // message is sent. The child will trigger
	                        // this when it receives the message.
	                        // This is used so that the controller can
	                        // wait for proper message receipt before
	                        // continuing execution.

	sem_t * childSent;      // Waited on by the parent to receive messages
	                        // from the child.
	sem_t * controllerReceived; // Waited on by the child to ensure that
	                            // messages are received by the parent 
	                            // before execution continues.

	int fd;            // File descriptor attached to the shared memory
	                   // that is used to pass message contents.

	int * messageCode; // Stores the number used to identify the type
	                   // of message being sent. Meaning is used defined.

	int * messageSize; // This is populated with the size of the message 
	                   // everytime a message is passed.
	int * messageType; // This is populated with the message type every
	                   // time a message is passed. See the #define 
	                   // statements at the top of the file.
};

// This function allocates shared memory of the specified
// size and returns a pointer to it. Since the same function
// needs to be called in other processes using the same name
// to get access to the shared memory, a name paramter must 
// be passed.
void * mallocShared(size_t size, char * name) {
	// readable and writeable.
	int protection = PROT_READ | PROT_WRITE;

	int visibility = MAP_SHARED;

	// We need to create a file descriptor for this memory
	// if one has not already been created. This allows
	// other processes to open the same memory by calling
	// the same function with the same name argument.
	int fd = shm_open(name, O_RDWR | O_CREAT, 0777);

	if (fd == -1) {
		printf("%d\n", errno);
		printf("shm_open failed\n");
	}

	struct stat s;

	if (fstat(fd, &s) == -1) {
		printf("fstat failed\n");
	}

	// If the size of the file descriptor doesn't match
	// up, resize it.
	if (s.st_size != size) {
		// This ensures that the size of the memory and
		// the size of the file descriptor match up.
		if (ftruncate(fd, size) == -1) {
			printf("ftruncate failed\n");
		}
	}

	void * result = mmap(NULL, size, protection, visibility, fd, 0);

	if (result == (void *)-1) {
		printf("mmap failed\n");
	}

	return result;
}

// Used to reallocate memory associated with the file
// descriptor that is assigned to the instance. This
// memory is used for passing messages between the 
// controller and the child.
void * reallocShared(size_t size, int fd) {

	// Resize the fd
	if (ftruncate(fd, size) == -1) {
		printf("ftruncate failed\n");
	}

	// readable and writeable.
	int protection = PROT_READ | PROT_WRITE;

	int visibility = MAP_SHARED;

	// reallocate the shared memory
	void * result = mmap(NULL, size, protection, visibility, fd, 0);

	if (result == (void *)-1) {
		printf("mmap failed\n");
		printf("errno: %d\n", errno);
	}

	return result;
}

// Constructs the name that should be used to identify
// the file descriptor for the shared memory used as
// a location for addresses being passed between 
// processes.
// YES, I know the name of this function is confusing.
char * getMessageFDNameLocationFDName(char * base) {
	char * addrFDName = malloc(sizeof(char) * 128);
	memset(addrFDName, 0, sizeof(char) * 128);
	strcat(addrFDName, "/");
	strcat(addrFDName, base);
	strcat(addrFDName, "_fd_message_fd_name");
	return addrFDName;
}

// Constructs the name that should be used to identify
// the file descriptor for the shared memory used as
// a location for message size indicators being passed
// between processes.
char * getMessageSizeFDName(char * base) {
	char * sizeFDName = malloc(sizeof(char) * 128);
	memset(sizeFDName, 0, sizeof(char) * 128);
	strcat(sizeFDName, "/");
	strcat(sizeFDName, base);
	strcat(sizeFDName, "_fd_message_size");
	return sizeFDName;
}

// Constructs the name that should be used to identify
// the file descriptor for the shared memory used as
// a location for message code indicators being passed
// between processes.
char * getMessageCodeFDName(char * base) {
	char * sizeFDName = malloc(sizeof(char) * 128);
	memset(sizeFDName, 0, sizeof(char) * 128);
	strcat(sizeFDName, "/");
	strcat(sizeFDName, base);
	strcat(sizeFDName, "_fd_message_code");
	return sizeFDName;
}

// Constructs the name that should be used to identify
// the file descriptor for the shared memory used as
// a location for message type indicators being passed
// between processes.
char * getMessageTypeFDName(char * base) {
	char * typeFDName = malloc(sizeof(char) * 128);
	memset(typeFDName, 0, sizeof(char) * 128);
	strcat(typeFDName, "/");
	strcat(typeFDName, base);
	strcat(typeFDName, "_fd_message_type");
	return typeFDName;
}

// ----------------------------------------------
// Initialization functions
// ----------------------------------------------
// These two functions need to be called in the 
// controller application and the Rank0 thread
// of the MPI world.
// CreateControllerInstance will call MPIEXEC with 
// specified arguments. At this point
// the Rank0 process should call createChildInstance.
// Once called, it will be able to access
// the shared memory for communication and the
// semphores for synchronization. 


// Called in the controlling program (the one not started with MPIEXEC).
// Will start an MPI world as a child process using MPIEXEC. Will then
// wait for the child process to call createChildInstance with 
// the same name argument. CreateChildInstance will trigger the
// childReceived semaphore and the controller instance will 
// know that the child has started.
// 
// parameters:
//     - name: user defined unique string
//             must be the same in both the controller and child processes
//
//     - MPIArguments: arguments to pass to MPIEXEC
struct MPIController * createControllerInstance(char * name, char * MPIArguments) {
	// We need to do the following:
	//     1) create and instance of MPIController
	//     2) initialize the named semaphores
	//     3) create the shared memory
	//     4) call MPIEXEC
	//     5) wait for the child process to start up 
	//        and trigger its own semaphore

	
	struct MPIController * instance = malloc(sizeof(struct MPIController));


	
	instance->is_controller = true;
	instance->system_name   = name;

	// initialize the semaphores
	
	char * conSentName = malloc(sizeof(char) * 128);
	memset(conSentName, 0, sizeof(char) * 128);
	strcat(conSentName, "/");
	strcat(conSentName, name);
	strcat(conSentName, "_con_sent");

	instance->controllerSent = sem_open(conSentName, O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO, 0); 
	

	char * childRecvName = malloc(sizeof(char) * 128);
	memset(childRecvName, 0, sizeof(char) * 128);
	strcat(childRecvName, "/");
	strcat(childRecvName, name);
	strcat(childRecvName, "_child_recv");
	instance->childReceived = sem_open(childRecvName, O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO, 0);
	

	char * childSentName = malloc(sizeof(char) * 128);
	memset(childSentName, 0, sizeof(char) * 128);
	strcat(childSentName, "/");
	strcat(childSentName, name);
	strcat(childSentName, "_child_sent");
	instance->childSent = sem_open(childSentName, O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO, 0);

	char * conRecvName = malloc(sizeof(char) * 128);
	memset(conRecvName, 0, sizeof(char) * 128);
	strcat(conRecvName, "/");
	strcat(conRecvName, name);
	strcat(conRecvName, "_con_recv");
	instance->controllerReceived = sem_open(conRecvName, O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO, 0);

	// Now that the instance members are initialized, we need 
	// to allocate the shared memory used to pass parameters 
	// between the processes.


	char * msgFDName = getMessageFDNameLocationFDName(name);
	instance->fd = shm_open(msgFDName, O_RDWR | O_CREAT, 0777);
	free(msgFDName);

	char * msgCodeFD = getMessageCodeFDName(name);
	instance->messageCode = mallocShared(sizeof(int), msgCodeFD);
	free(msgCodeFD);

	char * msgSizeFD = getMessageSizeFDName(name);
	instance->messageSize = mallocShared(sizeof(int), msgSizeFD);
	free(msgSizeFD);

	char * msgTypeFD = getMessageTypeFDName(name);
	instance->messageType = mallocShared(sizeof(int), msgTypeFD);
	free(msgTypeFD);

	// now that everything is in place, we can call MPIEXEC.

	// we need to construct the argument string for MPIEXEC.

	char * argString = malloc(sizeof(char) * 2048);
	memset(argString, 0, sizeof(char) * 2048);
	strcat(argString, "mpirun ");
	strcat(argString, MPIArguments);
	strcat(argString, " &"); // Necessary for it to run asynchronously.

	// Start the MPI child processes.
	system(argString);

	// Now we wait on the instance->childReceived member,
	// which will be set by the child process once it 
	// starts up.

	sem_wait(instance->childReceived);

	// If we get to here, the child process has started.
	// Time to return the instance.
	return instance;
}

// Called by the Rank0 process of the MPI world initiated by createControllerInstance.
//
// parameters:
//     - name: user defined unique string
//             must be the same in both the controller and child processes
//
struct MPIController * createChildInstance(char * name) {
	// We need to do the following:
	//     1) create and instance of MPIController
	//     2) get copies of the semaphores from the system
	//        they should have already been initialized by the
	//        controller
	//     3) get a pointer to each chunk of shared memory
	//     4) trigger the instance->childReceived semaphore
	//        to inform the controller that the system has
	//        initialized 


	struct MPIController * instance = malloc(sizeof(struct MPIController));

	instance->is_controller = false;
	instance->system_name   = name;

	// initialize the semaphores

	char * conSentName = malloc(sizeof(char) * 128);
	memset(conSentName, 0, sizeof(char) * 128);
	strcat(conSentName, "/");
	strcat(conSentName, name);
	strcat(conSentName, "_con_sent");

	instance->controllerSent = sem_open(conSentName, 0); 

	char * childRecvName = malloc(sizeof(char) * 128);
	memset(childRecvName, 0, sizeof(char) * 128);
	strcat(childRecvName, "/");
	strcat(childRecvName, name);
	strcat(childRecvName, "_child_recv");
	instance->childReceived = sem_open(childRecvName, 0);

	char * childSentName = malloc(sizeof(char) * 128);
	memset(childSentName, 0, sizeof(char) * 128);
	strcat(childSentName, "/");
	strcat(childSentName, name);
	strcat(childSentName, "_child_sent");
	instance->childSent = sem_open(childSentName, 0);

	char * conRecvName = malloc(sizeof(char) * 128);
	memset(conRecvName, 0, sizeof(char) * 128);
	strcat(conRecvName, "/");
	strcat(conRecvName, name);
	strcat(conRecvName, "_con_recv");
	instance->controllerReceived = sem_open(conRecvName, 0);

	// Now we map the shared memory.

	char * msgFDName = getMessageFDNameLocationFDName(name);
	instance->fd = shm_open(msgFDName, O_RDWR | O_CREAT, 0777);
	free(msgFDName);

	char * msgCodeFD = getMessageCodeFDName(name);
	instance->messageCode = mallocShared(sizeof(int), msgCodeFD);
	free(msgCodeFD);

	char * msgSizeFD = getMessageSizeFDName(name);
	instance->messageSize = mallocShared(sizeof(int), msgSizeFD);
	free(msgSizeFD);

	char * msgTypeFD = getMessageTypeFDName(name);
	instance->messageType = mallocShared(sizeof(int), msgTypeFD);
	free(msgTypeFD);


	// Now we trigger the semaphore to inform the controller
	// that we have succeeded.

	sem_post(instance->childReceived);

	return instance;
}

// Sends a message.
// Can be called on either a child or controller, doesn't matter.
// Internally the function will allocate some shared memory and 
// copy the message to it before triggering a semaphore. The caller
// is responsible for deallocating the message that they pass in.
// This function will halt execution until the receiver confirms
// that they have received the message.
void sendMessage(struct MPIController * instance, void * message, int code, int length, int type) {
	void * sharedMessage = reallocShared(length, instance->fd);
	memcpy(sharedMessage, message, length);

	*instance->messageCode = code;
	*instance->messageSize = length;
	*instance->messageType = type;

	if (instance->is_controller) {
		sem_post(instance->controllerSent);
		sem_wait(instance->childReceived);
	} else {
		sem_post(instance->childSent);
		sem_wait(instance->controllerReceived);
	}

	// Now we free the memory.
	// By this point the receiver will already
	// have it copied to non-shared memory.
	munmap(sharedMessage, length);
}

// Halts until revceiving a message. When a message is received, it 
// will be copied from shared memory into local memory. The returned
// pointer is the responsibility of the caller to free.
void * recvMessage(struct MPIController * instance, int * code, int * length, int * type) {
	// wait for a message to come in
	if (instance->is_controller) {
		sem_wait(instance->childSent);
	} else {
		sem_wait(instance->controllerSent);
	}

	*code   = *instance->messageCode;
	*length = *instance->messageSize;
	*type   = *instance->messageType;

	// Now we need to map the new data coming in
	// so that it can be accessed.
	void * msg = reallocShared(*length, instance->fd);

	// Allocate some process memory for it and copy it into
	// the new memory.
	void * result = malloc(*length);
	memcpy(result, msg, *length);

	if (instance->is_controller) {
		sem_post(instance->controllerReceived);
	} else {
		sem_post(instance->childReceived);
	}

	munmap(msg, *length);

	return result;
}

// Removes all semaphores, frees shared memory and 
// unlinks shared memory file descriptors. Call this in
// the controller program before it exits. Do not call
// in the child program. More than one call might cause
// a problem.
void destroyInstance(struct MPIController * instance) {
	// remove all of the semaphores from the system
	char * conSentName = malloc(sizeof(char) * 128);
	memset(conSentName, 0, sizeof(char) * 128);
	strcat(conSentName, "/");
	strcat(conSentName, instance->system_name);
	strcat(conSentName, "_con_sent");

	char * childRecvName = malloc(sizeof(char) * 128);
	memset(childRecvName, 0, sizeof(char) * 128);
	strcat(childRecvName, "/");
	strcat(childRecvName, instance->system_name);
	strcat(childRecvName, "_child_recv");

	char * childSentName = malloc(sizeof(char) * 128);
	memset(childSentName, 0, sizeof(char) * 128);
	strcat(childSentName, "/");
	strcat(childSentName, instance->system_name);
	strcat(childSentName, "_child_sent");

	char * conRecvName = malloc(sizeof(char) * 128);
	memset(conRecvName, 0, sizeof(char) * 128);
	strcat(conRecvName, "/");
	strcat(conRecvName, instance->system_name);
	strcat(conRecvName, "_con_recv");
	
	sem_unlink(conSentName);
	sem_unlink(childRecvName);
	sem_unlink(childSentName);
	sem_unlink(conRecvName);

	char * msgFDName = getMessageFDNameLocationFDName(instance->system_name);
	shm_unlink(msgFDName);
	free(msgFDName);

	char * msgCodeFD = getMessageCodeFDName(instance->system_name);
	shm_unlink(msgCodeFD);
	free(msgCodeFD);

	char * msgSizeFD = getMessageSizeFDName(instance->system_name);
	shm_unlink(msgSizeFD);
	free(msgSizeFD);

	char * msgTypeFD = getMessageTypeFDName(instance->system_name);
	shm_unlink(msgTypeFD);
	free(msgTypeFD);

	// deallocate the shared memory and the instance itself
	munmap(instance->messageCode, sizeof(int));
	munmap(instance->messageSize, sizeof(int));
	munmap(instance->messageType, sizeof(int));

	free(instance);
}