CC:=	g++
LD:=	g++

CXXFLAGS:=	-std=c++11 -Wall -g
LDFLAGS:=	-lpthread -lhiredis

HEADERS := $(wildcard *.h)
SRCS := $(wildcard *.cc)
OBJS := $(SRCS:%.cc=%.o)

TARGET= redis_test

.PHONY: all
all: $(TARGET)
$(TARGET): $(OBJS) $(HEADERS)
	$(LD) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

%.o: %.cc $(HEADERS)
	$(CC) -c $< -o $@ $(CXXFLAGS)
 
.PHONY : clean
clean : 
	rm -f $(TARGET) *.o 
