default: all

CXXFLAGS += -O0 -g -DCONFIG_NETIF_DEBUG
CFLAGS += -O0 -g -DCONFIG_NETIF_DEBUG -lpthread

TESTS = $(patsubst %.c, %.exe, $(wildcard test/*.c)) udptest.exe udpstress.exe
LIBS = libs/libnetif.a

libnetif-HEADERS := include/net/netif.h 
libnetif-SRCS := netif.c
libnetif-OBJS := $(patsubst %.c, %.o, $(libnetif-SRCS))


all: libs/libnetif.a $(TESTS)

libs/libnetif.a: $(libnetif-OBJS)
	ar rs  $@ $^


$(TESTS): libs/libnetif.a

$(libnetif-OBJS): $(libnetif-HEADERS)

%.o: %.cpp
	$(CXX) $< -o $*.o -c $(CXXFLAGS) -I./include -I../netmap/include

%.o: %.c
	$(CC) $< -o $*.o -c $(CFLAGS) -I./include -I../netmap/include

%.exe: %.c
	$(CC) $< -o $*.exe $(CFLAGS) -I./include -I../netmap/include -L./libs -lnetif

.PHONY: test
test: $(TESTS)
	@sh -c "if [ ! -n \"$(VIF)\" ]; then echo \"You must set VIF=interface\"; false; fi"
	@RESULT=""; for i in $(TESTS); do echo sudo $$i $(VIF); sudo $$i $(VIF); ERR=$$?; if [ $$ERR -ne 0 ]; then echo "$$i \033[38;5;1mERROR $$ERR\033[39m"; RESULT=$$RESULT"$$i \033[38;5;1mERROR $$ERR\033[39m\n"; false; else echo "$$i \033[38;5;2mPASSED\033[39m"; RESULT=$$RESULT"$$i \033[38;5;2mPASSED\033[39m\n"; true; fi  done; echo "\n\nResults: \n$$RESULT"


.PHONY: clean
clean:
	-rm $(LIBS) $(TESTS) $(wildcard *.o)

