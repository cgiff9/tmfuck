CC = gcc

tmf:
	$(CC) -o tmf tmfuck.c auto.c regex.c stack.c ops.c
