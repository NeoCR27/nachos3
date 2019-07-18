// addrspace.h 
//	Data structures to keep track of executing user programs 
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"
#include "noff.h"
#include <string>
#define UserStackSize		1024 	// increase this as necessary!

class AddrSpace {
public:

	AddrSpace(OpenFile *executable, std::string filename = "NULL" );
					// initializing it with the program
					// stored in the file "executable"
	AddrSpace(AddrSpace *addrspace);		
	~AddrSpace();			// De-allocate an address space

	void InitRegisters();		// Initialize user-level CPU registers,
					// before jumping to user code

	void SaveState();			// Save/restore address space-specific
	void RestoreState();		// info on a context switch
	int getSpaceId(); 
	NoffHeader NoffH;
  	unsigned int numPages;		// Number of pages in the virtual 
	static const int code = 0;
	unsigned int data;
	unsigned int initData;
	unsigned int noInitData;
	unsigned int stack;				// address space
	std::string filename;
	void load(unsigned int vpn);

private:
	int spaceId;
	TranslationEntry *pageTable;	// Assume linear page table translation
	void saveVictimData(int indexTLB, int prevUse); 
	void memPrincipal(unsigned int vpn);
	void swap(unsigned int vpn);
	void saveToSwap(int physicalPageVictim);
	void readFromSwap(int physicalPage , int swapPage);
	void cleanPages(int physicalPage);
	int  nextSecondChance();
	void useTLBIndex(int indexTLB, int vpn);
	int  getNextSCSWAP();
	void updateInfoVictimSwap(int swapIndex);

};

#endif // ADDRSPACE_H
