#include "cache.h"
#include "csapp.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

cache_t cache;

void init_cache() {
	cache.rear = 0;
	cache.t_size = 0;
	cache.time = 0;
}

void insert_cache(char* uri, size_t uri_length, char* object, size_t object_size) {
	if (object_size > MAX_OBJECT_SIZE) {
		printf("WARNING: object size exceeded the largest threshold for cache buf\n");
		return;
	} 

	size_t index = get_index_by_uri(uri, uri_length);
	if (cache.rear >= MAX_SLOTS_NUM && index == -1) {
//cache is full and key is not stored yet, evict least recently used slot
		index = find_earliest_slot(uri, uri_length, object, object_size);
		cache.t_size -= (cache.buf[index]->buffer_size);
	} else if (index != -1) {
//key was stored already
		cache.t_size -= (cache.buf[index]->buffer_size);
	} else {
//key is not found and cache is not full
		index = cache.rear;
		cache.rear += 1;
		cache.buf[index] = (sbuf_t*)Malloc(sizeof(sbuf_t));
	}
	
	memcpy(cache.buf[index]->buf, object, object_size);
	strncpy(cache.buf[index]->uri, uri, MIN(MAX_URL_SIZE, uri_length));
	cache.buf[index]->buffer_size = object_size;
	cache.buf[index]->timestamp = cache.time;
	cache.t_size += object_size;
}

int find_earliest_slot() {
	size_t earliest_ts = cache.time;
	int index = 0;
	for (size_t i = 0; i < cache.rear; i++) {
		if (cache.buf[i]->timestamp < earliest_ts) {
			earliest_ts = cache.buf[i]->timestamp;
			index = i;
		}
	}
	return index;
}

int get_index_by_uri(char* uri, size_t uri_length) {
	cache.time += 1;
	for (size_t i = 0; i < cache.rear; i++) {
		if (!strncmp(cache.buf[i]->uri, uri, uri_length)) {
			cache.buf[i]->timestamp = cache.time;
			return i;
		}
	}
	return -1;
}

sbuf_t* get_object_by_uri(char* uri, size_t uri_length) {
	int index = get_index_by_uri(uri, uri_length);
	if (index == -1) {
		return NULL;
	}
	return cache.buf[index];
}


void check_cache() {
	size_t n = 0; 
	for (size_t i = 0; i < cache.rear; i++) {
		printf("key: %s, content: %s, ts: %zu\n, buffer_size: %zu", cache.buf[i]->uri, cache.buf[i]->buf, cache.buf[i]->timestamp, cache.buf[i]->buffer_size);
		n += cache.buf[i]->buffer_size;
	}
	printf("\n");
	if (n != cache.t_size) {
		fprintf(stderr, "cache size is not matched for (%zu vs %zu)\n", n, cache.t_size);
		exit(0);
	}
	printf("cache rear is %zu\n", cache.rear);
	printf("total size %zu\n", cache.t_size);
}

/*
int main(int argc, char** argv) {
	printf("testing cache\n");
	init_cache();
	char uri1[MAX_URL_SIZE] = "http://www.google.com";
	char obj1[MAX_OBJECT_SIZE] = "object1";
	char uri2[MAX_URL_SIZE] = "http://www.google.com.";
	char obj2[MAX_OBJECT_SIZE] = "object2";
	insert_cache(uri1, strlen(uri1), obj1, strlen(uri1));
	insert_cache(uri2, strlen(uri2), obj2, strlen(obj2));
	check_cache();

	printf("checkpoint 1 %s\n", get_object_by_uri(uri1, strlen(uri1))->uri);
	check_cache();

	insert_cache(uri2, strlen(uri2), obj2, strlen(obj2));
	check_cache();

	char uri3[MAX_URL_SIZE] = "http://www.baidu.com";
	char obj3[MAX_OBJECT_SIZE] = "object3";
	sbuf_t* test1 = get_object_by_uri(uri3, strlen(uri3));
	if (test1 == NULL) {
		printf("null is returned\n");
	} else {
		printf("checkpoint 2 %s\n", test1->uri);
	}
	insert_cache(uri3, strlen(uri3), obj3, strlen(obj3));
	check_cache();

	printf("checkpoint 3 %s\n", get_object_by_uri(uri1, strlen(uri1))->uri);
	check_cache();
}*/
