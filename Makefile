CC = gcc
CFLAGS = -Wall

all: oss worker

oss: oss.c
	$(CC) $(CFLAGS) -o oss oss.c -lrt

worker: worker.c
	$(CC) $(CFLAGS) -o worker worker.c -lrt

clean:
	rm -f oss worker