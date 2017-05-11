all: main.o
	$(CXX) main.o -o 03-sort

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) main.cpp

clean:
	find . -type f -name '*.o' -exec rm {} +