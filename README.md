# mpi_controller
Library for controlling an MPI process through IPC in Linux. 

As of this writing, the MPI functionality for a process that was not started with mpirun taking control of or joining an existing mpi world is broken. This makes that functionality possible independent of any MPI libraries. It can be used for generic IPC, but it was written with an MPI process in mind. Last benchmark indicated inter-process data transfer rates averaging 200 Mb/s and around 10 microseconds per call.
