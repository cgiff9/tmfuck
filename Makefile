CC = gcc
#CC = clang

CFLAGS = -lm -Wall -Wextra -O2
DEBUGFLAGS = -lm -Wall -Wextra -g -pg

SOURCES = $(wildcard src/*.c)

ifeq (,$(wildcard ./src/xxhash.c))
CFLAGS += -lxxhash
DEBUGFLAGS += -lxxhash
else
ifeq (,$(wildcard ./src/xxhash.h))
SOURCES = $(subst src/xxhash.c,,$(wildcard src/*.c))
CFLAGS += -lxxhash
DEBUGFLAGS += -lxxhash
endif
endif


default:
	$(CC) -o tmf $(SOURCES) $(CFLAGS)

tmf:
	$(CC) -o tmf $(SOURCES) $(CFLAGS)

tmfuck:
	$(CC) -o tmfuck $(SOURCES) $(CFLAGS)

debug:
	$(CC) -o tmf $(SOURCES) $(DEBUGFLAGS)
