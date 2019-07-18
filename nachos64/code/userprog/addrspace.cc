// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"


//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory

//- ---------------------------------------------------------------------


AddrSpace::AddrSpace(OpenFile *executable, std::string filename)
{
	NoffHeader noffH;
	unsigned int i, size;
	this->filename = filename;
	executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
	if ((noffH.noffMagic != NOFFMAGIC) &&(WordToHost(noffH.noffMagic) == NOFFMAGIC)){
		SwapHeader(&noffH);
	}
	ASSERT(noffH.noffMagic == NOFFMAGIC);

	// Tamano address space
	size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize; // incrementar el tamano
	
	numPages = divRoundUp(size, PageSize); // Hay que dejar campo para el stack
	size = numPages * PageSize;

	DEBUG('a', "Initializing address space, num pages %d, size %d\n",
	numPages, size);

	pageTable = new TranslationEntry[numPages];
	for (i = 0; i < numPages; i++){
		pageTable[i].virtualPage = i;
		#ifdef VM
		pageTable[i].physicalPage = -1;
		pageTable[i].valid = false;
		#else
		pageTable[i].physicalPage = MiMapa->Find();
		pageTable[i].valid = true;
		#endif
		pageTable[i].use = false;
		pageTable[i].dirty = false;
		pageTable[i].readOnly = false; 
	}

	initData = divRoundUp(noffH.code.size, PageSize);
	noInitData = initData + divRoundUp(noffH.initData.size, PageSize);
	stack = numPages - divRoundUp(UserStackSize,PageSize);

	#ifndef VM
	printf("\n\n\n\t\t Virtual mem is no define\n\n\n");

	// then, copy in the code and data segments into memory

	/* Para el segmento de codigo*/
	int x = noffH.code.inFileAddr;
	int y = noffH.initData.inFileAddr;
	int index;
	int codeNumPages = divRoundUp(noffH.code.size, PageSize);
	int segmentNumPages = divRoundUp(noffH.initData.size, PageSize);

	DEBUG('a', "Initializing code segment, at 0x%x, size %d, number of pages %d\n",
	noffH.code.virtualAddr, noffH.code.size, codeNumPages);

	for (index = 0; index < codeNumPages; ++ index ){
		executable->ReadAt(&(machine->mainMemory[ pageTable[index].physicalPage *PageSize ] ),
		PageSize, x );
		x+=PageSize;
	}

	if(noffH.initData.size > 0){
		DEBUG('a', "Initializing data segment, at 0x%x, size %d\n",
		noffH.initData.virtualAddr, noffH.initData.size);
		for (index = codeNumPages; index < codeNumPages + segmentNumPages; ++ index){
			executable->ReadAt(&(machine->mainMemory[ pageTable[index].physicalPage *PageSize ] ),
			PageSize, y );
			y+=PageSize;
		}
	}
	#endif
}

AddrSpace::AddrSpace(AddrSpace *addrspace)
{
	unsigned int i, size,stackSize;
	
	numPages = addrspace->numPages;
	stackSize = UserStackSize/128;
	size = numPages * PageSize;
	
	DEBUG('a', "Initializing address space, num pages %d, size %d\n",numPages, size);
	
	pageTable = new TranslationEntry[numPages];
	filename = addrspace->filename;
	// Se hace una copia de las paginas
    for (i = 0; i < numPages-stackSize; i++){
		pageTable[i].virtualPage = addrspace->pageTable[i].virtualPage;
		pageTable[i].physicalPage = addrspace->pageTable[i].physicalPage;
		pageTable[i].valid = addrspace->pageTable[i].valid;
		pageTable[i].use = addrspace->pageTable[i].use;
		pageTable[i].dirty = addrspace->pageTable[i].dirty;
		pageTable[i].readOnly = addrspace->pageTable[i].readOnly;
    }
	
	for(i = i; i < numPages; ++i){
		pageTable[i].virtualPage = i;
		pageTable[i].physicalPage = MiMapa->Find();
		pageTable[i].valid = true;
		pageTable[i].use = false;
		pageTable[i].dirty = false;
		pageTable[i].readOnly = false;
	}
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
	for(unsigned int i = 0; i< numPages;i++){
      MiMapa->Clear(pageTable[i].physicalPage);  // Liberar espacios
   }
   delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}


