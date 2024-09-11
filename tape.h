#ifndef TAPE_H_
#define TAPE_H_

#include "stack.h"
#include "auto.h"

struct Tape {
	//struct Stack *right;
	//struct Stack *left;
	struct Stack tape;
	//struct Stack *tape;
	//struct Stack *stacks;
	//unsigned int head;
	//long head;
	int head;
	unsigned char ghost:1;
};

struct Tape Tape_init();
struct Tape Tape_init_size(size_t max);
void Tape_clear(struct Tape *tp0);
size_t Tape_free(struct Tape *tp0);
size_t Tape_pos(struct Tape *tp0, int dir);
int Tape_write(struct Tape *tp0, CELL_TYPE *sym);
CELL_TYPE Tape_head(struct Tape *tp0);
void Tape_print(struct Tape *tp0);
struct Tape *Tape_add(struct Automaton *a0);
void Tape_copy(struct Tape *dest, struct Tape *src);
void Tape_ghost(struct Tape *dest, struct Tape *src);
struct Tape Tape_import(struct Automaton *a0, char *input_str, int is_file);

#endif // TAPE_H_

