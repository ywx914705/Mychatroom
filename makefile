.PHONY:all
all:chatserver chatclient
chatclient:chatclient.cc
	g++ -o $@ $^ -std=c++11 -pthread
chatserver:chatserver.cc
	g++ -o $@ $^ -std=c++11 
.PHONY:clean
clean:
	rm -f chatserver chatclient