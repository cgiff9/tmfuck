#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <xxhash.h>

#include "auto.h"
#include "block.h"
#include "stack.h"
#include "var.h"

#include "file.h"

#include <limits.h>

// Frees allocated memory of
// all structures within Automaton
size_t Automaton_free(struct Automaton *a0)
{
	return Block_free(&a0->states)
		+ Block_free(&a0->trans)
		+ Block_free(&a0->vars)
		+ Block_free(&a0->names)
		+ Block_free(&a0->stacks)
		+ Block_free(&a0->tapes)

		+ Stack_free(&a0->finals)
		+ Stack_free(&a0->rejects)
		+ Stack_free(&a0->alphabet)
		+ Stack_free(&a0->delims);
}

// Creates new automaton
// > init storage blocks for states, transitions, and variables
// > init reference stacks for final and reject states
// > init reference stacks for transition symbol symgroups and vargroups
//
// Returns automaton
struct Automaton Automaton_init()
{
	struct Automaton a0;
	a0.states = Block_init(STATE);
	a0.trans = Block_init(TRANS);
	a0.vars = Block_init(VAR);
	a0.names = Block_init(NAME);
	a0.stacks = Block_init(STACK);
	a0.tapes = Block_init(TAPE);

	a0.start = NULL;
	a0.finals = Stack_init(STATEPTR);
	a0.rejects = Stack_init(STATEPTR);

	a0.alphabet = Stack_init(CELL);
	a0.delims = Stack_init(CELL);

	a0.tm = 0;
	a0.pda = 0;
	a0.regex = 0;
	a0.epsilon = 0;
	a0.strans = 0;
	a0.epsilon_loops = 0;

	return a0;
}

// Adds name to Name Block in Automaton
// > allows 'dynamic' string length while
//   minimizing malloc calls (and avoiding realloc)
// > Name Block contains array (or 'strips') of
//   multiple strings, each separated by '\0'
// > Does not check for duplicates
// > Stores names for Var and State structs
//   (see block.c for more details)
//
// Return pointer to newly added name string
char *Name_add(struct Automaton *a0, char *name, int state)
{
	struct Block *block = &a0->names;
	
	ptrdiff_t namelen = strlen(name) + 1; // plus null term
	if (state && namelen > longest_name) longest_name = namelen;

	//struct Stack *datastacks = block->data.elem;
	//struct Stack *strip = &datastacks[block->data.size-1];

	struct Stack *strip = Stack_get(&block->data, block->data.size-1);

	// If no more room for name in this strip, add new strip
	while (strip->max - strip->size < namelen) {

		block->size = block->max;
		strip->size = strip->max;

		Block_grow(block);
		
		//datastacks = block->data.elem;
		//strip = &datastacks[block->data.size-1];
		strip = Stack_get(&block->data, block->data.size-1);
	}

	char *namechars = strip->elem;
	char *newname = &namechars[strip->size];

	strcat(newname, name);
	
	strip->size += namelen;
	block->size += namelen;
	return newname;
}


// Adds state to automaton
// > assigns new state next open index in automaton's states block
// > hashes state by its name and adds to **hash table in block
// > grows automaton's state block if necessary
//
// Returns pointer to new/existing state
struct State *State_add(struct Automaton *a0, char *name)
{
	struct Block *block = &a0->states;
	struct State **hash_elem = block->hash.elem;

	XXH64_hash_t check = XXH3_64bits(name, strlen(name)) % block->hashmax;
	XXH64_hash_t offset = check;
	
	struct State *statecheck = hash_elem[offset];

	// open-addressing hash table in states block
	// > very basic collision mitigation (checks next neighbor til empty)
	while (statecheck != NULL) {
		if (!strcmp(statecheck->name, name)) return statecheck;
		if (++offset == block->hashmax) offset = 0;
		statecheck = hash_elem[offset];
	}

	if (block->size >= block->max) {
		Block_grow(block);
		hash_elem = block->hash.elem;

		// Rehash new state's name after block **hash resize
		check = XXH3_64bits(name, strlen(name)) % block->hashmax;
		offset = check;

		statecheck = hash_elem[offset];
		while (statecheck != NULL) {
			if (++offset == block->hashmax) offset = 0;
			statecheck = hash_elem[offset];
		}
	}

	/*struct Stack *datastacks = block->data.elem;
	struct Stack *strip = &datastacks[block->data.size-1];
	struct State *states = strip->elem;	
	struct State *newstate = &states[strip->size++];*/

