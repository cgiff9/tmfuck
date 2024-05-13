#ifndef STACK_H_
#define STACK_H_

struct Stack {
	int len;
	int max_len;
	int pos;
	char *stack;
};

struct MultiStack {
	int len;
	int max_len;
	struct State *state;
	struct Stack **stacks;
};

struct MultiStackList {
	int len;
	int max_len;
	struct MultiStack **mstacks;
};

struct Stack *Stack_create();
void Stack_push(struct Stack *stack, char symbol);
int Stack_change_pos(struct Stack *stack, char direction);
char Stack_pop(struct Stack *stack);
char Stack_peek(struct Stack *stack);
struct Stack *Stack_copy(struct Stack *stack);
struct MultiStack *MultiStack_create(struct State *state);
void Stack_add(struct MultiStack *ms0, struct Stack *stack);
struct MultiStackList *MultiStackList_create();
void Stack_destroy(struct Stack *stack);
void MultiStack_destroy(struct MultiStack *ms0);
void MultiStackList_destroy(struct MultiStackList *msl0);
void Stack_print(struct Stack *stack);
void MultiStack_print(struct MultiStack *ms0);
void MultiStackList_print(struct MultiStackList *msl0);

struct MultiStack *MultiStack_get(struct MultiStackList *msl0, struct State *state);
void MultiStack_add(struct MultiStackList *msl0, struct MultiStack *ms0);
void Stack_add_to(struct MultiStackList *msl0, struct State *s0, struct Stack *stack);
#endif // STACK_H_
