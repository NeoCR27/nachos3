// system.h 
//	All global variables used in Nachos are defined here.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef SYSTEM_H
#define SYSTEM_H
#define NUM_EXEC 32

#include "copyright.h"
#include "utility.h"
#include "thread.h"
#include "scheduler.h"
#include "interrupt.h"
#include "stats.h"
#include "timer.h"
#include "nachostabla.h"

// Initialization and cleanup routines
extern void Initialize(int argc, char **argv); 	// Initialization,
						// called before anything else
extern void Cleanup();				// Cleanup, called when
						// Nachos is done.

extern Thread *currentThread;			// the thread holding the CPU
extern Thread *threadToBeDestroyed;  		// the thread that just finished
extern Scheduler *scheduler;			// the ready list
extern Interrupt *interrupt;			// interrupt status
extern Statistics *stats;			// performance metrics
extern Timer *timer;				// the hardware alarm clock

#ifdef USER_PROGRAM
#include "machine.h"
#include "bitmap.h"
extern Machine* machine;	// user program memory and registers
extern BitMap* MiMapa;
extern BitMap* MemBitMap;
extern BitMap* SWAPBitMap;
extern TranslationEntry* IPT[NumPhysPages];
extern int indexTLBFIFO;
extern int indexTLBSndChc;
extern int indexSWAPSndChc;
extern int indexSWAPFIFO;
extern bool threadFirstTime;
#endif

#ifdef FILESYS_NEEDED 		// FILESYS or FILESYS_STUB 
#include "filesys.h"
extern FileSystem  *fileSystem;
#endif

#ifdef FILESYS
#include "synchdisk.h"
extern SynchDisk   *synchDisk;
#endif

#ifdef NETWORK
#include "post.h"
extern PostOffice* postOffice;
#endif

#ifdef VM
#include "bitmap.h"
extern int pTLB;
extern int pMem;
extern int pSwap;
extern OpenFile *swap;
extern BitMap *memMap;
extern BitMap *swapMap;
extern int *TPI;
#endif

#endif // SYSTEM_H