	struct Stack *strip = Stack_get(&block->data, block->data.size-1);
	struct State *newstate = Stack_get(strip, strip->size++);

	hash_elem[offset] = newstate;
	
	newstate->start = 0;
	newstate->final = 0;
	newstate->reject = 0;
	newstate->epsilon = 0;
	newstate->syms = 0;
	newstate->epsilon_mark = 0;
	newstate->exec = 0;

	newstate->name = Name_add(a0, name, 1);

	block->size++;

	return newstate;
}

// Takes parent state, symbol and return matching transition
// > if ntrans > 0, that indicates a previous run of Trans_get
//   found a transition with a sibling (shared pstate & sym) and
//   must hunt further down the block to find the sibling on this try
//
// Returns transition
struct Trans *Trans_get(struct Automaton *a0, struct State *pstate, CELL_TYPE inputsym, XXH64_hash_t *offset_prev, int epsilon)
{
	struct Block *block = &a0->trans;
	struct Trans **hash_elem = block->hash.elem;

	// If previous offset (previous sibling trans), start
	// searching from that hash index
	XXH64_hash_t offset = 0;
	if (*offset_prev == block->hashmax) {
		
		// Pre-mixing voodoo... it just made retrieval faster
		/*uintptr_t hashbuff[TRANS_HASHBUFF_LEN] = { 
			(uintptr_t) pstate, (uintptr_t) inputsym,
			~(uintptr_t) pstate ^ (uintptr_t) inputsym,
			(uintptr_t) pstate ^ ~(uintptr_t) inputsym,
			(uintptr_t) pstate ^ (uintptr_t) inputsym,
			~(uintptr_t) pstate ^ ~(uintptr_t) inputsym,
			~(uintptr_t) pstate, ~(uintptr_t) inputsym,
		};*/
		
		TRANS_HASHBUFF_TYPE hashbuff[TRANS_HASHBUFF_LEN] = { 
			(TRANS_HASHBUFF_TYPE) pstate, 
			(TRANS_HASHBUFF_TYPE) inputsym,
		};

		XXH64_hash_t check = XXH3_64bits(hashbuff, trans_hashbuff_bytes) % block->hashmax;
		offset = check;
	} else {
		offset = *offset_prev;
		if (++offset == block->hashmax) offset = 0;
	}

	struct Trans *transcheck = hash_elem[offset];
	while (transcheck) {
		if (transcheck->pstate == pstate && 
				transcheck->inputsym == inputsym &&
				transcheck->epsilon == epsilon) {
				
			*offset_prev = offset;
			return transcheck;
		}
		if (++offset == block->hashmax) offset = 0;
		transcheck = hash_elem[offset];
	}
	
	*offset_prev = block->hashmax;
	return NULL;
}

