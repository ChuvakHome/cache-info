CXX=clang++
CXXFLAGS=-std=c++20 -Wall -Wextra -O2
EXECUTABLE=cache-info

all: $(EXECUTABLE)

cache-info: src/main.o
	$(CXX) -o $@ $^

src/main.o: src/main.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(EXECUTABLE) src/*.o
