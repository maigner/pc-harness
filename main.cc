#include <cstdint>
#include <iostream>
#include <thread>
#include <unistd.h>

using namespace std;

// magic procided by scalloc
extern "C" size_t malloc_core_id();

struct Product {
  char payload[16];
};

struct SharedState {
  Product* volatile product;
} __attribute__((aligned(128)));

struct Stat {
  uint64_t allocation_time;
  uint64_t deallocation_time;
  uint64_t throughput;
} __attribute__((aligned(128)));

size_t kNumThreadPairs = 30;
const uint64_t kCpuMHz = 2000;
SharedState *shared_states;
Stat *stats;
int volatile terminator __attribute((aligned(128)));

inline uint64_t Rdtsc() {
  unsigned int hi, lo;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t) lo) | (((uint64_t) hi) << 32);
}

inline SharedState* GetSharedState(unsigned id) {
  unsigned index = id % kNumThreadPairs;
  //cout << "returning index " << index << " for id " << id << endl;
  return &(shared_states[index]);
}


double calculate_pi(int n) {
  int f = 1 - n;
  int ddF_x = 1;
  int ddF_y = -2 * n;
  int x = 0;
  int y = n;
  int64_t in = 0;
  while (x < y) {
    if (f >= 0) {
      --y;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    in += y - x;
  }
  return 8.0 * in / (static_cast<double>(n) * n);
}

void Dummy(unsigned id) {
  void *ptr = malloc(16);
  size_t core_id = malloc_core_id();
  //printf("dummy assigned to %lu\n", core_id);
}

void Producer(unsigned id) {
  void *ptr = malloc(16);
  size_t core_id = malloc_core_id();
  printf("producer assigned to %lu\n", core_id);
  SharedState* state = &shared_states[core_id];
  Stat* stat = &stats[id];
  uint64_t start;
  double pi;
  //cout << state << endl;
  while (1) {
    while (state->product != NULL && terminator == 0) {}
    if (terminator == 1) break;
    //pi = calculate_pi(100);
    start = Rdtsc();
    state->product = static_cast<Product*>(malloc(sizeof(Product))); //new Product();

    //printf("%lu: %p\n", id, state->product);

    //state->product = reinterpret_cast<Product*>(1); //new Product();
    stat->allocation_time += Rdtsc() - start;
    //cout << "Produced " << state->product << endl;
    __sync_synchronize();
  }
  printf("producer finally at %lu\n", malloc_core_id());
}

void Consumer(unsigned id) {
  void *ptr = malloc(16);
  size_t core_id = malloc_core_id();
  printf("consumer assigned to %lu\n", core_id);
  //SharedState* state = &shared_states[(core_id + 1) % kNumThreadPairs];
  SharedState* state = &shared_states[core_id - kNumThreadPairs];
  //SharedState* state = &shared_states[core_id];
  Stat* stat = &stats[id];
  uint64_t start;
  double pi;
  //cout << state << endl;
  while (1) {
    while (state->product == NULL && terminator == 0) {}
    if (terminator == 1) break;
    //pi = calculate_pi(100);
    start = Rdtsc();
    free(const_cast<Product*>(state->product));
    stat->deallocation_time += Rdtsc() - start;
    stat->throughput++;
    state->product = NULL;
    __sync_synchronize();
  }
  printf("consumer finally at %lu\n", malloc_core_id());
}

int main(int argc, char **argv) {

  if (argc > 1) {
    kNumThreadPairs = atoi(argv[1]);
  }
  void *ptr = malloc(16);
  terminator = 0;
  __sync_synchronize();

  thread** producers;
  thread** consumers;
  producers = (thread**)calloc(kNumThreadPairs, sizeof(thread*));
  consumers = (thread**)calloc(kNumThreadPairs, sizeof(thread*));

  size_t kNumCoreBuffers = 80;

  int r = posix_memalign((void**)&shared_states, 128, 
      kNumCoreBuffers * sizeof(SharedState));
  r = posix_memalign((void**)&stats, 128, 
      2 * kNumThreadPairs * sizeof(Stat));

  for (unsigned i = 0; i < kNumThreadPairs; ++i) {
    SharedState *state = &shared_states[i];
    Stat *producer_stat = &stats[i];
    Stat *consumer_stat = &stats[i+kNumThreadPairs];

    //printf("%p\t%p\t%p\n", state, producer_stat, consumer_stat);
    state->product = NULL;
    producer_stat->allocation_time = 0;
    consumer_stat->deallocation_time = 0;
    consumer_stat->throughput = 0;
  } 
    
  __sync_synchronize();
  
  for (unsigned i = 0; i < kNumThreadPairs; ++i) {
    producers[i] = new thread(Producer, i);
    //consumers[i] = new thread(Consumer, i + kNumThreadPairs);
  }

  sleep(2); // hack to allow assignment of allocator
  
  /*
  for (unsigned i = 0; i < kNumThreadPairs+2; ++i) {
    thread *d = new thread(Dummy, i);
    //producers[i] = new thread(Producer, i);
    //consumers[i] = new thread(Consumer, i + kNumThreadPairs);
  }

  sleep(2); // hack to allow assignment of allocator
  */
  
  for (unsigned i = 0; i < kNumThreadPairs; ++i) {
    //producers[i] = new thread(Producer, i);
    consumers[i] = new thread(Consumer, i + kNumThreadPairs);
  }


  sleep(20);

  terminator = 1;
  __sync_synchronize();


  for (unsigned i = 0; i < kNumThreadPairs; ++i) {
    producers[i]->join();
    consumers[i]->join();
  } 

  uint64_t alloc_total = 0;
  uint64_t dealloc_total = 0;
  uint64_t throughput_total = 0;

  for (unsigned i = 0; i < kNumThreadPairs; ++i) {
    Stat *producer_stat = &stats[i];
    Stat *consumer_stat = &stats[i+kNumThreadPairs];
    alloc_total += producer_stat->allocation_time;
    dealloc_total += consumer_stat->deallocation_time;
    throughput_total += consumer_stat->throughput;
  }

  alloc_total /= (kCpuMHz * 1000);
  dealloc_total /= (kCpuMHz * 1000);

  printf("n\talloc\tfree\tthroughput\n");
  printf("%lu\t%lu\t%lu\t%lu\n",
         kNumThreadPairs, alloc_total, dealloc_total, throughput_total);


  return EXIT_SUCCESS;
}

