#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <inttypes.h>
#include <sys/time.h>
#include <unistd.h>
#include <malloc.h>

#include "nv_memory.h"
#include "random.h"
#include "nv_utils.h"
#include "link-cache.h"

/*
 *  Global variables
 */

int num_threads = 1;
int duration = 1000;
int seed = 0;
__thread unsigned long * seeds;
uint32_t rand_max;
void** data;

#define rand_min 1

static volatile int stop;


#ifdef DO_PROFILE
__thread uint64_t inserts;
__thread uint64_t removes;
#ifdef TSX_ENABLED
__thread uint64_t inserts_tsx;
#endif
#endif


/*
 *  Barrier
 */

typedef struct barrier 
{
  pthread_cond_t complete;
  pthread_mutex_t mutex;
  int count;
  int crossing;
} barrier_t;

void barrier_init(barrier_t *b, int n) 
{
  pthread_cond_init(&b->complete, NULL);
  pthread_mutex_init(&b->mutex, NULL);
  b->count = n;
  b->crossing = 0;
}

void barrier_cross(barrier_t *b) 
{
  pthread_mutex_lock(&b->mutex);
  /* One more thread through */
  b->crossing++;
  /* If not all here, wait */
  if (b->crossing < b->count) {
    pthread_cond_wait(&b->complete, &b->mutex);
  } else {
    pthread_cond_broadcast(&b->complete);
    /* Reset for next time */
    b->crossing = 0;
  }
  pthread_mutex_unlock(&b->mutex);
}
barrier_t barrier, barrier_global;

typedef struct thread_data
{
  uint8_t id;
  linkcache_t* lc;
  uint64_t inserted;
  uint64_t removed;
} thread_data_t;


void* test(void* thread) {
  thread_data_t* td = (thread_data_t*) thread;
  uint8_t ID = td->id;
  linkcache_t* lc = td->lc;
#ifdef DO_PROFILE
  inserts = 0;
  removes = 0;
#endif

  seeds = seed_rand();

  barrier_cross(&barrier_global);

  while (stop == 0) {
	  uint64_t next = (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % rand_max);
      void* old = data[next];
      void* new = (void*)((uintptr_t) (ID+1));
      cache_try_link_and_add(lc, next, (volatile void**) &(data[next]), old, new);
  }

  barrier_cross(&barrier);

#ifdef DO_PROFILE
  td->removed = removes;
  td->inserted = inserts;
#ifdef TSX_ENABLED
  fprintf(stderr, "Thread %d: %lu inserts, out of which %lu through HTM, %lu removes\n", ID, inserts, inserts_tsx, removes);
#else
  fprintf(stderr, "Thread %d: %lu inserts, %lu removes\n", ID, inserts, removes);
#endif
#endif
  barrier_cross(&barrier_global);
  pthread_exit(NULL);
}

/*
 * 
 * Link cahe interface:
 *  cache_create
 *  cache_destroy
 *  bucket_wb
 *  cache_scan
 *  cache_try_link_and_add
 *  cache_wb_all_buckets
 */

int main(int argc, char **argv) {

  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"duration",                  required_argument, NULL, 'd'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"range",                     required_argument, NULL, 'r'},
    {NULL, 0, NULL, 0}
  };

  size_t range = 2048;

  int i, c;
  while(1) 
    {
      i = 0;
      c = getopt_long(argc, argv, "hd:n:r:", long_options, &i);
		
      if(c == -1)
	break;
		
      if(c == 0 && long_options[i].flag == 0)
	c = long_options[i].val;
		
      switch(c) 
	{
	case 0:
	  /* Flag is automatically set */
	  break;
	case 'h':
	  printf("link_cache_test -- link cache correctness test \n"
		 "Usage:\n"
		 "  ./test_link_cache [options...]\n"
		 "\n"
		 "Options:\n"
		 "  -h, --help\n"
		 "        Print this message\n"
		 "  -d, --duration <int>\n"
		 "        Test duration in milliseconds\n"
		 "  -n, --num-threads <int>\n"
		 "        Number of threads\n"
		 "  -r, --range <int>\n"
		 "        Range of integer values inserted in set for testing\n"
		 );
	  exit(0);
	case 'd':
	  duration = atoi(optarg);
	  break;
	case 'n':
	  num_threads = atoi(optarg);
	  break;
	case 'r':
	  range = atol(optarg);
	  break;
	case '?':
	default:
	  printf("Use -h or --help for help\n");
	  exit(1);
	}
    }

  linkcache_t* lc = cache_create();


  printf("# threads: %d / range: %zu\n", num_threads, range);

  struct timeval start, end;
  struct timespec timeout;
  timeout.tv_sec = duration / 1000;
  timeout.tv_nsec = (duration % 1000) * 1000000;
    
  stop = 0;
  

  rand_max = range;
  data = (void**) calloc(range, sizeof(void*));



  pthread_t threads[num_threads];
  pthread_attr_t attr;
  int rc;
  void *status;
    
  barrier_init(&barrier_global, num_threads + 1);
  barrier_init(&barrier, num_threads);
    
  /* Initialize and set thread detached attribute */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
  thread_data_t* tds = (thread_data_t*) malloc(num_threads * sizeof(thread_data_t));

  long t;
  for(t = 0; t < num_threads; t++)
    {
      tds[t].id = t;
      tds[t].inserted = 0;
      tds[t].removed = 0;
      tds[t].lc = lc;
    }


  for(t = 0; t < num_threads; t++) {
      rc = pthread_create(&threads[t], &attr, test, tds + t);
      if (rc)
	{
	  printf("ERROR; return code from pthread_create() is %d\n", rc);
	  exit(-1);
	}
        
    }
    
  /* Free attribute and wait for the other threads */
  pthread_attr_destroy(&attr);
    
  barrier_cross(&barrier_global);
  gettimeofday(&start, NULL);
  nanosleep(&timeout, NULL);

  stop = 1;
  barrier_cross(&barrier_global);
  gettimeofday(&end, NULL);
  duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
    
  for(t = 0; t < num_threads; t++) 
    {
      rc = pthread_join(threads[t], &status);
      if (rc) 
	{
	  printf("ERROR; return code from pthread_join() is %d\n", rc);
	  exit(-1);
	}
    }

 
  uint64_t sum_inserted = 0;
  uint64_t sum_removed = 0;
  uint64_t current_size;

  current_size = 0;
#ifdef DO_PROFILE
  current_size = cache_size(lc);
#endif

 for (t=0; t< num_threads; t++) {
    sum_inserted+=tds[t].inserted;
    sum_removed+=tds[t].removed;
 }

 if ((sum_removed + current_size) != sum_inserted) {
    printf("Incorrect element count.\n");
    printf("Inserted: %lu\n", sum_inserted);
    printf("Removed: %lu\n", sum_removed);
    printf("Currently in the link cache: %lu\n", current_size);
 } else {
    printf("Correct element count.\n");
    printf("Total number of inserts: %lu\n", sum_inserted);
    printf("Total number of removes: %lu\n", sum_removed);
    printf("Elements currently in the cache: %lu\n", current_size);
 }

  free(tds);
  cache_destroy(lc);

  pthread_exit(NULL);
  return 0;
}
