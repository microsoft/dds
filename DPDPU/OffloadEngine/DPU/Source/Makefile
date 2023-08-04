CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I../Include
LIBS = -libverbs -lrdmacm -lpthread

all: ibv_sample

ibv_sample: ibv_sample.c ../Include/latency_helpers.h ../Include/affinity.h
		$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
		rm -f ibv_sample
