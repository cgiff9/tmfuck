#ifndef STACK_H_
#define STACK_H_

#include <stddef.h>

// STACK GROWTH MACROS
//
// These macros determine the growth
// rate of struct Stack, which is used
// extensively throughout tmf.
//
// Note: The 'size' and 'max' fields for struct 
// Stack are integer types, so specifying a
// float for the multiplier (as I've done 
// here) will result in an 'imperfectly' 
// reflected growth rate, but one that is
// reasonably close to the one intended here.
//
// > This ratio should get more accurate as the 
//   stack size gets larger.
//
// Default values:
//    initial max size:  10
//    multiplier:        1.618033988749
//    growth function:   new data.max *= multiplier
//


   #define STACK_INITIAL_MAX   10
   #define STACK_MULTIPLIER    1.618033988749

   #define STACK_GROWTH_FUNC *= STACK_MULTIPLIER

/*===========================================*/

struct Automaton;

enum StackType {
	STATE_STACK,
	TRANS_STACK,
	VAR_STACK,
	STACK_STACK,
	TAPE_STACK,
	CHAR,
	CELL,
	PTRDIFFT,
	STATEPTR,
	VARPTR,
	CHARPTR,
	STACKPTR,
	TRANSPTR,
	TAPEPTR
};

struct Stack {
	enum StackType type;
	void *elem;
	unsigned int size;
	unsigned int max;
};

struct Stack Stack_init(enum StackType type);
struct Stack Stack_init_max(enum StackType type, size_t max);
size_t Stack_free(struct Stack *stk0);
unsigned int Stack_clear(struct Stack *stk0);
unsigned int Stack_clear_empty(struct Stack *stk0, struct Stack *empty_stacks);

unsigned int Stack_push(struct Stack *stk0, void *sym);
signed long int Stack_push_unique(struct Stack *stk0, void *sym);
void Stack_copy(struct Stack *dest, struct Stack *src);

void *Stack_get(struct Stack *stk0, unsigned int index);
void *Stack_pop(struct Stack *stk0);
void *Stack_peek(struct Stack *stk0);

struct Stack *Stack_add(struct Automaton *a0, enum StackType type);
void Stack_print(struct Stack *stk0);

#endif // STACK_H_
