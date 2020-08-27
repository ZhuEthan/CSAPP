#include <stdio.h>

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_URL_SIZE 100
#define MAX_SLOTS_NUM 10

typedef struct {
	char buf[MAX_OBJECT_SIZE];
	char uri[MAX_URL_SIZE];
	size_t buffer_size;
//Should be real timestamp in prod system
	size_t timestamp;
} sbuf_t;

typedef struct {
	sbuf_t* buf[MAX_SLOTS_NUM];
	size_t rear;
	size_t t_size;
//Should be real timestamp in prod system
	size_t time;
} cache_t;

void init_cache();
void insert_cache(char* uri, size_t uri_length, char* object, size_t object_size);
int find_earliest_slot();
void check_cache();
int get_index_by_uri(char* uri, size_t uri_length);
sbuf_t* get_object_by_uri(char* uri, size_t uri_length);

