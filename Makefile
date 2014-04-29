
harness: main.cc
	g++ -c -O0 -std=c++11 -pthread main.cc
	g++ -std=c++11 -o pc-harness main.o -pthread -lscalloc-static -L../acdc/allocators 

clean:
	rm pc-harness
