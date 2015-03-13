PROGS := echo-client

all:	$(PROGS)

$(PROGS) : %: %.o
	$(CXX) $< -o $@ -lpthread

%.o: %.cpp
	$(CXX) $< -c -o $@ -std=c++0x -g -O2

clean:
	rm -f $(PROGS) *.o

