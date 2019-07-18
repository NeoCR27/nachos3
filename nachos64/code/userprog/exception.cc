// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "nachostabla.h"
#include <sys/stat.h>
#include "synch.h"
#include <fcntl.h>
#include "machine.h"
#include <unistd.h>
#include <iostream>
#include <vector>
#include <deque>
#include <map>

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

struct thread{
	int cantidadThreads;
	Semaphore * sem;
};

NachosOpenFilesTable * openFilesTable = new NachosOpenFilesTable();
Semaphore * console = new Semaphore("ConsoleSem", 1);
int currentIndex = 0;
Semaphore ** semaphores = new Semaphore * [32];
Semaphore ** semThreads = new Semaphore * [32];
std::map<long,thread*> threadMap;

void returnFromSystemCall() {
    int pc, npc;
    pc = machine->ReadRegister( PCReg );
    npc = machine->ReadRegister( NextPCReg );
    machine->WriteRegister( PrevPCReg, pc );        // PrevPC <- PC
    machine->WriteRegister( PCReg, npc );           // PC <- NextPC
    machine->WriteRegister( NextPCReg, npc + 4 );   // NextPC <- NextPC + 4
}      // returnFromSystemCall

void Nachos_Halt() {                    // System call 0

        DEBUG('a', "Shutdown, initiated by user program.\n");
        interrupt->Halt();

}       // Nachos_Halt

void Nachos_Exit() {                    // System call 1
	int status = machine->ReadRegister(4);
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	if(status == 0){
		printf("Proceso finalizado con estado %d \n", status);
	}

	if(threadMap.find((long)currentThread) != threadMap.end()){
		printf("Corriendo joins.\n");
		thread * d = threadMap[(long)currentThread];
		for(int i = 0; i < d->cantidadThreads; ++i){
			d->sem->V();
		}
		threadMap.erase((long)currentThread);
	}
	
	Thread *nextThread;
	nextThread = scheduler->FindNextToRun();
	
	if(nextThread != NULL){
		printf("Pasando al siguiente thread %s.\n",nextThread->getName());
		scheduler->Run(nextThread);
	}else{
		printf("Finalizando thread: %s.\n",currentThread->getName());
		currentThread->Finish();
	}
	interrupt->SetLevel(oldLevel);
	
	returnFromSystemCall();
}

void Nachos_ExecThread(void * p) {

	long addr = (long)p;
	int buffer, index;
	char name[128];
	machine->ReadMem(addr, 1, &buffer);
	for (index = 0; buffer != '\0'; ++index)
	{
		name[index] = (char)buffer;
		addr++;
		machine->ReadMem(addr,1,&buffer);
	}
	name[index] = '\0';
	printf("Nombre: %s\n",name);
	OpenFile * executable = fileSystem->Open(name);
  AddrSpace * space;

  if (executable == NULL) {
		printf("Unable to open file %s\n", name);
		return;
  }
    
  space = new AddrSpace(executable);    
  currentThread->space = space;

  delete executable;

  space->InitRegisters();
  space->RestoreState();

  machine->Run();
}

void Nachos_Exec(){		// System call 2
	int addr = machine->ReadRegister(4);
	int buffer, index;
	char name[128];
	machine->ReadMem(addr, 1, &buffer);
	for (index = 0; buffer != '\0'; ++index)
	{
		name[index] = (char)buffer;
		addr++;
		machine->ReadMem(addr,1,&buffer);
	}
	name[index] = '\0';

	OpenFile * file = fileSystem->Open(name);
	if(!file){
		machine->WriteRegister(2, -1);
	}

	Thread * newT = new Thread( "Thread to execute code" );
	newT->space = new AddrSpace(file);
	int spaceId = newT->space->getSpaceId();
	semThreads[spaceId] = new Semaphore("sem", 0);
	scheduler->Run(newT);
	machine->WriteRegister(2, spaceId);
}


void Nachos_Join(){		// System call 3
	int spaceId = machine->ReadRegister(4);
	semThreads[spaceId]->P();
	currentThread->Finish();
}

void Nachos_Create(){		// System call 4
	int addr = machine->ReadRegister(4);
	int buff, index;
	char * buffer = new char [1024];
	machine->ReadMem(addr, 1, &buff);
	for (index = 0; buff != '\0'; ++index)
	{
		buffer[index] = (char)buff;
		addr++;
		machine->ReadMem(addr,1, &buff);
	}
	buffer[index] = '\0';

    fopen(buffer ,"a");

}

