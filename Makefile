CC := g++
CXXFLAGS := -std=c++11 -Wall -g
LDFLAGS := -lpthread -lhiredis

HEADERS := $(wildcard *.h)
SRCS := $(wildcard *.cc)
OBJS := $(SRCS:%.cc=%.o)
 
redis_test: $(OBJS) $(HEADERS)
	$(CC) -o $@ $(CXXFLAGS) $(LDFLAGS) $^

%.o: %.cc $(HEADERS)
	$(CC) -o $@ -c $(CXXFLAGS) $<
 
.PHONY : clean
clean : 
	rm -f redis_client 
	rm -f *.o
