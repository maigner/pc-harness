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
//  uint64_t //TODO metrics and probes
  //volatile int product;
} __attribute__((aligned(64)));


const size_t kNumThreadPairs = 30;
SharedState shared_states[kNumThreadPairs];

inline SharedState* GetSharedState(unsigned id) {
  unsigned index = id % kNumThreadPairs;
  //cout << "returning index " << index << " for id " << id << endl;
  return &(shared_states[index]);
}

void Producer(unsigned id) {
  SharedState* state = GetSharedState(id);
  //cout << state << endl;
  while (1) {
    while (state->product != NULL) {}

    state->product = static_cast<Product*>(malloc(sizeof(Product))); //new Product();
    //cout << "Produced " << state->product << endl;
    __sync_synchronize();
  }
}

void Consumer(unsigned id) {
  SharedState* state = GetSharedState(id);
  //cout << state << endl;
  while (1) {
    while (state->product == NULL) {}
    
    //cout << "Consumed " << state->product << endl;
    free(const_cast<Product*>(state->product));
    state->product = NULL;
    __sync_synchronize();
  }
}

int main(int argc, char **argv) {

  thread* producers[kNumThreadPairs];
  thread* consumers[kNumThreadPairs];

  for (unsigned i = 0; i < kNumThreadPairs; ++i) {
    shared_states[i].product = NULL;
    __sync_synchronize();
    producers[i] = new thread(Producer, i);
    consumers[i] = new thread(Consumer, i+kNumThreadPairs);
  }

  sleep(15);
  return EXIT_SUCCESS;
}