//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//---------------------
void AddrSpace::SaveState()
{
	#ifdef VM
	DEBUG ( 't', "\nSe salva el estado del hilo: %s\n", currentThread->getName() );
	for(int i = 0; i < TLBSize; ++i){
		pageTable[machine->tlb[i].virtualPage].use = machine->tlb[i].use;
		pageTable[machine->tlb[i].virtualPage].dirty = machine->tlb[i].dirty;
	}
	
	machine->tlb = new TranslationEntry[ TLBSize ];
	
	for (int i = 0; i < TLBSize; ++i){
		machine->tlb[i].valid = false;
	}
	#endif
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState()
{
	DEBUG ( 't', "\nSe restaura el estado del hilo: %s\n", currentThread->getName() );
	#ifndef VM
	machine->pageTable = pageTable;
	machine->pageTableSize = numPages;
	#else
	
	machine->tlb = new TranslationEntry[ TLBSize ];
	for (int i = 0; i < TLBSize; ++i){
		machine->tlb[i].valid = false;
	}
	#endif
}

int AddrSpace::getSpaceId(){
    return spaceId;
}

void AddrSpace::cleanPages( int physicalPage )
{
	if(physicalPage < 0 || physicalPage >= NumPhysPages){
		DEBUG( 'v', "Error cleanPages: Invalid physical address: %d\n", physicalPage );
		ASSERT( false );
	}
	//Set physical pages values to 0
	for (int index = 0; index < PageSize; ++index ){
		machine->mainMemory[ physicalPage*PageSize + index ] = 0;
	}
}

void AddrSpace::saveToSwap(int victim){
	int swapPage = SWAPBitMap->Find();

	if (victim < 0 || victim >= NumPhysPages){
		DEBUG('v', "Error saveToSwap : Invalid physical address: %d\n", victim);
		ASSERT( false );
	}
	
	if (swapPage == -1){
		DEBUG( 'v', "Error saveToSwap : Swap space not available\n");
		ASSERT(false);
	}
	OpenFile *swapFile = fileSystem->Open( SWAPFILENAME );
	if( swapFile == NULL ){
		DEBUG( 'v', "Error saveToSwap : could not open swap file\n");
		ASSERT(false);
	}
	IPT[victim]->valid = false;
	IPT[victim]->physicalPage = swapPage;
	swapFile->WriteAt((&machine->mainMemory[victim * PageSize]),PageSize, swapPage * PageSize);
	MiMapa->Clear(indexSWAPFIFO);
	delete swapFile;
}

void AddrSpace::readFromSwap(int physicalPage , int swapPage){
	DEBUG('h', "Reading swap from position: %d\n", swapPage);
	SWAPBitMap->Clear(swapPage);
	OpenFile *swapFile = fileSystem->Open(SWAPFILENAME);
	if (swapPage >=0 && swapPage < SWAPSize == false){
		DEBUG( 'v',"Error readFromSwap: invalid swap position = %d\n", swapPage );
		ASSERT( false );
	}
	if(swapFile == NULL){
		DEBUG( 'v', "could not open swap file\n");
		ASSERT(false);
	}
	if(physicalPage < 0 || physicalPage >= NumPhysPages){
		DEBUG( 'v', "Error readFromSwap: Invalid physical address: %d\n", physicalPage );
		ASSERT( false );
	}
	swapFile->ReadAt((&machine->mainMemory[physicalPage*PageSize]), PageSize, swapPage*PageSize);
	++stats->numPageFaults;
	delete swapFile;
}



int AddrSpace::nextSecondChance(){// Second Chance Algorithm
	// Return next second chance from TLB
	int freeSpace = -1;

	for ( int x = 0; x < TLBSize; ++x ){
		if (machine->tlb[ x ].valid == false) {
			return x;
		}
	}
	bool found = false;
	while(found == false){
		if(machine->tlb[indexTLBSndChc].use == true ){ // Used
			machine->tlb[indexTLBSndChc].use = false; // Set not used
			saveVictimData( indexTLBSndChc, true ); // Save info
		}else{
			found = true;
			freeSpace = indexTLBSndChc;
			saveVictimData( freeSpace, false ); // Save info
		}
		indexTLBSndChc = (indexTLBSndChc+1) % TLBSize;
	}
	if (freeSpace < 0 || freeSpace >= TLBSize ){ // Check pos
		DEBUG('v',"\ngetNextSCTLB: Invalid tlb information\n");
		ASSERT( false );
	}
	return freeSpace;
}


void AddrSpace::useTLBIndex(int indexTLB, int vpn) // Update contents of TLB
{
	if (indexTLB < 0 || indexTLB >= TLBSize){
		DEBUG('v',"\n useindexTLB: indexTLB = %d, vpn = %d\n", indexTLB, vpn);
		ASSERT(false);
	}
	if (vpn < 0 || (unsigned int) vpn >= numPages){
		DEBUG('v',"\n useTLBIndex: indexTLB = %d, vpn = %d\n", indexTLB, vpn);
		ASSERT(false);
	}
	machine->tlb[indexTLB].virtualPage = pageTable[vpn].virtualPage;
	machine->tlb[indexTLB].physicalPage = pageTable[vpn].physicalPage;
	machine->tlb[indexTLB].valid = pageTable[vpn].valid;
	machine->tlb[indexTLB].use = pageTable[vpn].use;
	machine->tlb[indexTLB].dirty = pageTable[vpn].dirty;
	machine->tlb[indexTLB].readOnly = pageTable[vpn].readOnly;
}

void AddrSpace::saveVictimData(int indexTLB, int prevUse)
{
	if (indexTLB < 0 || indexTLB >= TLBSize){ // Revisa rango
		DEBUG('v',"\n saveVictimData: indexTLB = %d\n", indexTLB); 
		ASSERT(false);
	}
	pageTable[machine->tlb[indexTLB].virtualPage].use = (prevUse == 1?prevUse:machine->tlb[indexTLB].use); // Update used bit
	pageTable[machine->tlb[indexTLB].virtualPage].dirty = machine->tlb[indexTLB].dirty; // Update dirty bit
}

int  AddrSpace::getNextSCSWAP()
{
	if (indexSWAPSndChc < 0 || indexSWAPSndChc >= NumPhysPages){
		DEBUG('v', "getNextSCSWAP:: Invalid indexSWAPSndChc value = %d \n", indexSWAPSndChc );
		ASSERT(false);
	}
	int freeSpace = -1;
	bool found = false;

	while (found == false){
		if ( IPT[ indexSWAPSndChc ] == NULL ){ // Revision
			DEBUG('v', "\ngetNextSCSWAP:: Invalid IPT state\n");
			ASSERT( false );
		}

		if (IPT[ indexSWAPSndChc ]->valid == false){ // Bit validacion
			DEBUG('v', "\ngetNextSCSWAP:: Invalid IPT[%d].valid values, is false\n");
			ASSERT( false );
		}

		if ( IPT[ indexSWAPSndChc ]->use == true ){
			IPT[ indexSWAPSndChc ]->use = false;
		}else{
			freeSpace = indexSWAPSndChc;
			found = true;
		}
		indexSWAPSndChc = (indexSWAPSndChc+1) % NumPhysPages;
	}

	if (freeSpace < 0 || freeSpace >= NumPhysPages){
		DEBUG('v',"\ngetNextSCSWAP: Invalid IPT information\n");
		ASSERT( false );
	}
	return freeSpace;
}

void AddrSpace::updateInfoVictimSwap(int swapIndex)
{
	for(int index = 0; index < TLBSize; ++index){
		if(machine->tlb[index].valid && (machine->tlb[index].physicalPage == IPT[swapIndex]->physicalPage)){
			DEBUG('v',"%s\n", "Sí estaba la victim en TLB" );
			machine->tlb[ index ].valid = false; 
			IPT[swapIndex]->use = machine->tlb[ index ].use; 
			IPT[swapIndex]->dirty = machine->tlb[ index ].dirty;
			break;
		}
	}
}

void AddrSpace::memPrincipal(unsigned int vpn){ // If page is invalid and clean
			
		DEBUG('v', "\t1-Page is invalid and clean\n");
		++stats->numPageFaults; // ++pageFaults
		OpenFile* executable = fileSystem->Open(filename.c_str());
		if (executable == NULL){
			DEBUG('v',"could not open file %s\n", filename.c_str());
			ASSERT(false);
		}
		NoffHeader noffH;
		executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
		int freeFrame = 0;
		if(vpn >= 0 && vpn < initData){
			DEBUG('v',"1.1 Page code\n");
			freeFrame = MiMapa->Find();
			if (freeFrame != -1){ // if its valid
				DEBUG('v',"\tFree frame : %d\n", freeFrame );
				pageTable[ vpn ].physicalPage = freeFrame;
				executable->ReadAt(&(machine->mainMemory[ ( freeFrame * PageSize ) ] ),
				PageSize, noffH.code.inFileAddr + PageSize*vpn );
				pageTable[ vpn ].valid = true; 				
				IPT[freeFrame] = &(pageTable[ vpn ]); // Update inverted TLB		
				int tlbSPace = nextSecondChance();
				useTLBIndex( tlbSPace, vpn );

			}else{	
				indexSWAPFIFO = getNextSCSWAP();
				updateInfoVictimSwap( indexSWAPFIFO );

				bool victimDirty = IPT[indexSWAPFIFO]->dirty;

				if (victimDirty){
				
					DEBUG('v',"\tvictim f = %d,l = %d and dirty \n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
					saveToSwap( IPT[indexSWAPFIFO]->physicalPage );
					freeFrame = MiMapa->Find();
					if ( -1 == freeFrame ){
						printf("Invalid frame %d\n", freeFrame );
						ASSERT( false );
					}
					pageTable[ vpn ].physicalPage = freeFrame;
					executable->ReadAt(&(machine->mainMemory[ ( freeFrame * PageSize ) ] ), // Load to memory
					PageSize, noffH.code.inFileAddr + PageSize*vpn );
					pageTable[ vpn ].valid = true; // Update valid bit
					// Update inverted page table
					IPT[ freeFrame ] = &( pageTable[ vpn ] );
					int tlbSPace = nextSecondChance(); // Update TLB
						useTLBIndex( tlbSPace, vpn );
				}else{
					DEBUG('v',"\tvictim f=%d,l=%d and clean\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
					int oldPhysicalPage = IPT[indexSWAPFIFO]->physicalPage;
					IPT[indexSWAPFIFO]->valid = false;
					IPT[indexSWAPFIFO]->physicalPage = -1;
					MiMapa->Clear( oldPhysicalPage );

					freeFrame = MiMapa->Find();
					if (freeFrame == -1){
						printf("Invalid frame %d\n", freeFrame );
						ASSERT( false );
					}
					pageTable[ vpn ].physicalPage = freeFrame;
					executable->ReadAt(&(machine->mainMemory[ ( freeFrame * PageSize ) ] ),
					PageSize, noffH.code.inFileAddr + PageSize*vpn );
					pageTable[ vpn ].valid = true;
					IPT[ freeFrame ] = &(pageTable [ vpn ]);
					int tlbSPace = nextSecondChance();
					useTLBIndex( tlbSPace, vpn );
				}
			}
		}
		else if(vpn >= initData && vpn < noInitData){
			DEBUG('v', "Initialized data page\n");
			freeFrame = MiMapa->Find();

			if (freeFrame != -1 ){ // Frame valido
				DEBUG('v',"Frame valido %d\n", freeFrame );
				pageTable[ vpn ].physicalPage = freeFrame;
				executable->ReadAt(&(machine->mainMemory[ ( freeFrame * PageSize ) ] ),
				PageSize, noffH.code.inFileAddr + PageSize*vpn );
				pageTable[ vpn ].valid = true;
				IPT[freeFrame] = &(pageTable[ vpn ]);
				int tlbSPace = nextSecondChance(); // Update TLB
				useTLBIndex(tlbSPace, vpn);
			}else{
				indexSWAPFIFO = getNextSCSWAP(); // Need a victim to send to swap
				updateInfoVictimSwap( indexSWAPFIFO );
				bool victimDirty = IPT[indexSWAPFIFO]->dirty; // Revisar si la página victim está dirty
				if (victimDirty){
					// Send to swap
					DEBUG('v',"\tvictim f=%d,l=%d and dirty\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
					saveToSwap( IPT[indexSWAPFIFO]->physicalPage );
					freeFrame = MiMapa->Find();
					if ( -1 == freeFrame ){
						printf("Invalid frame %d\n", freeFrame );
						ASSERT( false );
					}
					// set pageTable[ vpn ] to freeFrame
					pageTable[ vpn ].physicalPage = freeFrame;
					// read from executable file
					executable->ReadAt(&(machine->mainMemory[ ( freeFrame * PageSize ) ] ),
					PageSize, noffH.code.inFileAddr + PageSize*vpn );
					// set pages to valid
					pageTable[ vpn ].valid = true;
					// update inverted page table
					IPT[ freeFrame ] = &( pageTable[ vpn ] );
					// Update TLB
					int tlbSPace = nextSecondChance();
					useTLBIndex( tlbSPace, vpn );
				}else{
					//if its not dirty
					DEBUG('v',"\ttvictim f=%d,l=%d and clean\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
						// save old physical page from victim
						int oldPhysicalPage = IPT[indexSWAPFIFO]->physicalPage;
						MiMapa->Clear( oldPhysicalPage );
						// Set valid bit of victim page to false
						IPT[indexSWAPFIFO]->valid = false;
						// Set physical page to -1
						IPT[indexSWAPFIFO]->physicalPage = -1;
						// ask for a new free frame
						freeFrame = MiMapa->Find();
						// validate
						if ( freeFrame == -1 ){
							printf("Invalid frame %d\n", freeFrame );
							ASSERT( false );
						}			
						pageTable[ vpn ].physicalPage = freeFrame;
						executable->ReadAt(&(machine->mainMemory[ ( freeFrame * PageSize ) ] ),
						PageSize, noffH.code.inFileAddr + PageSize*vpn );
						// set valid bit of pageTable[vpn]
						pageTable[ vpn ].valid = true;
						// Update inverted page table
						IPT[ freeFrame ] = &(pageTable [ vpn ]);
						// Update TLB
						int tlbSPace = nextSecondChance();
						useTLBIndex( tlbSPace, vpn );
				}
			}
		}else if(vpn >= noInitData && vpn < numPages){ // Data not initialized
			DEBUG('v',"\t1.3 Data not initialized\n");
			freeFrame = MiMapa->Find();
			DEBUG('v',"\tLooks for a new page\n" );
			if ( freeFrame != -1 )
			{
				pageTable[ vpn ].physicalPage = freeFrame;
				pageTable[ vpn ].valid = true;
				IPT[freeFrame] = &(pageTable[ vpn ]); // Update
				int tlbSPace = nextSecondChance(); // Update
				useTLBIndex( tlbSPace, vpn );
			}else{
				indexSWAPFIFO = getNextSCSWAP();
				updateInfoVictimSwap( indexSWAPFIFO );
				bool victimDirty = IPT[indexSWAPFIFO]->dirty;
				if ( victimDirty ){
					DEBUG('v',"\tvictim f=%d,l=%dand dirty\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
					saveToSwap( IPT[indexSWAPFIFO]->physicalPage );

					// pedir el nuevo marco libre
					freeFrame = MiMapa->Find();
					if ( -1 == freeFrame ){
						printf("Invalid frame %d\n", freeFrame );
						ASSERT( false );
					}

					// Update la página física para la nueva página virtual vpn
					pageTable [ vpn ].physicalPage = freeFrame;
					pageTable [ vpn ].valid = true;

					//actualizo la TLB invertida
					IPT[ freeFrame ] = &(pageTable [ vpn ]);

					//actualizo el tlb
					int tlbSPace = nextSecondChance();
					useTLBIndex( tlbSPace, vpn );
				}else //si no está dirty
				{
					DEBUG('v',"\tvictim f=%d,l=%d limpia\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
					int oldPhysicalPage = IPT[indexSWAPFIFO]->physicalPage;
					IPT[indexSWAPFIFO]->valid = false;
					IPT[indexSWAPFIFO]->physicalPage = -1;
					MiMapa->Clear( oldPhysicalPage );
					cleanPages( oldPhysicalPage );

					// pedir el nuevo marco libre
					freeFrame = MiMapa->Find();

					//verificar la validez del marco
					if ( freeFrame == -1 ) //marco inválido
					{
						DEBUG('v',"Invalid free frame %d\n", freeFrame );
						ASSERT( false );
					}
					pageTable [ vpn ].physicalPage = freeFrame;
					pageTable [ vpn ].valid = true;

					//Se actualiza la TLB invertida
					IPT[ freeFrame ] = &(pageTable [ vpn ]);
					//se actualiza la tlb
					int tlbSPace = nextSecondChance();
					useTLBIndex( tlbSPace, vpn );
				}
			}
		}
		else{
			printf("%s %d\n", "El numero de pagina es invalido!", vpn);
			ASSERT(false);
		}
		delete executable; // Cerrar el archivo

}

void AddrSpace::swap(unsigned int vpn ){ // if invalid and dirty page, use swap
		int freeFrame = 0;
		freeFrame = MiMapa->Find(); // Buscar frame valido
		if(freeFrame != -1){ // Es valido
			DEBUG('v',"%s\n", "Si hay espacio en memoria, solo leemos de SWAP\n" );
			// Update la pagina física de la que leo del swap
			int oldSwapPageAddr = pageTable [ vpn ].physicalPage;
			pageTable [ vpn ].physicalPage = freeFrame;
			readFromSwap( freeFrame, oldSwapPageAddr ); // Cargar
			pageTable [ vpn ].valid = true; // Update su bit de validez
			IPT[ freeFrame ] = &(pageTable [ vpn ]); // Update tabla de páginas invertidas
			int tlbSPace = nextSecondChance(); // Update posicion
			useTLBIndex( tlbSPace, vpn );
		}
		else{
			// Se debe selecionar una página victim para enviar al SWAP
			DEBUG('v',"\n%s\n", "NO hay memoria, es más complejo.\n" );
			indexSWAPFIFO = getNextSCSWAP();
			updateInfoVictimSwap( indexSWAPFIFO );

			bool victimDirty = IPT[indexSWAPFIFO]->dirty;
			if (victimDirty){  // Si está dirty
				DEBUG('v',"\t\t\tvictim f=%d,l=%d dirty\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage );
				saveToSwap(IPT[indexSWAPFIFO]->physicalPage);
				freeFrame = MiMapa->Find();
				if (freeFrame == -1){  // if its dirty
					printf("Invalid free frame %d\n", freeFrame );
					ASSERT( false );
				}

				int oldSwapPageAddr = pageTable [vpn].physicalPage;
				pageTable [vpn].physicalPage = freeFrame;
				readFromSwap(freeFrame, oldSwapPageAddr);
				pageTable [vpn].valid = true;
				// Update inverted TLB
				IPT[freeFrame] = &(pageTable [ vpn ]);

				// Update TLB
				int tlbSPace = nextSecondChance();
				useTLBIndex(tlbSPace, vpn);
			}else{ // Not dirty
				DEBUG('v',"\t\t\tvictim f=%d,l=%d limpia\n",IPT[indexSWAPFIFO]->physicalPage, IPT[indexSWAPFIFO]->virtualPage);
				int oldPhysicalPage = IPT[indexSWAPFIFO]->physicalPage;
				IPT[indexSWAPFIFO]->valid = false;
				IPT[indexSWAPFIFO]->physicalPage = -1;
				MiMapa->Clear( oldPhysicalPage );
				// ask for a free frame
				freeFrame = MiMapa->Find();
				if (freeFrame == -1){
					printf("Invalid free frame %d\n", freeFrame );
					ASSERT( false );
				}
				int oldSwapPageAddr = pageTable [vpn].physicalPage;
				pageTable [vpn].physicalPage = freeFrame;
				
				readFromSwap(freeFrame, oldSwapPageAddr);
				pageTable [vpn].valid = true;
				
				IPT[freeFrame] = &(pageTable [vpn]); // Update inverted TLB
				
				int tlbSPace = nextSecondChance(); // Update TLB
				useTLBIndex(tlbSPace, vpn);
			}
		}
}

void AddrSpace::load(unsigned int vpn){
	if (!pageTable[vpn].valid && !pageTable[vpn].dirty){ // if page is invalid and clean
		memPrincipal(vpn);
	}
	else if(!pageTable[vpn].valid && pageTable[vpn].dirty){ // if page is invalid and dirty
		swap(vpn);
	}
	else if(pageTable[vpn].valid && !pageTable[vpn].dirty){ // If page is valid and clean
		DEBUG('v', "- Page valid and clean\n");
		int tlbSPace = nextSecondChance();
		useTLBIndex(tlbSPace, vpn);
	}
	else{ //If page is valid and dirty
		DEBUG('v', "- Page valid and dirty\n");
		int tlbSPace = nextSecondChance();
		useTLBIndex(tlbSPace, vpn);
	}
}
