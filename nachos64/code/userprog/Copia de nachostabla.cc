#include "nachostabla.h"

NachosOpenFilesTable::NachosOpenFilesTable(){
	this->usage = 0;
	this->openFilesMap = new BitMap(32);
	this->openFiles = new int [32];
}       // Initialize 

NachosOpenFilesTable::~NachosOpenFilesTable(){
	delete openFilesMap;
	delete openFiles;
}      // De-allocate

int NachosOpenFilesTable::Open( int UnixHandle ){
	bool alreadyOpened = false;
	int nachosHandle = 0;

	for (int i = 0; i < 32; ++i)
	{
		if(openFiles[i] == UnixHandle){
			if(isOpened(i)){
				nachosHandle = i;
				alreadyOpened = true;
				addThread();
				break;
			}
		}
	}

	if(!alreadyOpened){
		nachosHandle = openFilesMap->Find();
		if(nachosHandle == -1){
			printf( "Maximum of files opened.");
		}else{
			openFiles[nachosHandle] = UnixHandle;
		}
	}
	return nachosHandle;
} // Register the file handle

int NachosOpenFilesTable::Close( int NachosHandle ){
	if(usage != 1){
		delThread();
	}else{
		for (int i = 0; i < 32; ++i)
		{
			if(openFilesMap->Test(i)){
				openFilesMap->Clear(i);
			}
		}
	}
	return 0;
}      // Unregister the file handle

bool NachosOpenFilesTable::isOpened( int NachosHandle ){
	return openFilesMap->Test(NachosHandle);
}

int NachosOpenFilesTable::getUnixHandle( int NachosHandle ){
	if(isOpened(openFiles[NachosHandle])){
		return openFiles[NachosHandle];
	}else{
		return -1;
	};
}

void NachosOpenFilesTable::addThread(){
	usage++;
}	// If a user thread is using this table, add it

void NachosOpenFilesTable::delThread(){
	usage--;
}		// If a user thread is using this table, delete it

void NachosOpenFilesTable::Print(){
	openFilesMap->Print();
}              // Print contents