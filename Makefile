
harness: main.cc
	g++ -O3 -std=c++11 -pthread -o pc-harness main.cc

clean:
	rm pc-harness
