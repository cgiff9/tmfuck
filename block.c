#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <xxhash.h>

#include "auto.h"
#include "var.h"
#include "stack.h"
#include "tape.h"

#include "file.h"

// The following two functions were pulled
// from Howard Hinnant's answer on 
// StackExchange. Much thanks for the C++
// code and the incredibly detailed answer.
//
// For my purposes, 'Implementation #5' should
// be sufficient.
// url: https://stackoverflow.com/a/5694432
//
// The 'bucket size' of the hash table is
// the next prime after the hash threshold
// factor multiplied by the data array's
// maximum size
// > Macros for these hash values and 
//   functions can be found in block.h

// Determines if given number is prime
//
// Returns 0 for false, 1 for true
int is_prime(size_t x)
{
    size_t o = 4;
    for (size_t i = 5; 1; i += o)
    {
        size_t q = x / i;
        if (q < i)
            return 1;
        if (x == q * i)
            return 0;
        o ^= 6;
    }
    return 1;
}

// Finds the next prime following the
// given number
//
// Returns the next prime
size_t next_prime(size_t x)
{
    switch (x)
    {
    case 0:
    case 1:
    case 2:
        return 2;
    case 3:
        return 3;
    case 4:
    case 5:
        return 5;
    }
    size_t k = x / 6;
    size_t i = x - 6 * k;
    size_t o = i < 2 ? 1 : 5;
    x = 6 * k + o;
    for (i = (3 + o) / 2; !is_prime(x); x += i)
        i ^= 6;
    return x;
}

size_t Block_free(struct Block *block)
{	
	size_t elem = 0;

	struct Stack *datastacks = block->data.elem;
	for (unsigned int i = 0; i < block->data.size; i++) {
		struct Stack strip = datastacks[i];

		if (block->type == VAR) {
			struct Var *strip_elem = strip.elem;
			for (unsigned int j = 0; j < strip.size; j++) {
				elem += Var_free(&strip_elem[j]);
			}
		} else if (block->type == STACK) {
			struct Stack *strip_elem = strip.elem;
			for (unsigned int j = 0; j < strip.size; j++) {
				elem += Stack_free(&strip_elem[j]);
			}
		} else if (block->type == TAPE) {
			struct Tape *strip_elem = strip.elem;
			for (unsigned int j = 0; j < strip.size; j++) {
				elem += Tape_free(&strip_elem[j]);
			}
		}
		
		elem += Stack_free(&strip);
	}

	elem += Stack_free(&block->data);
	
	if (block->type == STATE ||
			block->type == TRANS ||
			block->type == VAR) {
		elem += Stack_free(&block->hash);
	}

	return elem;
}

struct Block Block_init(enum BlockType type)
{
	struct Block block;
	block.type = type;

	block.max = BLOCK_INITIAL_MAX;
	block.hashmax = next_prime(block.max * BLOCK_HASHFACTOR);	

	block.size = 0;

	block.data = Stack_init(STACK_STACK);
	struct Stack newstrip;

	switch(type) {
		case STATE:
			newstrip = Stack_init_max(STATE_STACK, block.max);
			block.hash = Stack_init_max(STATEPTR, block.hashmax);
			break;
		case TRANS:
			newstrip = Stack_init_max(TRANS_STACK, block.max);
			block.hash = Stack_init_max(TRANSPTR, block.hashmax);
			break;
		case VAR:
			newstrip = Stack_init_max(VAR_STACK, block.max);
			block.hash = Stack_init_max(VARPTR, block.hashmax);
			break;
		case NAME:
			newstrip = Stack_init_max(CHAR, block.max);
			break;
		case STACK:
			newstrip = Stack_init_max(STACK_STACK, block.max);
			break;
		case TAPE:
			newstrip = Stack_init_max(TAPE_STACK, block.max);
			break;
	}

	Stack_push(&block.data, &newstrip);
	
	return block;
}

