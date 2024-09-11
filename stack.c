#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stack.h"
#include "auto.h"
#include "var.h"
#include "block.h"
#include "tape.h"
#include "file.h"

// Creates stack, does not initialize memory yet
// Shorthand for Stack_init_max with max zero
//
// Return struct Stack
struct Stack Stack_init(enum StackType type)
{

	/*	struct Stack stk0;
	stk0.type = type;
	stk0.size = 0;
	stk0.max = 4;
	stk0.elem = NULL;
	//stk0.ptape = NULL;*/

	return Stack_init_max(type, 0);
}

// Creates stack with initialized number of of elements
//
// Returns static struct
struct Stack Stack_init_max(enum StackType type, size_t max)
{
	struct Stack stk0;
	stk0.type = type;
	stk0.size = 0;
	stk0.elem = NULL;

	if (max) {
		stk0.max = max;
		
		switch(stk0.type) {
			case STATE_STACK:
				stk0.elem = malloc(sizeof(struct State) * stk0.max);
				break;
			case TRANS_STACK:
				stk0.elem = malloc(sizeof(struct Trans) * stk0.max);
				break;
			case VAR_STACK:
				stk0.elem = malloc(sizeof(struct Var) * stk0.max);
				break;
			case STACK_STACK:
				stk0.elem = malloc(sizeof(struct Stack) * stk0.max);
				break;
			case TAPE_STACK:
				stk0.elem = malloc(sizeof(struct Tape) * stk0.max);
				break;
			case CHAR:
				//stk0.elem = malloc(sizeof(char) * stk0.max);
				stk0.elem = calloc(stk0.max, sizeof(char));
				break;
			case CELL:
				stk0.elem = malloc(sizeof(CELL_TYPE) * stk0.max);
				//stk0.elem = calloc(stk0.max, sizeof(CELL_TYPE));
				break;
			case PTRDIFFT:
				stk0.elem = malloc(sizeof(ptrdiff_t) * stk0.max);
				break;
			case STATEPTR:
				stk0.elem = malloc(sizeof(struct State *) * stk0.max);
				if (stk0.elem) {
					struct State **tmpelem = stk0.elem;
					for (unsigned int i = 0; i < stk0.max; i++) tmpelem[i] = NULL;
					stk0.elem = tmpelem;
				}
				break;
			case VARPTR:
				stk0.elem = malloc(sizeof(struct Var *) * stk0.max);
				if (stk0.elem) {
					struct Var **tmpelem = stk0.elem;
					for (unsigned int i = 0; i < stk0.max; i++) tmpelem[i] = NULL;
					stk0.elem = tmpelem;
				}
				break;
			case CHARPTR:
				stk0.elem = malloc(sizeof(char *) * stk0.max);
				if (stk0.elem) {
					char **tmpelem = stk0.elem;
					for (unsigned int i = 0; i < stk0.max; i++) tmpelem[i] = NULL;
					stk0.elem = tmpelem;
				}
				break;
			case STACKPTR:
				stk0.elem = malloc(sizeof(struct Stack *) * stk0.max);
				if (stk0.elem) {
					struct Stack **tmpelem = stk0.elem;
					for (unsigned int i = 0; i < stk0.max; i++) tmpelem[i] = NULL;
					stk0.elem = tmpelem;
				}
				break;
			case TRANSPTR:
				stk0.elem = malloc(sizeof(struct Trans *) * stk0.max);
				if (stk0.elem) {
					struct Trans **tmpelem = stk0.elem;
					for (unsigned int i = 0; i < stk0.max; i++) tmpelem[i] = NULL;
					stk0.elem = tmpelem;
				}
				break;
			case TAPEPTR:
				stk0.elem = malloc(sizeof(struct Tape *) * stk0.max);
				if (stk0.elem) {
					struct Tape **tmpelem = stk0.elem;
					for (unsigned int i = 0; i < stk0.max; i++) tmpelem[i] = NULL;
					stk0.elem = tmpelem;
				}
				break;
		}

		if (stk0.elem == NULL) {
			fprintf(stderr, "Error allocating initial memory for Stack *elem\n");
			exit(EXIT_FAILURE);
		}

	} else stk0.max = STACK_INITIAL_MAX;

	return stk0;
}

// Frees stack void* allocation
//
// Returns number of bytes freed
size_t Stack_free(struct Stack *stk0)
{
	if (stk0->elem) free(stk0->elem);

	switch(stk0->type) {
		case STATE_STACK:
			return stk0->max * sizeof(struct State);
			break;
		case TRANS_STACK:
			return stk0->max * sizeof(struct Trans);
			break;
		case VAR_STACK:
			return stk0->max * sizeof(struct Var);
			break;
		case STACK_STACK:
			return stk0->max * sizeof(struct Stack);
			break;
		case TAPE_STACK:
			return stk0->max * sizeof(struct Tape);
			break;
		case CHAR:
			return stk0->max * sizeof(char);
			break;
		case CELL:
			return stk0->max * sizeof(CELL_TYPE);
			break;
		case PTRDIFFT:
			return stk0->max * sizeof(ptrdiff_t);
			break;
		case STATEPTR:
			return stk0->max * sizeof(struct State *);
			break;
		case VARPTR:
			return stk0->max * sizeof(struct Var *);
			break;
		case CHARPTR:
			return stk0->max * sizeof(char *);
			break;
		case STACKPTR:
			return stk0->max * sizeof(struct Stack *);
			break;
		case TRANSPTR:
			return stk0->max * sizeof(struct Trans *);
			break;
		case TAPEPTR:
			return stk0->max * sizeof(struct Tape *);
			break;
	}

	return 0;
}

