CXX = g++

default: all

all: client

client: client.cpp
	$(CXX) -pthread -std=c++0x client.cpp -o client

clean:
	$(RM) client