void Nachos_Open() {                    // System call 5
/* System call definition described to user
	int Open(
		char *name	// Register 4
	);
*/
	// Read the name from the user memory, see 5 below
	int addr = machine->ReadRegister(4);
	int buffer, index;
	char name[128];
	machine->ReadMem(addr, 1, &buffer);
	for (index = 0; buffer != '\0'; ++index)
	{
		name[index] = (char)buffer;
		addr++;
		machine->ReadMem(addr,1,&buffer);
	}
	name[index] = '\0';
	// Use NachosOpenFilesTable class to create a relationship
	// between user file and unix file
	int file = open(name, O_RDWR);
	if(file >= 0){
		file = openFilesTable->Open(file);
	}
	// Verify for errors
	machine->WriteRegister(2, file);

    returnFromSystemCall();		// Update the PC registers

}       // Nachos_Open

void Nachos_Write() {                   // System call 6

/* System call definition described to user
        void Write(
		char *buffer,	// Register 4
		int size,	// Register 5
		OpenFileId id	// Register 6
	);
*/
	int size = machine->ReadRegister( 5 );	// Read size to write
    int addr = machine->ReadRegister(4);
	int buff, index;
    char * buffer = new char [1024];
	machine->ReadMem(addr, 1, &buff);
	for (index = 0; buff != '\0'; ++index)
	{
		buffer[index] = (char)buff;
		addr++;
		machine->ReadMem(addr,size, &buff);
	}
	buffer[index] = '\0';

    OpenFileId id = machine->ReadRegister( 6 );	// Read file descriptor
	// Need a semaphore to synchronize access to console
	console->P();
	switch (id) {
		case  ConsoleInput:	// User could not write to standard input
			machine->WriteRegister( 2, -1 );
			break;
		case  ConsoleOutput:
			buffer[ size ] = 0;
			printf( "%s", buffer );
		break;
		case ConsoleError:	// This trick permits to write integers to console
			printf( "%d\n", machine->ReadRegister( 4 ) );
			break;
		default:	// All other opened files
			// Verify if the file is opened, if not return -1 in r2
			// Get the unix handle from our table for open files
			// Do the write to the already opened Unix file
			// Return the number of chars written to user, via r2
			if(!openFilesTable->isOpened(id)){
				machine->WriteRegister(2, -1);
			}else{
				int UnixHandleId = openFilesTable->getUnixHandle(id);
				write(UnixHandleId, buffer, index);
				machine->WriteRegister(2, index);
			}
			break;
	}
	// Update simulation stats, see details in Statistics class in machine/stats.cc
	console->V();

    returnFromSystemCall();		// Update the PC registers

}       // Nachos_Write

void Nachos_Read() {                   // System call 7

/* System call definition described to user
        void Write(
		char *buffer,	// Register 4
		int size,	// Register 5
		OpenFileId id	// Register 6
	);
*/
    int size = machine->ReadRegister( 5 );	// Read size to write
    int addr = machine->ReadRegister(4);
	int buff, index;
    char * buffer = new char [1024];
	machine->ReadMem(addr, 1, &buff);
	for (index = 0; buff != '\0'; ++index)
	{
		buffer[index] = (char)buff;
		addr++;
		machine->ReadMem(addr,size, &buff);
	}
	buffer[index] = '\0';

    OpenFileId id = machine->ReadRegister( 6 );	// Read file descriptor
	// Need a semaphore to synchronize access to console
	console->P();
	switch (id) {
		case  ConsoleInput:	// User could not Read to standard input
			machine->WriteRegister( 2, -1 );
			break;
		case  ConsoleOutput:
			buffer[ size ] = 0;
			printf( "%s", buffer );
		break;
		case ConsoleError:	// This trick permits to write integers to console
			printf( "%d\n", machine->ReadRegister( 4 ) );
			break;
		default:	// All other opened files
			// Verify if the file is opened, if not return -1 in r2
			// Get the unix handle from our table for open files
			// Do the read to the already opened Unix file
			// Return the number of chars written to user, via r2
			if(!openFilesTable->isOpened(id)){
				machine->WriteRegister(2, -1);
			}else{
				int UnixHandleId = openFilesTable->getUnixHandle(id);
				int ssize = read(UnixHandleId, buffer, addr);
				if(ssize == -1){
					machine->WriteRegister(2, -1);
				}else{
					machine->WriteRegister(2, ssize);
				}
			}
			break;
	}
	// Update simulation stats, see details in Statistics class in machine/stats.cc
	console->V();

    returnFromSystemCall();		// Update the PC registers

}       // Nachos_Write

void Nachos_Close(){
	OpenFileId id = machine->ReadRegister( 4 );
	openFilesTable->Close(id);
}