// Resets the size of the array to zero
// > No elements freed or deleted, just marked
//   to be overwritten (size set to zero)
// > If Stack contains other stacks (type == STACKPTR),
//   recursively clear all contained stacks across 
//   all stack 'levels'
// > If Stack is of type TAPEPTR (Tape *), clear
//   the Tapes' contained stacks (tape) and
//   reset heads to zero.
//
// Returns size of stack before being cleared
unsigned int Stack_clear(struct Stack *stk0)
{
	if (!stk0) return 0;
	if (stk0->type == STACKPTR) {
		for (size_t i = 0; i < stk0->size; i++) {
			struct Stack *substack = Stack_get(stk0, i);
			
			// DEBUG:
			//printf("Entering substack...\n");

			Stack_clear(substack);
		}
	}

	if (stk0->type == TAPEPTR) {
		for (size_t i = 0; i < stk0->size; i++) {
			struct Tape *tape = Stack_get(stk0, i);
			Tape_clear(tape);
		}
	}

	// DEBUG:
	//if (stk0->type == STACKPTR) printf("   Clearing STACKPTR stack\n");
	//else printf("   Clearing other root stack\n");
	
	stk0->size = 0;
	
	return 0;
}

unsigned int Stack_clear_empty(struct Stack *stk0, struct Stack *empty_stacks)
{
	if (!stk0) return 0;
	if (stk0->type == STACKPTR) {
		for (size_t i = 0; i < stk0->size; i++) {
			struct Stack *substack = Stack_get(stk0, i);
			
			// DEBUG:
			//printf("Entering substack...\n");

			Stack_clear_empty(substack, empty_stacks);
		}
	}
	
	if (stk0->type == TAPEPTR) {
		for (size_t i = 0; i < stk0->size; i++) {
			struct Tape *tape = Stack_get(stk0, i);
			Tape_clear(tape);
			Stack_push(empty_stacks, &tape);
		}
	}

	// DEBUG:
	//if (stk0->type == STACKPTR) printf("   Clearing STACKPTR stack\n");
	//else printf("   Clearing other root stack\n");
	
	if (stk0->type != STACKPTR && stk0->type != TAPEPTR) { 
		Stack_push(empty_stacks, &stk0);
	}
	
	stk0->size = 0;
	
	return 0;
}
// Generic push: Pushes sym to 'top' of stack
// Takes a *ptr to the object being pushed, which
// is then dereferenced when stored in the stack
// > ie. Want to push a State *ptr? Give Stack_push a State **ptr
//       Want to push a char? Give Stack_push a char *ptr
//       Want to push a char *ptr? Stack_push a char **ptr
// > Does NOT validate stack type (enum), use carefully!
// > Stacks are initialized with no memory allocated, so if
//   stack size = 0, initial memory is allocated
//
// Returns size of stack
unsigned int Stack_push(struct Stack *stk0, void *sym)
{
	//if (!stk0) return 0;
	// If no alloc for stack elements yet, do so here
	if (stk0->elem == NULL) {
		switch(stk0->type) {
			case STATE_STACK:
				stk0->elem = malloc(sizeof(struct State) * stk0->max);
				break;
			case TRANS_STACK:
				stk0->elem = malloc(sizeof(struct Trans) * stk0->max);
				break;
			case VAR_STACK:
				stk0->elem = malloc(sizeof(struct Var) * stk0->max);
				break;
			case STACK_STACK:
				stk0->elem = malloc(sizeof(struct Stack) * stk0->max);
				break;
			case TAPE_STACK:
				stk0->elem = malloc(sizeof(struct Tape) * stk0->max);
				break;
			case CHAR:
				//stk0->elem = malloc(sizeof(char) * stk0->max);
				stk0->elem = calloc(stk0->max, sizeof(char));
				break;
			case CELL:
				stk0->elem = malloc(sizeof(CELL_TYPE) * stk0->max);
				//stk0->elem = calloc(stk0->max, sizeof(CELL_TYPE));
				break;
			case PTRDIFFT:
				stk0->elem = malloc(sizeof(ptrdiff_t) * stk0->max);
				break;
			case STATEPTR:
				stk0->elem = malloc(sizeof(struct State *) * stk0->max);
				if (stk0->elem) {
					struct State **tmpelem = stk0->elem;
					for (unsigned int i = 0; i < stk0->max; i++) tmpelem[i] = NULL;
					stk0->elem = tmpelem;
				}
				break;
			case VARPTR:
				stk0->elem = malloc(sizeof(struct Var *) * stk0->max);
				if (stk0->elem) {
					struct Var **tmpelem = stk0->elem;
					for (unsigned int i = 0; i < stk0->max; i++) tmpelem[i] = NULL;
					stk0->elem = tmpelem;
				}
				break;
			case CHARPTR:
				stk0->elem = malloc(sizeof(char *) * stk0->max);
				if (stk0->elem) {
					char **tmpelem = stk0->elem;
					for (unsigned int i = 0; i < stk0->max; i++) tmpelem[i] = NULL;
					stk0->elem = tmpelem;
				}
				break;
			case STACKPTR:
				stk0->elem = malloc(sizeof(struct Stack *) * stk0->max);
				if (stk0->elem) {
					struct Stack **tmpelem = stk0->elem;
					for (unsigned int i = 0; i < stk0->max; i++) tmpelem[i] = NULL;
					stk0->elem = tmpelem;
				}
				break;
			case TRANSPTR:
				stk0->elem = malloc(sizeof(struct Trans *) * stk0->max);
				if (stk0->elem) {
					struct Trans **tmpelem = stk0->elem;
					for (unsigned int i = 0; i < stk0->max; i++) tmpelem[i] = NULL;
					stk0->elem = tmpelem;
				}
				break;
			case TAPEPTR:
				stk0->elem = malloc(sizeof(struct Tape *) * stk0->max);
				if (stk0->elem) {
					struct Tape **tmpelem = stk0->elem;
					for (unsigned int i = 0; i < stk0->max; i++) tmpelem[i] = NULL;
					stk0->elem = tmpelem;
				}
				break;
		}

		if (stk0->elem == NULL) {
			fprintf(stderr, "Stack push: Error allocating initial memory for Stack *elem\n");
			exit(EXIT_FAILURE);
		}
	
	// Realloc size up if needed
	} else if (stk0->size >= stk0->max) {
		void *newelem = NULL;

		// See stack.h for more info
		stk0->max STACK_GROWTH_FUNC;

		switch(stk0->type) {
			case STATE_STACK:
				newelem = realloc(stk0->elem, sizeof(struct State) * stk0->max);
				break;
			case TRANS_STACK:
				newelem = realloc(stk0->elem, sizeof(struct Trans) * stk0->max);
				break;
			case VAR_STACK:
				newelem = realloc(stk0->elem, sizeof(struct Var) * stk0->max);
				break;
			case STACK_STACK:
				newelem = realloc(stk0->elem, sizeof(struct Stack) * stk0->max);
				break;
			case TAPE_STACK:
				newelem = realloc(stk0->elem, sizeof(struct Tape) * stk0->max);
				break;
			case CHAR:
				newelem = realloc(stk0->elem, sizeof(char) * stk0->max);
				break;
			case CELL:
				newelem = realloc(stk0->elem, sizeof(CELL_TYPE) * stk0->max);
				break;
			case PTRDIFFT:
				newelem = realloc(stk0->elem, sizeof(ptrdiff_t) * stk0->max);
				break;
			case STATEPTR:
				newelem = realloc(stk0->elem, sizeof(struct State *) * stk0->max);
				if (newelem) {
					struct State **tmpelem = newelem;
					for (unsigned int i = stk0->size; i < stk0->max; i++) tmpelem[i] = NULL;
					newelem = tmpelem;
				}
				break;
			case VARPTR:
				newelem = realloc(stk0->elem, sizeof(struct Var *) * stk0->max);
				if (newelem) {
					struct Var **tmpelem = newelem;
					for (unsigned int i = stk0->size; i < stk0->max; i++) tmpelem[i] = NULL;
					newelem = tmpelem;
				}

				break;
			case CHARPTR:
				newelem = realloc(stk0->elem, sizeof(char *) * stk0->max);
				if (newelem) {
					char **tmpelem = newelem;
					for (unsigned int i = stk0->size; i < stk0->max; i++) tmpelem[i] = NULL;
					newelem = tmpelem;
				}
				break;
			case STACKPTR:
				newelem = realloc(stk0->elem, sizeof(struct Stack *) * stk0->max);
				if (newelem) {
					struct Stack **tmpelem = newelem;
					for (unsigned int i = stk0->size; i < stk0->max; i++) tmpelem[i] = NULL;
					newelem = tmpelem;
				}
				break;
			case TRANSPTR:
				newelem = realloc(stk0->elem, sizeof(struct Trans *) * stk0->max);
				if (newelem) {
					struct Trans **tmpelem = newelem;
					for (unsigned int i = stk0->size; i < stk0->max; i++) tmpelem[i] = NULL;
					newelem = tmpelem;
				}
				break;
			case TAPEPTR:
				newelem = realloc(stk0->elem, sizeof(struct Tape *) * stk0->max);
				if (newelem) {
					struct Tape **tmpelem = newelem;
					for (unsigned int i = stk0->size; i < stk0->max; i++) tmpelem[i] = NULL;
					newelem = tmpelem;
				}
				break;
		}

		if (newelem == NULL) {
			fprintf(stderr, "Error reallocating memory during stack push\n");
			free(stk0->elem);
			exit(EXIT_FAILURE);
		}
		stk0->elem = newelem;
	}

	// Dereference sym *ptr based on stack's type, then 'push' it to stack
	// > void *ptr can NOT be type-validated with the stack type,
	//   care must be taken when constructing/passing the 'sym' arg
	// > stack elements that are themselves pointers are are cast 'one
	//   pointer level up' to ensure accurate dereferencing
	switch(stk0->type) {
		case STATE_STACK:
			{
				struct State *tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(struct State *)sym;
			}
			break;
		case TRANS_STACK:
			{
				struct Trans *tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(struct Trans *)sym;
			}
			break;
		case VAR_STACK:
			{
				struct Var *tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(struct Var *)sym;
			}
			break;
		case STACK_STACK:
			{
				struct Stack *tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(struct Stack *)sym;
			}
			break;
		case TAPE_STACK:
			{
				struct Tape *tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(struct Tape *)sym;
			}
			break;
		case CHAR:
			{
				char *tmpelem = stk0->elem;           // implicit cast of stack void *elements region
				tmpelem[stk0->size] = *(char *)sym;   // explicit cast of void *sym stack element
			}
			break;
		case CELL:
			{
				CELL_TYPE *tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(CELL_TYPE *)sym;
			}
			break;
		case PTRDIFFT:
			{
				ptrdiff_t *tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(ptrdiff_t *)sym;
			}
			break;
		case STATEPTR:
			{
				struct State **tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(struct State **)sym;
			}
			break;
		case VARPTR:
			{
				struct Var **tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(struct Var **)sym;
			}
			break;
		case CHARPTR:
			{
				char **tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(char **)sym;
			}
			break;
		case STACKPTR:
			{
				struct Stack **tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(struct Stack **)sym;
			}
			break;
		case TRANSPTR:
			{
				struct Trans **tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(struct Trans **)sym;
			}
			break;
		case TAPEPTR:
			{
				struct Tape **tmpelem = stk0->elem;
				tmpelem[stk0->size] = *(struct Tape **)sym;
			}
			break;
	}

	return stk0->size++;
}

