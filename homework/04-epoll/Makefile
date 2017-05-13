all: main.o Listener.o
	$(CXX) main.o Listener.o -o 04-epoll

main.o: main.cpp
	$(CXX) -c $(CXXFLAGS) main.cpp

Listener.o: Listener.cpp
	$(CXX) -c $(CXXFLAGS) Listener.cpp

clean:
	find . -type f -name '*.o' -exec rm {} +
