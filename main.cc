#include <cstdint>
#include <iostream>
#include <thread>
#include <unistd.h>

using namespace std;


struct Product {
  char payload[16];
};

struct SharedState {
  Product* volatile product;
  uint64_t allocation_time;
  uint64_t deallocation_time;
  uint64_t throughput;
//  uint64_t //TODO metrics and probes
  //volatile int product;
} __attribute__((aligned(64)));


size_t kNumThreadPairs = 30;
const uint64_t kCpuMHz = 2000;
SharedState *shared_states;
int volatile terminator = 0;

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

void Producer(unsigned id) {
  SharedState* state = GetSharedState(id);
  uint64_t start;
  //cout << state << endl;
  while (1) {
    while (state->product != NULL && terminator == 0) {}
    if (terminator == 1) return;
    start = Rdtsc();
    state->product = static_cast<Product*>(malloc(sizeof(Product))); //new Product();
    state->allocation_time += Rdtsc() - start;
    //cout << "Produced " << state->product << endl;
    __sync_synchronize();
  }
}

void Consumer(unsigned id) {
  SharedState* state = GetSharedState(id);
  uint64_t start;
  //cout << state << endl;
  while (1) {
    while (state->product == NULL && terminator == 0) {}
    if (terminator == 1) return;
    start = Rdtsc();
    free(const_cast<Product*>(state->product));
    state->deallocation_time += Rdtsc() - start;
    state->throughput++;
    state->product = NULL;
    __sync_synchronize();
  }
}

int main(int argc, char **argv) {

  if (argc > 1) {
    kNumThreadPairs = atoi(argv[1]);
  }

  thread** producers;
  thread** consumers;
  producers = (thread**)calloc(kNumThreadPairs, sizeof(thread*));
  consumers = (thread**)calloc(kNumThreadPairs, sizeof(thread*));
  shared_states = (SharedState*)calloc(kNumThreadPairs, sizeof(SharedState));

  for (unsigned i = 0; i < kNumThreadPairs; ++i) {
    SharedState *state = &shared_states[i];
    state->product = NULL;
    state->allocation_time = 0;
    state->deallocation_time = 0;
    state->throughput = 0;
    
    __sync_synchronize();
    producers[i] = new thread(Producer, i);
    consumers[i] = new thread(Consumer, i+kNumThreadPairs);
  } 

  sleep(5);


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
    SharedState *state = &shared_states[i];
    alloc_total += state->allocation_time;
    dealloc_total += state->deallocation_time;
    throughput_total += state->throughput;
  }

  alloc_total /= (kCpuMHz * 1000);
  dealloc_total /= (kCpuMHz * 1000);

  printf("n\talloc\tfree\tthroughput\n");
  printf("%lu\t%lu\t%lu\t%lu\n",
         kNumThreadPairs, alloc_total, dealloc_total, throughput_total);


  return EXIT_SUCCESS;
}