// Same as Stack_push, but does not add duplicate syms (elements)
//
// Returns index of new/existing stack element
signed long int Stack_push_unique(struct Stack *stk0, void *sym)
{
	if (stk0->elem == NULL) {
		switch(stk0->type) {
			case CHAR:
				stk0->elem = malloc(sizeof(char) * stk0->max);
				break;
			case CELL:
				stk0->elem = malloc(sizeof(CELL_TYPE) * stk0->max);
				break;
			case PTRDIFFT:
				stk0->elem = malloc(sizeof(ptrdiff_t) * stk0->max);
				break;
			case STATEPTR:
				stk0->elem = malloc(sizeof(struct State *) * stk0->max);
				break;
			case VARPTR:
				stk0->elem = malloc(sizeof(struct Var *) * stk0->max);
				break;
			case CHARPTR:
				stk0->elem = malloc(sizeof(char *) * stk0->max);
				break;
			case STACKPTR:
				stk0->elem = malloc(sizeof(struct Stack *) * stk0->max);
				break;
			case TRANSPTR:
				stk0->elem = malloc(sizeof(struct Trans *) * stk0->max);
				break;
			case TAPEPTR:
				stk0->elem = malloc(sizeof(struct Tape *) * stk0->max);
				break;
			case STATE_STACK:
			case TRANS_STACK:
			case VAR_STACK:
			case STACK_STACK:
			case TAPE_STACK:
				break;
		}

		if (stk0->elem == NULL) {
			fprintf(stderr, "Error allocating initial memory for Stack *elem\n");
			exit(EXIT_FAILURE);
		}
	
	} else if (stk0->size >= stk0->max) {
		void *newelem = NULL;

		// See stack.h for more info
		stk0->max STACK_GROWTH_FUNC;

		switch(stk0->type) {
			case CHAR:
				newelem = realloc(stk0->elem, sizeof(char) * stk0->max);
				break;
			case CELL:
				newelem = realloc(stk0->elem, sizeof(CELL_TYPE) * stk0->max);
				break;
			case PTRDIFFT:
				newelem = realloc(stk0->elem, sizeof(ptrdiff_t) * stk0->max);
				break;
			case STATEPTR:
				newelem = realloc(stk0->elem, sizeof(struct State *) * stk0->max);
				break;
			case VARPTR:
				newelem = realloc(stk0->elem, sizeof(struct Var *) * stk0->max);
				break;
			case CHARPTR:
				newelem = realloc(stk0->elem, sizeof(char *) * stk0->max);
				break;
			case STACKPTR:
				newelem = realloc(stk0->elem, sizeof(struct Stack *) * stk0->max);
				break;
			case TRANSPTR:
				newelem = realloc(stk0->elem, sizeof(struct Trans *) * stk0->max);
				break;
			case TAPEPTR:
				newelem = realloc(stk0->elem, sizeof(struct Tape *) * stk0->max);
				break;
			case STATE_STACK:
			case TRANS_STACK:
			case VAR_STACK:
			case STACK_STACK:
			case TAPE_STACK:
				break;
		}

		if (newelem == NULL) {
			fprintf(stderr, "Error reallocating memory during stack push\n");
			free(stk0->elem);
			exit(EXIT_FAILURE);
		}
		stk0->elem = newelem;
	}

	switch(stk0->type) {
		case CHAR:
			{
				char *tmpelem = stk0->elem;  // assignment does implicit cast
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(char *)sym) return -1; //stk0->size;
				}
				tmpelem[stk0->size] = *(char *)sym;
			}
			break;
		case CELL:
			{
				CELL_TYPE *tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(CELL_TYPE *)sym) return -1; //stk0->size;
				}
				tmpelem[stk0->size] = *(CELL_TYPE *)sym;
			}
			break;
		case PTRDIFFT:
			{
				ptrdiff_t *tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(ptrdiff_t *)sym) return -1; //stk0->size;
				}
				tmpelem[stk0->size] = *(ptrdiff_t *)sym;
			}
			break;

		case STATEPTR:
			{
				struct State **tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(struct State **)sym) return -1; //return stk0->size;
				}
				tmpelem[stk0->size] = *(struct State **)sym;
			}
			break;
		case VARPTR:
			{
				struct Var **tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(struct Var **)sym) return -1; //stk0->size;
				}
				tmpelem[stk0->size] = *(struct Var **)sym;
			}
			break;
		case CHARPTR:
			{
				char **tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(char **)sym) return -1; //stk0->size;
				}
				tmpelem[stk0->size] = *(char **)sym;
			}
			break;
		case STACKPTR:
			{
				struct Stack **tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(struct Stack **)sym) return -1; //stk0->size;
				}
				tmpelem[stk0->size] = *(struct Stack **)sym;
			}
			break;
		case TRANSPTR:
			{
				struct Trans **tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(struct Trans **)sym) return -1; //stk0->size;
				}
				tmpelem[stk0->size] = *(struct Trans **)sym;
			}
			break;
		case TAPEPTR:
			{
				struct Tape **tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(struct Tape **)sym) return -1; //stk0->size;
				}
				tmpelem[stk0->size] = *(struct Tape **)sym;
			}
			break;
		case STATE_STACK:
		case TRANS_STACK:
		case VAR_STACK:
		case STACK_STACK:
		case TAPE_STACK:
			break;
	}

	return stk0->size++;
}
/*
int Stack_elem_is_unique(struct Stack *stk0, void *sym)
{
	// If stack empty/unalloc'd, sym must be unique
	if (!stk0->elem || !stk0->size) return -1;

	switch (stk0->type) {
		case CHAR:
			{
				char *tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(char *)sym) return i;
				}
			}
			break;
		case CELL:
			{
				CELL_TYPE *tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(CELL_TYPE *)sym) return i;
				}
			}
			break;
		case PTRDIFFT:
			{
				ptrdiff_t *tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(ptrdiff_t *)sym) return i;
				}
			}
			break;
		case STATEPTR:
			{	
				struct State **tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
				//for (long int i = stk0->size-1; i >=0; i--) {
					if (tmpelem[i] == *(struct State **)sym) return i;
				}
			}
			break;
		case VARPTR:
			{	
				struct Var **tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(struct Var **)sym) return i;
				}
			}
			break;
		case CHARPTR:
			{	
				char **tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(char **)sym) return i;
				}
			}
			break;
		case STACKPTR:
			{	
				struct Stack **tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(struct Stack **)sym) return i;
				}
			}
			break;
		case TRANSPTR:
			{	
				struct Trans **tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(struct Trans **)sym) return i;
				}
			}
			break;
		case TAPEPTR:
			{	
				struct Tape **tmpelem = stk0->elem;
				for (unsigned int i = 0; i < stk0->size; i++) {
					if (tmpelem[i] == *(struct Tape **)sym) return i;
				}
			}
			break;
	}

	return -1;
}*/

