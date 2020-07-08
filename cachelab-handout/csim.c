#include "cachelab.h"
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>


typedef struct Cache {
	bool validbit;
	unsigned long tag;
	int timestamp;

	//int setIndex will be 2-d array 1st dimention
	//int blockOffset;
} cache_t;

int s;
int E; 
int b; 
bool v;

int misses;
int hits;
int evicts;

int timestamp;

char fileName[30];

cache_t** cacheSets;

unsigned long getSetIndex(unsigned long address) {
	unsigned long mask = 0xffffffff;
	unsigned long setIndex_mask = (mask >> (sizeof(mask)*4-b-s)) & (mask << b);
	/*printf("%lu\n", sizeof(mask)*4);
	printf("%lu\n", mask >> (sizeof(mask)*4-b-s));
	printf("%lu\n", mask << b);
	printf("%lu\n", setIndex_mask);*/
	unsigned long setIndex = (address & setIndex_mask) >> b;
	
	if(v) printf("setIndex for address %lu(%lx) is %lu(0x%lx)\n", address, address, setIndex, setIndex);

	return setIndex;
}

unsigned long getTag(unsigned long address) {
	unsigned long mask = 0xffffffff;
	unsigned long tag_mask = mask << (b+s);
	//printf("tag_mask is %lx\n", tag_mask);
	unsigned long tag = address & tag_mask;

	if (v) printf("tag for address %lu(%lx) is %lu(0x%lx)\n", address, address, tag, tag);

	return tag;
}

int isExist(cache_t* cacheSet, unsigned long address) {
	for (int i = 0; i < E; i++) {
		if (cacheSet[i].validbit && cacheSet[i].tag == getTag(address)) {
			return i;	
		}
	}
	return -1;
}

int findFreeCacheLine(cache_t* cacheSet) {
	for (int i = 0; i < E; i++) {
		if (!cacheSet[i].validbit)
			return i;
	}

	return -1;
}

int findOldestCacheLine(cache_t* cacheSet, unsigned long address) {
	int oldest_timestamp = timestamp;
	int line = 0;
	for (int i = 0; i < E; i++) {
		if ((cacheSet+i)->timestamp < oldest_timestamp) {
			oldest_timestamp = (cacheSet+i)->timestamp;
			line = i;
		}
	}
	return line;
}

void incrementTimestamp() {
	timestamp += 1;
}

cache_t constructCacheStruct(unsigned long address) {
	cache_t cache = {true, getTag(address), timestamp};
	return cache;
}

bool insert(cache_t* cacheSet, unsigned long address) {
	//address already exists in cache
	int i = isExist(cacheSet, address);
	if (i != -1) {
		hits += 1;
		cacheSet[i].timestamp = timestamp;
		if (v) printf("hit\n");
		return false;
	}
	
	i = findFreeCacheLine(cacheSet);
	// not exist and cache not full
	if (i != -1) {
		misses += 1;
		cacheSet[i] = constructCacheStruct(address);
		if (v) printf("miss\n");
		return true;
	}
	// not exit and cache has been full;
	i = findOldestCacheLine(cacheSet, address);
	cacheSet[i] = constructCacheStruct(address);
	misses += 1;
	evicts += 1;
	if (v) printf("miss, evict\n");
	return true;
}

bool initCache(int s, int E) {
	int setsNumber = 1 << s;
	cacheSets = (cache_t**) malloc (setsNumber * sizeof(cache_t*));
	if (cacheSets == NULL) 
		return false;

    for (int i = 0; i < setsNumber; i++) {
		cacheSets[i] = (cache_t*) malloc (E * sizeof(cache_t));
		if (cacheSets[i] == NULL)
			return false;
	}

	misses = 0;
	hits = 0;
	evicts = 0;

	return true;
}

void operate(char opt, unsigned long address) {
	unsigned long setIndex = getSetIndex(address);
	incrementTimestamp();

	switch (opt) {
	case 'L':
		insert(cacheSets[setIndex], address);
		break;
	case 'I':
		//insert(cacheSets[setIndex], address);
		break;
	case 'S':
		insert(cacheSets[setIndex], address);
		break;
	case 'M':
		insert(cacheSets[setIndex], address);
		insert(cacheSets[setIndex], address);
		break;
	}
}

void printCache() {
	int setNumber = 1 << s;
	for (int i = 0; i < setNumber; i++) {
		for (int j = 0; j < E; j++)	{
			printf("(set %d line %d) -> (validbit %d tag %lu, timestamp %d)\n", i, j, cacheSets[i][j].validbit, cacheSets[i][j].tag, cacheSets[i][j].timestamp);
		}
		printf("\n");
	}
}


int main(int argc, char **argv) {
	char ch;
	while ((ch = getopt(argc, argv, "b:s:E:t:v")) != -1) {
		switch(ch) {
		case 'b':
			b = atoi(optarg);
			break;
		case 's':
			s = atoi(optarg);
			break;
		case 'E':
			E = atoi(optarg);
			break;
		case 't':
			strncpy(fileName, optarg, 30);
			break;
		case 'v':
			v = true;
			break;
		default:
			printf("error");
			break;
		}
	}

	initCache(s, E);
	
	printf("\ns is %d, E is %d, b is %d, file is %s\n", s, E, b, fileName);

	FILE* filePointer;
	char buffer[30];
	filePointer = fopen(fileName, "r");
	char delim[] = " ,";
	while(fgets(buffer, 30, filePointer)) {
		if (v) printf("%s", buffer);
		char* opt = strtok(buffer, delim);
		char* address = strtok(NULL, delim);
		char* block = strtok(NULL, delim);
		if (v) printf("\nopt is %s, address is %s, block is %s\n", opt, address, block);
		
		operate(opt[0], strtoul(address, NULL, 16));
		if (v) printCache();
    	if (v) printSummary(hits, misses, evicts);
	}

    printSummary(hits, misses, evicts);

    return 0;
}