void NachosForkThread( void * p ) { // for 64 bits version

    AddrSpace *space;

    space = currentThread->space;
    space->InitRegisters();             // set the initial register values
    space->RestoreState();              // load page table register

// Set the return address for this thread to the same as the main thread
// This will lead this thread to call the exit system call and finish
    machine->WriteRegister( RetAddrReg, 4 );

    machine->WriteRegister( PCReg, (long) p );
    machine->WriteRegister( NextPCReg, (long) p + 4 );

    machine->Run();                     // jump to the user progam

}

void Nachos_Fork() {			// System call 9

	DEBUG( 'u', "Entering Fork System call\n" );
	// We need to create a new kernel thread to execute the user thread
	Thread * newT = new Thread( "child to execute Fork code" );

	// We need to share the Open File Table structure with this new child

	// Child and father will also share the same address space, except for the stack
	// Text, init data and uninit data are shared, a new stack area must be created
	// for the new child
	// We suggest the use of a new constructor in AddrSpace class,
	// This new constructor will copy the shared segments (space variable) from currentThread, passed
	// as a parameter, and create a new stack for the new child
	newT->space = new AddrSpace( currentThread->space );

	// We (kernel)-Fork to a new method to execute the child code
	// Pass the user routine address, now in register 4, as a parameter
	// Note: in 64 bits register 4 need to be casted to (void *)
	newT->Fork( NachosForkThread, (void *)machine->ReadRegister( 4 ) );

	returnFromSystemCall();	// This adjust the PrevPC, PC, and NextPC registers

	DEBUG( 'u', "Exiting Fork System call\n" );
}	// Kernel_Fork

void Nachos_Yield(){
	currentThread->Yield();
}

int Nachos_SemCreate(){
	int initval = machine->ReadRegister(4);
	if(currentIndex == 32){
		machine->WriteRegister(2, -1);
	}else{
		char arr[3] = "";
		sprintf(arr, "%d", currentIndex);
		semaphores[currentIndex] = new Semaphore(arr, initval);
		currentIndex++; 
	}
	return currentIndex;
}

/* SemDestroy destroys a semaphore identified by id */ 
int Nachos_SemDestroy(){
	int semId = machine->ReadRegister(4);
	delete semaphores[semId];
	return 0;
}

/* SemSignal signals a semaphore, awakening some other thread if necessary */
int Nachos_SemSignal(){
	int semId = machine->ReadRegister(4);
	semaphores[semId]->V();
	return 0;
}

/* SemWait waits a semaphore, some other thread may awake if one blocked */
int Nachos_SemWait(){
	int semId = machine->ReadRegister(4);
	semaphores[semId]->P();
	return 0;
}


void ExceptionHandler(ExceptionType which){
    
    int type = machine->ReadRegister(2);
    unsigned int pageNumber;
    switch ( which ) {

      case SyscallException:
          switch ( type ) {
             case SC_Halt:
                Nachos_Halt();             // System call # 0
                break;
             case SC_Exit:
                Nachos_Exit();		       // System call # 1
                break;
             case SC_Exec:
                Nachos_Exec();		   	   // System call # 2
                break;
             case SC_Join:
                Nachos_Join();		   	   // System call # 3
                break;
             case SC_Create:
                Nachos_Create();		   // System call # 4
                break;
             case SC_Open:
                Nachos_Open();             // System call # 5
                break;
             case SC_Read:
                Nachos_Read();             // System call # 6
                break;
             case SC_Write:
                Nachos_Write();             // System call # 7
                break;
             case SC_Close:
                Nachos_Close();             // System call # 8
                break;
             case SC_Fork:
             		Nachos_Fork();				// System call # 9
             		break;
             case SC_Yield:
             		Nachos_Yield();				// System call # 10
             		break;
             case SC_SemCreate:
                Nachos_SemCreate();             // System call # 11
                break;
             case SC_SemDestroy:
                Nachos_SemDestroy();             // System call # 12
                break;
             case SC_SemSignal:
                Nachos_SemSignal();             // System call # 13
                break;
             case SC_SemWait:
                Nachos_SemWait();             // System call # 14
                break;
          }
      		break;
      		
      case PageFaultException:
			printf("PageFaultException found");
      		pageNumber = (machine->ReadRegister ( 39 ));
        	printf(", logic address: %d ", pageNumber);
        	pageNumber /= PageSize;
        	printf(" in page: %d \n", pageNumber);
        	currentThread->space->load(pageNumber);
      		break;
      
      default:
      		printf( "Unexpected exception %d\n", which );
        	ASSERT(false);
        	break;
    
    }
}