// Copies stack elements from one to the other
// > resizes dest stack if it's too small
// > mallocs initial memory for dest if needed
// > stacks must be same type (enum StackType)
// > if src empty, dest is cleared (made empty)
//
// Returns void
void Stack_copy(struct Stack *dest, struct Stack *src)
{
	if (!src || !dest) return;
	if (src->type != dest->type) return;
	if (!src->size) { 
		Stack_clear(dest);
		return;
	}

	if (!dest->elem || src->size >= dest->max) {
		void *tmpelem = NULL;
		switch(src->type) {
			case CHAR:
				if (dest->elem == NULL)
					tmpelem = malloc(sizeof(char) * src->max);
				else
					tmpelem = realloc(dest->elem, sizeof(char) * src->max);
				break;
			case CELL:
				if (dest->elem == NULL)
					tmpelem = malloc(sizeof(CELL_TYPE) * src->max);
				else
					tmpelem = realloc(dest->elem, sizeof(CELL_TYPE) * src->max);
				break;
			case PTRDIFFT:
				if (dest->elem == NULL)
					tmpelem = malloc(sizeof(ptrdiff_t) * src->max);
				else
					tmpelem = realloc(dest->elem, sizeof(ptrdiff_t) * src->max);
				break;
			case STATEPTR:
				if (dest->elem == NULL)
					tmpelem = malloc(sizeof(struct State *) * src->max);
				else
					tmpelem = realloc(dest->elem, sizeof(struct State *) * src->max);
				break;
			case VARPTR:
				if (dest->elem == NULL)
					tmpelem = malloc(sizeof(struct Var *) * src->max);
				else
					tmpelem = realloc(dest->elem, sizeof(struct Var *) * src->max);
				break;
			case CHARPTR:
				if (dest->elem == NULL)
					tmpelem = malloc(sizeof(char *) * src->max);
				else
					tmpelem = realloc(dest->elem, sizeof(char *) * src->max);
				break;
			case STACKPTR:
				if (dest->elem == NULL)
					tmpelem = malloc(sizeof(struct Stack *) * src->max);
				else
					tmpelem = realloc(dest->elem, sizeof(struct Stack *) * src->max);
				break;
			case TRANSPTR:
				if (dest->elem == NULL)
					tmpelem = malloc(sizeof(struct Trans *) * src->max);
				else
					tmpelem = realloc(dest->elem, sizeof(struct Trans *) * src->max);
				break;
			case TAPEPTR:
				if (dest->elem == NULL)
					tmpelem = malloc(sizeof(struct Tape *) * src->max);
				else
					tmpelem = realloc(dest->elem, sizeof(struct Tape *) * src->max);
				break;
			case STATE_STACK:
			case TRANS_STACK:
			case VAR_STACK:
			case STACK_STACK:
			case TAPE_STACK:
				break;
		}

		if (tmpelem == NULL) {
			fprintf(stderr, "Error reallocating memory during stack copy\n");
			exit(EXIT_FAILURE);
		}

		dest->elem = tmpelem;
		dest->max = src->max;
	}

	size_t numbytes = 0;
	switch(src->type) {
		case CHAR:
			numbytes = src->size * sizeof(char);
			break;
		case CELL:
			numbytes = src->size * sizeof(CELL_TYPE);
			break;
		case PTRDIFFT:
			numbytes = src->size * sizeof(ptrdiff_t);
			break;
		case STATEPTR:
			numbytes = src->size * sizeof(struct State *);
			break;
		case VARPTR:
			numbytes = src->size * sizeof(struct Var *);
			break;
		case CHARPTR:
			numbytes = src->size * sizeof(char *);
			break;
		case STACKPTR:
			numbytes = src->size * sizeof(struct Stack *);
			break;
		case TRANSPTR:
			numbytes = src->size * sizeof(struct Trans *);
			break;
		case TAPEPTR:
			numbytes = src->size * sizeof(struct Tape *);
			break;
		case STATE_STACK:
		case TRANS_STACK:
		case VAR_STACK:
		case STACK_STACK:
		case TAPE_STACK:
			break;
	}

	memcpy(dest->elem, src->elem, numbytes);
	dest->size = src->size;

	return;
}

