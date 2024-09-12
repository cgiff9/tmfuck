CC = gcc

CFLAGS = -lxxhash -lm -Wall -Wextra -O2
LOCALXXHASHFLAGS = -lm -Wall -Wextra -O2

DEBUGFLAGS = -lxxhash -lm -Wall -Wextra -g -pg
DEBUGLOCALXXHASHFLAGS = -lm -Wall -Wextra -g -pg

default:
	$(CC) -o tmf tmfuck.c auto.c block.c file.c machine.c regex.c stack.c var.c tape.c $(CFLAGS)

tmf:
	$(CC) -o tmf tmfuck.c auto.c block.c file.c machine.c regex.c stack.c var.c tape.c $(CFLAGS)

tmfuck:
	$(CC) -o tmfuck tmfuck.c auto.c block.c file.c machine.c regex.c stack.c var.c tape.c $(CFLAGS)


# To use the below option, the files xxhash.c and xxhash.h must be placed in the root tmf folder
# and the system '#include <xxhash.h>' statements must be replaced with relative '#include "xxhash.h" 
# statements in the following files:
#
# + tmfuck.c
# + block.c
# + auto.c
# + auto.h
# + var.c:
#
# Or, rather than do this manually, use the xxhash_conf.sh script to help yourself out:
# ./xxhash_conf.sh local
# ./xxhash_conf.sh download
# make localtmf
#
# If you wish to reset tmf source to system xxhash, run:
# ./xxhash_conf.sh system
#

localtmf:
	$(CC) -o tmf tmfuck.c auto.c block.c file.c machine.c regex.c stack.c var.c tape.c xxhash.c $(LOCALXXHASHFLAGS)

debug:
	$(CC) -o tmf tmfuck.c auto.c block.c file.c machine.c regex.c stack.c var.c tape.c $(DEBUGFLAGS)

localdebug:
	$(CC) -o tmf tmfuck.c auto.c block.c file.c machine.c regex.c stack.c var.c tape.c xxhash.c $(DEBUGLOCALXXHASHFLAGS)
