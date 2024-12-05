#ifndef BLOCK_H_
#define BLOCK_H_

#include "stack.h"

//
// BLOCK GROWTH/HASHING MACROS:
//
// The macros below determine the performance
// of the hash tables used for storing/retrieving
// states, transitions, and variables.
//
// An Automaton struct consists of 3 crucial 'Blocks'
// that store the aforementioned objects. Each of
// these Blocks contains a 'data' array and a 'hash'
// array. The proportion of maxmimum elements in the 
// hash array to those in the data array determine 
// the hash table's load factor.
// 
// > block.data is a struct Stack that stores the actual
//   objects themselves. It is a semi-contiguous area of 
//   memory, implemented as a Stack of progressively 
//   growing internal Stacks.
// > block.hash is a struct Stack that stores hash 
//   buckets (struct pointers) to the objects in block.data
//
// This (hopefully) allows the hash table to be sufficiently
// larger than the raw struct data, so that empty buckets
// in the hash array merely contain NULL pointers (8 bytes)
// rather than unused struct objects (ie. struct Trans being 
// 24 bytes with CELL_TYPE char and 56 bytes with CELL_TYPE long
// on my machine).
//
// The hash load factor is critical to the performance 
// of fetching transitions during machine simulation. The 
// load factor is determined by the following procedure:
// 
//   1. Increase block.data.max: Apply the block growth function 
//      to the previous maximum data size. Default function 
//      is to multiply previous max by the Golden Ratio.
//
//   2. Increase block.hash.max (number of hash buckets):
//      Multiply the new data.max by the block's hash-factor.
//      Then, find the next prime larger than that result.
//      That prime number becomes the maximum number of
//      buckets in the hash table.
//
// The Name Block growth function is defined separately
// because it's safest to always let it grow such that its 
// next 'strip' of memory is larger than the previous one; 
// Name_add() in auto.c will greedily grow the Name Block 
// (add a new strip of chars) until a relatively long 
// state/variable name can fit within the strip's available 
// character allocation.
//
// Note: The block.data.max and block.hashmax (block.hash.max) 
// fields are integer types, so specifying a float as the 
// multiplier (as I've done here) will not result in a consistent 
// hash ratio across several block resizes.
//
// > Even if the multiplier is an integer,
//   the next_prime() function will most likely result
//   in a hashmax that produces an 'imperfect' ratio that
//   slightly differs from the ratio intended here.
//
// Default macro values:
//    initial size:         10
//    multiplier:           1.6188033988749
//    hash factor:          3   (10/31 ~= 33% full before resize/rehash)
//    growth function:      old data.max * multiplier
//    hash ratio function:  next prime larger than (new data.max * hash factor)


   #define BLOCK_INITIAL_MAX  10
   #define BLOCK_MULTIPLIER   1.618033988749
   #define BLOCK_HASHFACTOR   3

   #define BLOCK_GROWTH_FUNC *= BLOCK_MULTIPLIER
   #define BLOCK_NAME_GROWTH_FUNC *= BLOCK_MULTIPLIER

	#define BLOCK_HASH_RATIO_FUNC = next_prime(block->max * BLOCK_HASHFACTOR);

/*===========================================*/
// 
// Transition fetching hash macros:
//
// HASHBUFF_LEN:
// Determines size of hash buffer for
// Trans_add() and Trans_get() functions.
//
// HASHBUFF_TYPE:
// Determines datatype of elements in the
// hash buffer. After being cast to this type,
// the pointer to the parent state of the 
// transition and the input symbol of the 
// transition form the contents of this buffer,
// which is passed to xxHash.


   #define TRANS_HASHBUFF_LEN 2
	#define TRANS_HASHBUFF_TYPE uintptr_t

/*===========================================*/

extern const size_t trans_hashbuff_bytes;

enum BlockType {
	STATE,
	TRANS,
	VAR,
	NAME,
	STACK,
	TAPE
};

struct Block {
	struct Stack data;
	struct Stack hash;
	enum BlockType type;
	unsigned int size;
	unsigned int max;
	unsigned int hashmax;
};

size_t Block_free(struct Block *block);
struct Block Block_init(enum BlockType type);
void Block_grow(struct Block *block);

#endif // BLOCK_H_