// Returns stack element at given index
// > interpreting return value is left to the programmer
//   -> stacks that hold pointer types may simply be cast
//      to the appropriate pointer
//      ie. struct State *s = Stack_pop(stack);
//   -> stacks that hold value types (char, CELL_TYPE, etc)
//      must be cast to the appropriate pointer type, then
//      dereferenced to obtain the value
//      ie. char c = *(char*)Stack_get(chars, 20);
//      ie. char cptr = Stack_get(chars, 20);
//          char c = *cptr;
// > NULL returned if index is out of bounds
//
// Returns void * to stack element at specified index
void *Stack_get(struct Stack *stk0, unsigned int index)
{
	if (index >= stk0->size) return NULL;

	switch(stk0->type) {
		case STATE_STACK:
			{
				struct State *tmpelem = stk0->elem;
				return &tmpelem[index];
			}
			break;
		case TRANS_STACK:
			{
				struct Trans *tmpelem = stk0->elem;
				return &tmpelem[index];
			}
			break;
		case VAR_STACK:
			{
				struct Var *tmpelem = stk0->elem;
				return &tmpelem[index];
			}
			break;
		case STACK_STACK:
			{
				struct Stack *tmpelem = stk0->elem;
				return &tmpelem[index];
			}
			break;
		case TAPE_STACK:
			{
				struct Tape *tmpelem = stk0->elem;
				return &tmpelem[index];
			}
			break;
		case CHAR:
			{
				char *tmpelem = stk0->elem;
				return &tmpelem[index];
			}
			break;
		case CELL:
			{
			CELL_TYPE *tmpelem = stk0->elem;
			return &tmpelem[index];
			//return tmpelem + index * sizeof(CELL_TYPE);
			}
			break;
		case PTRDIFFT:
			{
			ptrdiff_t *tmpelem = stk0->elem;
			return &tmpelem[index];
			}
			break;
		case STATEPTR:
			{
			struct State **tmpelem = stk0->elem;
			return tmpelem[index];
			}
			break;	
		case VARPTR:
			{
			struct Var **tmpelem = stk0->elem;
			return tmpelem[index];
			}
			break;
		case CHARPTR:
			{
			char **tmpelem = stk0->elem;
			return tmpelem[index];
			}
			break;
		case STACKPTR:
			{
			struct Stack **tmpelem = stk0->elem;
			return tmpelem[index];
			}
			break;
		case TRANSPTR:
			{
			struct Trans **tmpelem = stk0->elem;
			return tmpelem[index];
			}
			break;
		case TAPEPTR:
			{
			struct Tape **tmpelem = stk0->elem;
			return tmpelem[index];
			}
			break;
	}

	return NULL;
}

