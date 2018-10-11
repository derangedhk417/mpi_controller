# mpi_controller
Library for controlling an MPI process through IPC in Linux. 

As of this writing, the MPI functionality for a process that was not started with mpirun taking control of or joining an existing mpi world is broken. This makes that functionality possible independent of any MPI libraries. It can be used for generic IPC, but it was written with an MPI process in mind. Last benchmark indicated inter-process data transfer rates averaging 200 Mb/s and around 10 microseconds per call. This benchmark was performed on an unusually powerful computer. Please test for yourself.


To run the benchmark, do the following on the command line:

```sh
./build.sh
./controller.o
```

Read through controller.c and primary slave.c to get an understanding of how this library is used. The intended usage is as follows.

**Parent Process Code**
```c
#include "mpi_controller.h"

...

int main() {
  struct MPIController * inst = createControllerInstance(
    "test_controller", "-n 4 ./primary_slave.o");
  // The second argument to this function is the parameters passed into
  // mpirun. In this case, we are telling mpirun to launch primary_slave.o
  // which contains the code under "Child Process Code" below.
  
  sendMessage(inst, "Hello Child", 0, sizeof(char) * 12, MSG_TYPE_STRING);
  destroyInstance(inst);
  return 0;
}
```

**Child Process Code**
```c
#include "mpi_controller.h"

...

int main() {
  if (world_rank == 0) {
    struct MPIController * inst = createChildInstance("test_controller");

    int code;
    int length;
    int type;
    char * message;
    message = recvMessage(inst, &code, &length, &type);
    free(message);
  }
}
```

This example code only sends one message from controller to child. You can send messages both ways though. The `sendMessage` function will block until the receiver verifies that it has received the message. The `recvMessage` function will block until a message is received. Make sure to free the received message when you are done with it. This is not done for you.
