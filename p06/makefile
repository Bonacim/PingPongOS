all: build run clean

build: p6-conta p6-conta-prio
 
p6-conta: pingpong-contab.o p06-pingpong.o queue.o
	gcc -o p6-conta pingpong-contab.o p06-pingpong.o queue.o -Wall -std=c17

p6-conta-prio: pingpong-contab-prio.o p06-pingpong.o queue.o
	gcc -o p6-conta-prio pingpong-contab-prio.o p06-pingpong.o queue.o -Wall -std=c17

pingpong-contab.o: pingpong-contab.c pingpong.h p06-datatypes.h
	gcc -o pingpong-contab.o pingpong-contab.c -c -Wall -std=c17

pingpong-contab-prio.o: pingpong-contab-prio.c pingpong.h p06-datatypes.h
	gcc -o pingpong-contab-prio.o pingpong-contab-prio.c -c -Wall -std=c17

p06-pingpong.o: p06-pingpong.c pingpong.h queue.h p06-datatypes.h
	gcc -o p06-pingpong.o p06-pingpong.c -c -Wall -std=c17
    
queue.o: queue.c queue.h
	gcc -o queue.o queue.c -c -Wall -std=c17

run: 
	./p6-conta
	@echo ""
	./p6-conta-prio
	@echo ""

clean:
	rm -rf *.o *~ p6-conta p6-conta-prio