// Returns stack element at stack top and reduces
// size of the stack
// > popped element still exists in stack's memory
//   (stack size is merely decremented)
// > interpreting return value is left to the programmer
//   -> stacks that hold pointer types may simply be cast
//      to the appropriate pointer
//      ie. struct State *s = Stack_pop(stack);
//   -> stacks that hold value types (char, CELL_TYPE, etc)
//      must be cast to the appropriate pointer type, then
//      dereferenced to obtain the value
//      ie. char c = *(char*)Stack_pop(chars);
//      ie. char cptr = Stack_pop(chars);
//          char c = *cptr;
// > NULL returned if stack is empty
//
// Returns void * to top (popped) stack element
void *Stack_pop(struct Stack *stk0)
{
	if (!stk0->size) return NULL;

	switch(stk0->type) {
		case STATE_STACK:
			{
				struct State *tmpelem = stk0->elem;
				return &tmpelem[--stk0->size];
			}
			break;
		case TRANS_STACK:
			{
				struct Trans *tmpelem = stk0->elem;
				return &tmpelem[--stk0->size];
			}
			break;
		case VAR_STACK:
			{
				struct Var *tmpelem = stk0->elem;
				return &tmpelem[--stk0->size];
			}
			break;
		case STACK_STACK:
			{
				struct Stack *tmpelem = stk0->elem;
				return &tmpelem[--stk0->size];
			}
			break;
		case TAPE_STACK:
			{
				struct Tape *tmpelem = stk0->elem;
				return &tmpelem[--stk0->size];
			}
			break;
		case CHAR:
			{
				char *tmpelem = stk0->elem;
				return &tmpelem[--stk0->size];
			}
			break;
		case CELL:
			{
			CELL_TYPE *tmpelem = stk0->elem;
			return &tmpelem[--stk0->size];
			}
			break;
		case PTRDIFFT:
			{
			ptrdiff_t *tmpelem = stk0->elem;
			return &tmpelem[--stk0->size];
			}
			break;
		case STATEPTR:
			{
			struct State **tmpelem = stk0->elem;
			return tmpelem[--stk0->size];
			}
			break;	
		case VARPTR:
			{
			struct Var **tmpelem = stk0->elem;
			return tmpelem[--stk0->size];
			}
			break;
		case CHARPTR:
			{
			char **tmpelem = stk0->elem;
			return tmpelem[--stk0->size];
			}
			break;
		case STACKPTR:
			{
			struct Stack **tmpelem = stk0->elem;
			return tmpelem[--stk0->size];
			}
			break;
		case TRANSPTR:
			{
			struct Trans **tmpelem = stk0->elem;
			return tmpelem[--stk0->size];
			}
			break;
		case TAPEPTR:
			{
			struct Tape **tmpelem = stk0->elem;
			return tmpelem[--stk0->size];
			}
			break;
	}

	return NULL;
}

// Returns stack element at stack top, but does NOT
// remove (pop) it from the stack
// > interpreting return value is left to the programmer
//   -> stacks that hold pointer types may simply be cast
//      to the appropriate pointer
//      ie. struct State *s = Stack_peek(stack);
//   -> stacks that hold value types (char, CELL_TYPE, etc)
//      must be cast to the appropriate pointer type, then
//      dereferenced to obtain the value
//      ie. char c = *(char*)Stack_peek(chars);
//      ie. char cptr = Stack_peek(chars);
//          char c = *cptr;
// > NULL returned if stack is empty
//
// Returns void * to top stack element
void *Stack_peek(struct Stack *stk0)
{
	if (!stk0->size) return NULL;

	switch(stk0->type) {
		case STATE_STACK:
			{
				struct State *tmpelem = stk0->elem;
				return &tmpelem[stk0->size-1];
			}
			break;
		case TRANS_STACK:
			{
				struct Trans *tmpelem = stk0->elem;
				return &tmpelem[stk0->size-1];
			}
			break;
		case VAR_STACK:
			{
				struct Var *tmpelem = stk0->elem;
				return &tmpelem[stk0->size-1];
			}
			break;
		case STACK_STACK:
			{
				struct Stack *tmpelem = stk0->elem;
				return &tmpelem[stk0->size-1];
			}
			break;
		case TAPE_STACK:
			{
				struct Tape *tmpelem = stk0->elem;
				return &tmpelem[stk0->size-1];
			}
			break;
		case CHAR:
			{
				char *tmpelem = stk0->elem;
				return &tmpelem[stk0->size-1];
			}
			break;
		case CELL:
			{
			CELL_TYPE *tmpelem = stk0->elem;
			return &tmpelem[stk0->size-1];
			}
			break;
		case PTRDIFFT:
			{
			ptrdiff_t *tmpelem = stk0->elem;
			return &tmpelem[stk0->size-1];
			}
			break;
		case STATEPTR:
			{
			struct State **tmpelem = stk0->elem;
			return tmpelem[stk0->size-1];
			}
			break;	
		case VARPTR:
			{
			struct Var **tmpelem = stk0->elem;
			return tmpelem[stk0->size-1];
			}
			break;
		case CHARPTR:
			{
			char **tmpelem = stk0->elem;
			return tmpelem[stk0->size-1];
			}
			break;
		case STACKPTR:
			{
			struct Stack **tmpelem = stk0->elem;
			return tmpelem[stk0->size-1];
			}
			break;
		case TRANSPTR:
			{
			struct Trans **tmpelem = stk0->elem;
			return tmpelem[stk0->size-1];
			}
			break;
		case TAPEPTR:
			{
			struct Tape **tmpelem = stk0->elem;
			return tmpelem[stk0->size-1];
			}
			break;
	}

	return NULL;
}

