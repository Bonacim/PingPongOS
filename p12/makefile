all: build run clean

build: p12-fila
 
p12-fila: pingpong-mqueue.o p12-pingpong.o queue.o
	gcc -g -o p12-fila pingpong-mqueue.o p12-pingpong.o queue.o -Wall -lm
 
pingpong-mqueue.o: pingpong-mqueue.c pingpong.h p12-datatypes.h
	gcc -g -o pingpong-mqueue.o pingpong-mqueue.c -c -Wall -lm
 
p12-pingpong.o: p12-pingpong.c pingpong.h queue.h p12-datatypes.h
	gcc -g -o p12-pingpong.o p12-pingpong.c -c -Wall 
    
queue.o: queue.c queue.h
	gcc -g -o queue.o queue.c -c -Wall 

run: 
	./p12-fila
	@echo ""
	
clean:
	rm -rf *.o *~ p12-fila