void Block_grow(struct Block *block)
{
	// See block.h for more info on macros below
	if (block->type == NAME) {
		block->max BLOCK_NAME_GROWTH_FUNC;
	} else {
		block->max BLOCK_GROWTH_FUNC;
	}

	struct Stack *datastacks = block->data.elem;
	struct Stack newstack = Stack_init_max(datastacks[0].type, block->max - block->size);
	Stack_push(&block->data, &newstack);

	// Only these enums need be re-hashed
	if (block->type == STATE ||
			block->type == TRANS ||
			block->type == VAR) {

		// See block.h for more info
		block->hashmax BLOCK_HASH_RATIO_FUNC;

		Stack_free(&block->hash);
		block->hash = Stack_init_max(block->hash.type, block->hashmax);
	}
	
	datastacks = block->data.elem;
	switch(block->type) {
		case STATE:
		{
			struct State **hash_elem = block->hash.elem;
			for (unsigned int i = 0; i < block->data.size; i++) {

				struct Stack *strip = &datastacks[i];
				struct State *strip_elem = strip->elem;
				for (unsigned int j = 0; j < strip->size; j++) {
					
					struct State *unhashed = &strip_elem[j];
					XXH64_hash_t check = XXH3_64bits(unhashed->name, 
							strlen(unhashed->name)) % block->hashmax;
					XXH64_hash_t offset = check;

					struct State *statecheck = hash_elem[offset];
					while (statecheck != NULL) {
						if (++offset == block->hashmax) offset = 0;
						statecheck = hash_elem[offset];
					}
					hash_elem[offset] = unhashed;
				}
			}
		}
		break;
		// Transitions are hashed by parent state and input symbol
		case TRANS:
		{
			struct Trans **hash_elem = block->hash.elem;
			for (unsigned int i = 0; i < block->data.size; i++) {

				struct Stack *strip = &datastacks[i];
				struct Trans *strip_elem = strip->elem;
				for (unsigned int j = 0; j < strip->size; j++) {

					struct Trans *unhashed = &strip_elem[j];

					/*uintptr_t hashbuff[TRANS_HASHBUFF_LEN] = { 
						(uintptr_t) unhashed->pstate, (uintptr_t) unhashed->inputsym,
						~(uintptr_t) unhashed->pstate ^ (uintptr_t) unhashed->inputsym,
						(uintptr_t) unhashed->pstate ^ ~(uintptr_t) unhashed->inputsym,
						(uintptr_t) unhashed->pstate ^ (uintptr_t) unhashed->inputsym,
						//~(uintptr_t) unhashed->pstate, ~(uintptr_t) unhashed->inputsym,
						~(uintptr_t) unhashed->pstate ^ ~(uintptr_t) unhashed->inputsym,
						~(uintptr_t) unhashed->pstate, ~(uintptr_t) unhashed->inputsym,
					};*/
					
					TRANS_HASHBUFF_TYPE hashbuff[TRANS_HASHBUFF_LEN] = { 
						(TRANS_HASHBUFF_TYPE) unhashed->pstate, 
						(TRANS_HASHBUFF_TYPE) unhashed->inputsym,
					};

					XXH64_hash_t check = XXH3_64bits(hashbuff, TRANS_HASHBUFF_LEN) % block->hashmax;

					XXH64_hash_t offset = check;
					struct Trans *transcheck = hash_elem[offset];
					while (transcheck != NULL) {
						if (++offset == block->hashmax) offset = 0;
						transcheck = hash_elem[offset];
					}
					hash_elem[offset] = unhashed;
				}
			}
		}
		break;
		// Variables are hashed by name
		case VAR:
		{	
			struct Var **hash_elem = block->hash.elem;
			for (unsigned int i = 0; i < block->data.size; i++) {

				struct Stack *strip = &datastacks[i];
				struct Var *strip_elem = strip->elem;
				for (unsigned int j = 0; j < strip->size; j++) {

					struct Var *unhashed = &strip_elem[j];

					XXH64_hash_t check = XXH3_64bits(unhashed->name, 
							strlen(unhashed->name)) % block->hashmax;
					XXH64_hash_t offset = check;

					struct Var *varcheck = hash_elem[offset];
					while (varcheck != NULL) {
						if (++offset == block->hashmax) offset = 0;
						varcheck = hash_elem[offset];
					}
					hash_elem[offset] = unhashed;
				}
			}
		}
		break;
		// no hashing for these enums
		case NAME:
		break;
		case STACK:
		break;
		case TAPE:
		break;
	}

	return;
}
