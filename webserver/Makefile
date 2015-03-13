
OBJS := webserver.o

all: webserver libfaux.so

# CC :=  gcc
CFLAGS := -DLINUX -DNDEBUG -I. -fPIC $(FAUX_OPT)

.SUFFIXES: .c .o

libfaux.so: libfaux.o
	gcc -shared -Wl,-soname,$@ -o $@  $^

webserver: $(OBJS)
	$(CC) -o $@ $^

run_webserver: webserver libfaux.so
	LD_PRELOAD=libfaux.so ./webserver
