CC = gcc
CFLAGS = -Wall -Wextra

.PHONY: all clean

all: parallel_min_max process_memory parallel_sum


parallel_min_max: ../../lab3/src/parallel_min_max.c ../../lab3/src/find_min_max.c ../../lab3/src/utils.c
	$(CC) $(CFLAGS) \
	../../lab3/src/parallel_min_max.c \
	../../lab3/src/find_min_max.c \
	../../lab3/src/utils.c \
	-o parallel_min_max


process_memory: process_memory.c
	$(CC) $(CFLAGS) \
	process_memory.c \
	-o process_memory


parallel_sum: parallel_sum.c sum.c ../../lab3/src/utils.c
	$(CC) $(CFLAGS) \
	parallel_sum.c \
	sum.c \
	../../lab3/src/utils.c \
	-I../../lab3/src \
	-pthread \
	-o parallel_sum


clean:
	rm -f parallel_min_max process_memory parallel_sum
