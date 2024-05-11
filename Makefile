CC = gcc

tmf:
	$(CC) -o tmf tmfuck.c auto.c regex.c stack.c ops.c

tmfuck:
	$(CC) -o tmfuck tmfuck.c auto.c regex.c stack.c ops.c