// Explicit 'get' functions
// > does an enum BlockType check to ensure function
//   is being used with the correct stack
//
// Returns element at specified index (no dereferencing needed)
// > Return -1 or NULL if index too large
/*
signed short int Stack_char_get(struct Stack *stack, unsigned int index)
{
	if (index >= stack->size) return -1;
	if (stack->type != CHAR) return -1;
	char *tmpelem = stack->elem;
	return tmpelem[index];
}

//signed long int Stack_cell_get(struct Stack *stack, unsigned int index)
ptrdiff_t Stack_cell_get(struct Stack *stack, unsigned int index)
{
	if (index >= stack->size) return PTRDIFF_MIN;
	//if (index >= stack->max) return PTRDIFF_MIN;
	if (stack->type != CELL) return PTRDIFF_MIN;
	CELL_TYPE *tmpelem = stack->elem;
	return tmpelem[index];
}

ptrdiff_t Stack_ptrdifft_get(struct Stack *stack, unsigned int index)
{
	if (index >= stack->size) return -1;
	if (stack->type != PTRDIFFT) return -1;
	ptrdiff_t *tmpelem = stack->elem;
	return tmpelem[index];
}

struct State *Stack_Stateptr_get(struct Stack *stack, unsigned int index)
{
	if (index >= stack->size) return NULL;
	if (stack->type != STATEPTR) return NULL;

	struct State **tmpelem = stack->elem;
	return tmpelem[index];
}

struct Var *Stack_Varptr_get(struct Stack *stack, unsigned int index)
{
	if (index >= stack->size) return NULL;
	if (stack->type != VARPTR) return NULL;

	struct Var **tmpelem = stack->elem;
	return tmpelem[index];
}

char *Stack_charptr_get(struct Stack *stack, unsigned int index)
{
	if (index >= stack->size) return NULL;
	if (stack->type != CHARPTR) return NULL;

	char **tmpelem = stack->elem;
	return tmpelem[index];
}

struct Stack *Stack_Stackptr_get(struct Stack *stack, unsigned int index)
{
	if (index >= stack->size || !stack->elem) return NULL;
	if (stack->type != STACKPTR) return NULL;

	struct Stack **tmpelem = stack->elem;
	return tmpelem[index];
}

struct Trans *Stack_Transptr_get(struct Stack *stack, unsigned int index)
{
	if (index >= stack->size || !stack->elem) return NULL;
	if (stack->type != TRANSPTR) return NULL;

	struct Trans **tmpelem = stack->elem;
	return tmpelem[index];
}

struct Tape *Stack_Tapeptr_get(struct Stack *stack, unsigned int index)
{
	if (index >= stack->size || !stack->elem) return NULL;
	if (stack->type != TAPEPTR) return NULL;

	struct Tape **tmpelem = stack->elem;
	return tmpelem[index];
}
*/

// Initializes new stack in automaton's stack block 
//
// Optional stack ptr may be provided that will
// serve as a starting template for the new stack
// > If NULL, the newstack is added as empty
// > If new stack type does not match existing
//   stack type, return NULL
//
// Returns pointer to the new stack
struct Stack *Stack_add(struct Automaton *a0, enum StackType type)
{
	struct Block *block = &a0->stacks;

	if (block->size >= block->max) {
		Block_grow(block);
	}

	/*struct Stack *datastacks = block->data.elem;
	struct Stack *strip = &datastacks[block->data.size-1];
	struct Stack *stacks = strip->elem;
	struct Stack *newstack = &stacks[strip->size++];*/

	struct Stack *strip = Stack_get(&block->data, block->data.size-1);
	struct Stack *newstack = Stack_get(strip, strip->size++);

	switch(type) {
		case CHAR:
			*newstack = Stack_init(CHAR);
			break;
		case CELL:
			*newstack = Stack_init(CELL);
			break;
		case PTRDIFFT:
			*newstack = Stack_init(PTRDIFFT);
			break;
		case STATEPTR:
			*newstack = Stack_init(STATEPTR);
			break;
		case VARPTR:
			*newstack = Stack_init(VARPTR);
			break;
		case CHARPTR:
			*newstack = Stack_init(CHARPTR);
			break;
		case STACKPTR:
			*newstack = Stack_init(STACKPTR);
			break;
		case TRANSPTR:
			*newstack = Stack_init(TRANSPTR);
			break;
		case TAPEPTR:
			*newstack = Stack_init(TAPEPTR);
			break;
		case STATE_STACK:
		case TRANS_STACK:
		case VAR_STACK:
		case STACK_STACK:
		case TAPE_STACK:
			break;

	}
	block->size++;
	return newstack;
}


