/*
 * TcProfilerLinkedLibrary.cpp
 *
 *  Created on: Dec 16, 2013
 *      Author: raphael
 */

#define __STDC_FORMAT_MACROS

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct _loopStats {

	struct _loopStats *next;

	int64_t ID;
	int LoopClass;
	int numInstances;
	double predictionAccuracy;

} LoopStats;

LoopStats* createLoopStats(int64_t new_ID, int LoopClass){

	LoopStats* result = (LoopStats*)malloc(sizeof(LoopStats));

	result->ID = new_ID;
	result->LoopClass = LoopClass;
	result->numInstances = 0;
	result->predictionAccuracy = 1.0;
	result->next = NULL;

	return result;
}

void addInstance(LoopStats* Stats, int64_t tripCount, int64_t estimatedTripCount ){

	double instanceAccuracy;
	int64_t delta = tripCount - estimatedTripCount;

	if ( delta >= -1 && delta <= 1 ) {
		instanceAccuracy = 1.0;
	} else {
		instanceAccuracy = 0.0;
	}

	Stats->predictionAccuracy = (((double)Stats->numInstances * Stats->predictionAccuracy) + instanceAccuracy) / ((double)Stats->numInstances + 1.0);

	Stats->numInstances++;
}

typedef struct {

	LoopStats* First;

} LoopList;


void freeList(LoopList *L){

	LoopStats* currentNode = L->First;

	while (currentNode != NULL){
		LoopStats* nextNode = currentNode->next;
		free(currentNode);
		currentNode = nextNode;
	}

	L->First = NULL;
}

LoopStats* getOrInsertLoop(LoopList *L, int64_t ID, int LoopClass){

	//Get
	LoopStats* currentNode = L->First;
	LoopStats* lastNode = NULL;

	while (currentNode != NULL){
		if (currentNode->ID == ID) break;
		lastNode = currentNode;
		currentNode = currentNode->next;
	}

	if (currentNode != NULL) return currentNode;

	//Not found; insert
	LoopStats* newNode = createLoopStats(ID, LoopClass);
	if (lastNode != NULL) {
		lastNode->next = newNode;
	} else {
		L->First = newNode;
	}

	return newNode;
}

LoopList loops;

void initLoopList(){
	loops.First = NULL;
}

void collectLoopData(int64_t LoopHeaderBBPointer, int64_t tripCount, int64_t estimatedTripCount, int LoopClass){
	addInstance(getOrInsertLoop(&loops, LoopHeaderBBPointer, LoopClass), tripCount, estimatedTripCount);
}

void flushLoopStats(char* moduleIdentifier){

	FILE* outStream;
	outStream = fopen("loops.out", "a");

	if(!outStream){
		fprintf(stderr, "Error opening file loops.out");
	}else{

		LoopStats* currentNode = loops.First;

		while (currentNode != NULL){

			fprintf(outStream, "TripCount %d %s.%" PRId64 " %f\n",
						currentNode->LoopClass,
						moduleIdentifier,
						currentNode->ID,
						currentNode->predictionAccuracy );

			currentNode = currentNode->next;
		}

		freeList(&loops);

		fclose(outStream);
	}



}