// Adds transition to automaton
// > assigns new transition the next open index in automaton's trans block
// > hashes transitions by its parent state and sym, both casted as (uintptr)
//   and fed in a buffer to xxHash
//
// Returns pointer to new/existing transition
struct Trans *Trans_add(struct Automaton *a0, struct State *pstate, struct State *dstate, 
		CELL_TYPE *inputsym, CELL_TYPE *popsym, CELL_TYPE *pushsym, CELL_TYPE *dir, CELL_TYPE *writesym)
{
	struct Block *block = &a0->trans;
	struct Trans **hash_elem = block->hash.elem;

	unsigned char input_bit = 0;
	CELL_TYPE inputsym_tmp = (input_bit = (inputsym && 1)) ? *inputsym : CELL_MAX;
	
	/*
	uintptr_t hashbuff[TRANS_HASHBUFF_LEN] = { 
		(uintptr_t) pstate, (uintptr_t) inputsym_tmp,
		~(uintptr_t) pstate ^ (uintptr_t) inputsym_tmp,
		(uintptr_t) pstate ^ ~(uintptr_t) inputsym_tmp,
		(uintptr_t) pstate ^ (uintptr_t) inputsym_tmp,
		~(uintptr_t) pstate ^ ~(uintptr_t) inputsym_tmp,
		~(uintptr_t) pstate, ~(uintptr_t) inputsym_tmp,
	};*/

	TRANS_HASHBUFF_TYPE hashbuff[TRANS_HASHBUFF_LEN] = { 
		(TRANS_HASHBUFF_TYPE) pstate, 
		(TRANS_HASHBUFF_TYPE) inputsym_tmp,
	};

	XXH64_hash_t check = XXH3_64bits(hashbuff, trans_hashbuff_bytes) % block->hashmax;
	XXH64_hash_t offset = check;
	
	unsigned char pop_bit, push_bit, write_bit = 0;
	CELL_TYPE popsym_tmp = (pop_bit = (popsym && 1)) ? *popsym : CELL_MAX;
	CELL_TYPE pushsym_tmp = (push_bit = (pushsym && 1)) ? *pushsym : CELL_MAX;
	CELL_TYPE dir_tmp = (dir && *dir) ? *dir : 0;
	CELL_TYPE writesym_tmp = (write_bit = (writesym && 1)) ? *writesym : CELL_MAX;
	
	struct Trans *transcheck = hash_elem[offset];
	
	while (transcheck != NULL) {
		if (transcheck->pstate == pstate && 
				((transcheck->epsilon == !input_bit) && (transcheck->inputsym == inputsym_tmp))) {
		
			if (transcheck->dstate == dstate &&
					((transcheck->pop == pop_bit) && (transcheck->popsym == popsym_tmp)) &&
					((transcheck->push == push_bit) && (transcheck->pushsym == pushsym_tmp)) &&
					(transcheck->dir == dir_tmp) &&
					((transcheck->write == write_bit) && (transcheck->writesym == writesym_tmp))) {
				return transcheck;
				printf("dupe\n");
			} else {
				transcheck->strans = 1; // another trans shares this sym/pstate
				a0->strans = 1;
			}
		}

		if (++offset == block->hashmax) offset = 0;
		transcheck = hash_elem[offset];
	}

	if (block->size >= block->max) {
		Block_grow(block);
		hash_elem = block->hash.elem;

		check = XXH3_64bits(hashbuff, trans_hashbuff_bytes) % block->hashmax;
		offset = check;
		
		transcheck = hash_elem[offset];
		while (transcheck != NULL) {
			if (++offset == block->hashmax) offset = 0;
			transcheck = hash_elem[offset];
		}
	}

	/*struct Stack *datastacks = block->data.elem;
	struct Stack *strip = &datastacks[block->data.size-1];
	struct Trans *trans = strip->elem;
	struct Trans *newtrans = &trans[strip->size++];*/

	struct Stack *strip = Stack_get(&block->data, block->data.size-1);
	struct Trans *newtrans = Stack_get(strip, strip->size++);

	hash_elem[offset] = newtrans;

	// Set parent, destination states
	newtrans->pstate = pstate;
	newtrans->dstate = dstate;
	
	// Set symbols and symbol indicator bits
	newtrans->epsilon = !input_bit;
	newtrans->inputsym = inputsym_tmp;
	newtrans->pop = pop_bit;
	newtrans->popsym = popsym_tmp;
	newtrans->push = push_bit;
	newtrans->pushsym = pushsym_tmp;
	newtrans->write = write_bit;
	newtrans->writesym = writesym_tmp;
	newtrans->dir = dir_tmp;

	// Initialize other indicator bits
	newtrans->epsilon_loop = 0;
	newtrans->epsilon_mark = 0;
	newtrans->strans = 0;
	newtrans->exec = 0;

	if (input_bit) {
		pstate->syms = 1;
	
		// Only check for new longest_sym if not an epsilon transition
		// > E-trans share a hash with input symbol CELL_MAX, but are
		//   printed as empty space in verbose mode.
		unsigned int new_longest_sym = num_places(newtrans->inputsym);
		if (new_longest_sym > longest_sym) longest_sym = new_longest_sym;

	} else {
		pstate->epsilon = 1;
	}

	block->size++;
	
	return newtrans;
}

// sets start state flag, unsets existing start state's flag
void set_start(struct Automaton *a0, struct State *s0)
{
	s0->start = 1;
	if (a0->start != NULL) a0->start = 0;
	a0->start = s0;
}

// sets final state flag, adds to list of final states
void set_final(struct Automaton *a0, struct State *s0)
{
	s0->final = 1;
	Stack_push(&a0->finals, &s0);
}

// sets reject state flag, adds to list of reject states
void set_reject(struct Automaton *a0, struct State *s0)
{
	s0->reject = 1;
	Stack_push(&a0->rejects, &s0);
}