/*
signed long int Stack_Stateptr_index(struct Stack *stack, struct State *state)
{
	if (stack->elem == NULL) return -1;
	struct State **elem = stack->elem;
	for (unsigned int i = 0; i < stack->size; i++) {
		//struct State *elem = Stack_Stateptr_get(stack, i);
		if (elem[i] == state) return i;
	}
	return -1;
}
*/

// Explicit 'pop' functions
// Removes element from top of stack
// > Allocated memory for void *elem is not
//   adjusted. The 'size' field simply
//   determines which elements are considered
//   stored (lazy deletion).
//
// Returns element at the top of the stack
// > Returns -1 or NULL if stack empty
// > (for char) -1 is never expected to be a 
//   stack element value, so is used as empty
//   return flag
/*
signed short int Stack_char_pop(struct Stack *stack)
{
	if (!stack->size) return -1;
	if (stack->type != CHAR) return -1;
	char *tmpelem = stack->elem;
	return tmpelem[--stack->size];
}

//signed long int Stack_cell_pop(struct Stack *stack)
ptrdiff_t Stack_cell_pop(struct Stack *stack)
{
	if (!stack || !stack->size) return -1;
	if (stack->type != CELL) return -1;
	CELL_TYPE *tmpelem = stack->elem;
	return tmpelem[--stack->size];
}

ptrdiff_t Stack_ptrdifft_pop(struct Stack *stack)
{
	if (!stack || !stack->size) return -1;
	if (stack->type != PTRDIFFT) return -1;
	ptrdiff_t *tmpelem = stack->elem;
	return tmpelem[--stack->size];
}

struct State *Stack_Stateptr_pop(struct Stack *stack)
{
	if (!stack->size) return NULL;
	if (stack->type != STATEPTR) return NULL;
	struct State **tmpelem = stack->elem;
	return tmpelem[--stack->size];
}

struct Var *Stack_Varptr_pop(struct Stack *stack)
{
	if (!stack->size) return NULL;
	if (stack->type != VARPTR) return NULL;
	struct Var **tmpelem = stack->elem;
	return tmpelem[--stack->size];
}

struct Stack *Stack_Stackptr_pop(struct Stack *stack)
{
	if (!stack->size) return NULL;
	//if (stack->type != STACKPTR) return NULL;
	struct Stack **tmpelem = stack->elem;
	return tmpelem[--stack->size];
}

struct Trans *Stack_Transptr_pop(struct Stack *stack)
{
	if (!stack->size) return NULL;
	if (stack->type != TRANSPTR) return NULL;
	struct Trans **tmpelem = stack->elem;
	return tmpelem[--stack->size];
}

struct Tape *Stack_Tapeptr_pop(struct Stack *stack)
{
	if (!stack->size) return NULL;
	if (stack->type != TAPEPTR) return NULL;
	struct Tape **tmpelem = stack->elem;
	return tmpelem[--stack->size];
}

signed short int Stack_char_peek(struct Stack *stack)
{
	if (!stack->size) return -1;
	if (stack->type != CHAR) return -1;
	char *tmpelem = stack->elem;
	return tmpelem[stack->size - 1];
}

//signed long int Stack_cell_peek(struct Stack *stack)
ptrdiff_t Stack_cell_peek(struct Stack *stack)
{
	if (!stack->size) return -1;
	if (stack->type != CELL) return -1;
	CELL_TYPE *tmpelem = stack->elem;
	return tmpelem[stack->size - 1];
}

struct State *Stack_Stateptr_peek(struct Stack *stack)
{
	if (!stack->size) return NULL;
	if (stack->type != STATEPTR) return NULL;
	struct State **tmpelem = stack->elem;
	return tmpelem[stack->size - 1];
}
*/

void Stack_print(struct Stack *stk0)
{
	if (!stk0->size) return;

	int buff_len = tape_print_width * print_max + 4 + 1; // +2 for dec spacers, 
																				// +2 for head brackets, 
																				// +1 for null term

	unsigned int begin = 0;
	if (stk0->size > tape_print_width) begin = stk0->size - tape_print_width;

	// fill buffer with printed symbols
	char buff[buff_len];
	buff[0] = '\0';
	char *fmt = NULL;
	unsigned int num_chars = 0;
	int prev_dec = 0;
	unsigned int i = 0;
	CELL_TYPE *cell = NULL;
	for (i = begin; i < stk0->size; i++) {
		cell = Stack_get(stk0, i);
		if (*cell > 31 && *cell < 127) {
			if (i != begin && prev_dec) fmt = "|%c";
			else fmt = "%c";
			prev_dec = 0;
		} else {
			if (i != begin) 
				fmt = (sign) ? "|%02" SIGNED_PRINT_FMT : "|%02" UNSIGNED_PRINT_FMT;
			else 
				fmt = (sign) ? "%02" SIGNED_PRINT_FMT : "%02" UNSIGNED_PRINT_FMT;
			prev_dec = 1;
		}
		num_chars += (sign) ? 
			snprintf(buff + num_chars, buff_len, fmt, (SIGNED_PRINT_TYPE) *cell) :
			snprintf(buff + num_chars, buff_len, fmt, (UNSIGNED_PRINT_TYPE) *cell);
	}

	/*ptrdiff_t cell = Stack_cell_get(stk0, stk0->size-1);
	if (cell > 31 && cell < 127) {
		if (prev_dec) fmt = "|[%c]";
		else fmt = "[%c]";
	} else {
		fmt = "|[%02td]";
	}	
	num_chars += snprintf(buff + num_chars, buff_len, fmt, cell);
*/

	if (num_chars <= tape_print_width) {
		printf("   %s", buff);
	} else {
		printf("...%s", buff + num_chars - tape_print_width);
	}

	return;
}
