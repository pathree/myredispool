CC:=	g++
LD:=	g++

CXXFLAGS:=	-std=c++11 -Wall -g -fPIC
LDFLAGS:=	-lpthread -lhiredis

HEADERS := $(wildcard *.h)
SRCS := $(wildcard *.cc)
OBJS := $(SRCS:%.cc=%.o)

TARGET= libmy_redis_pool.so redis_test

.PHONY: all
all: $(TARGET)
libmy_redis_pool.so: $(OBJS)
	$(LD) -shared -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

redis_test: $(OBJS) 
	$(LD) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

%.o: %.cc $(HEADERS)
	$(CC) -c $< -o $@ $(CXXFLAGS)

.PHONY: install
install:
	install -d $(DESTDIR)/usr/local/bin $(DESTDIR)/usr/local/lib
	install -m 0755 redis_test $(DESTDIR)/usr/local/bin/
	install -m 0644 libmy_redis_pool.so $(DESTDIR)/usr/local/lib
 
.PHONY : clean
clean : 
	rm -f $(TARGET) *.o 
