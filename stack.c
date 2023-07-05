#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stack.h"
#include "auto.h"

struct Stack *Stack_create()
{
	struct Stack *stack = malloc(sizeof(struct Stack));
	if (stack == NULL) {
		fprintf(stderr, "Error allocating memory for Stack struct\n");
		exit(EXIT_FAILURE);
	}
	stack->len = 0;
	stack->max_len = 2;
	stack->stack = malloc(sizeof(char ) * (stack->max_len+1)); // Allow null terminator
	if (stack->stack == NULL) {
		fprintf(stderr, "Error allocating memory for string in Stack struct\n");
	}
	
	stack->stack[0] = '\0';
	return stack;
	
}

void Stack_push(struct Stack *stack, char symbol)
{
	stack->len++;
	if (stack->len > stack->max_len) {
		stack->max_len *= 2;
		stack->stack = realloc(stack->stack, sizeof(char ) * (stack->max_len+1));
		if (stack->stack == NULL) {
			fprintf(stderr, "Error reallocating memory for string in Stack struct\n");
			exit(EXIT_FAILURE);
		}
	}
	stack->stack[stack->len-1] = symbol;
	stack->stack[stack->len] = '\0';
}

char Stack_pop(struct Stack *stack)
{
	if (stack->len == 0) return '\0';
	char symbol = stack->stack[stack->len-1];
	stack->stack[stack->len-1] = '\0';
	stack->len--;
	return symbol;
}

char Stack_peek(struct Stack *stack)
{
	if (stack->len == 0) return '\0';
	else return stack->stack[stack->len-1];
}

struct Stack *Stack_copy(struct Stack *stack)
{
	struct Stack *new_stack = Stack_create();
	new_stack->len = stack->len;
	new_stack->max_len = stack->max_len;
	//new_stack->stack = strdup(stack->stack);
	new_stack->stack = realloc(new_stack->stack, sizeof (char) * (new_stack->max_len+1));
	if (new_stack->stack == NULL) {
		fprintf(stderr, "Error allocating memory for copy of stack\n");
		exit(EXIT_FAILURE);
	}
	memcpy(new_stack->stack, stack->stack, stack->max_len+1);
	
	return new_stack;
}

struct MultiStack *MultiStack_create(struct State *state)
{
	struct MultiStack *ms0 = malloc(sizeof(struct MultiStack));
	if (ms0 == NULL) {
		fprintf(stderr, "Error allocating memory for MultiStack\n");
		exit(EXIT_FAILURE);
	}
	ms0->state = state;
	ms0->len = 0;
	ms0->max_len = 2;
	ms0->stacks = malloc(sizeof(struct Stack *) * ms0->max_len);
	if (ms0->stacks == NULL) {
		fprintf(stderr, "Error allocating memory for Stack array in MultiStack\n");
		exit(EXIT_FAILURE);
	}
	
	return ms0;
}

void Stack_add(struct MultiStack *ms0, struct Stack *stack)
{
	ms0->len++;
	if (ms0->len > ms0->max_len) {
		ms0->max_len *= 2;
		ms0->stacks = realloc(ms0->stacks, sizeof(struct Stack *) * ms0->max_len);
		if (ms0->stacks == NULL) {
			fprintf(stderr, "Error reallocating memory for Stack array in MultiStack\n");
			exit(EXIT_FAILURE);
		}
	}
	ms0->stacks[ms0->len-1] = stack;
}



struct MultiStackList *MultiStackList_create()
{
	struct MultiStackList *msl0 = malloc(sizeof(struct MultiStackList));
	if (msl0 == NULL) {
		fprintf(stderr, "Error allocating memory for MultiStackList\n");
		exit(EXIT_FAILURE);
	}
	msl0->len = 0;
	msl0->max_len = 2;
	msl0->mstacks = malloc(sizeof(struct MultiStack *) * msl0->max_len);
	if (msl0->mstacks == NULL) {
		fprintf(stderr, "Error allocating memory for MultiStack array in MultiStackList\n");
		exit(EXIT_FAILURE);
	}
	
	return msl0;
}

void Stack_destroy(struct Stack *stack)
{
	free(stack->stack);
	free(stack);
}

void MultiStack_destroy(struct MultiStack *ms0)
{
	for (int i = 0; i < ms0->len; i++) {
		Stack_destroy(ms0->stacks[i]);
	}
	free(ms0->stacks);
	free(ms0);
}

void MultiStackList_destroy(struct MultiStackList *msl0)
{
	for (int i = 0; i < msl0->len; i++) {
		MultiStack_destroy(msl0->mstacks[i]);
	}
	free(msl0->mstacks);
	free(msl0);
}

void MultiStack_print(struct MultiStack *ms0)
{
	printf("%s: ", ms0->state->name);
	for (int i = 0; i < ms0->len; i++) {
		printf("%s ", ms0->stacks[i]->stack);
	}
	printf("\n");
}

void MultiStackList_print(struct MultiStackList *msl0)
{
	for (int i = 0; i < msl0->len; i++) {
		MultiStack_print(msl0->mstacks[i]);
	}
}

struct MultiStack *MultiStack_get(struct MultiStackList *msl0, struct State *state)
{
	for (int i = 0; i < msl0->len; i++) {
		if (msl0->mstacks[i]->state == state)
			return msl0->mstacks[i];
	}
	return NULL;
}

void MultiStack_add(struct MultiStackList *msl0, struct MultiStack *ms0)
{
	msl0->len++;
	if (msl0->len > msl0->max_len) {
		msl0->max_len *= 2;
		msl0->mstacks = realloc(msl0->mstacks, sizeof(struct MultiStack *) * msl0->max_len);
		if (msl0 == NULL) {
			fprintf(stderr, "Error reallocating memory for MultiStack array in MultiStackList\n");
			exit(EXIT_FAILURE);
		}
	}
	msl0->mstacks[msl0->len-1] = ms0;
}

void Stack_add_to(struct MultiStackList *msl0, struct State *s0, struct Stack *stack)
{
	struct MultiStack *ms0 = MultiStack_get(msl0, s0);
	
	if (ms0 == NULL) {
		ms0 = MultiStack_create(s0);
		MultiStack_add(msl0, ms0);
	}
	Stack_add(ms0, stack);
	
	
}

void MultiStack_push(struct MultiStackList *msl0, struct State *s0, char symbol)
{
	struct MultiStack *ms0 = NULL;
	for (int i = 0; i < msl0->len; i++) {
		if (msl0->mstacks[i]->state == s0)
			ms0 = msl0->mstacks[i];
	}
	
	// Create MultiStack if it doesn't exist
	if (ms0 == NULL) {
		ms0 = MultiStack_create(s0);
		struct Stack *new_stack = Stack_create();
		Stack_add(ms0, new_stack);
		MultiStack_add(msl0, ms0);
	}
	
	// Push symbol to all stacks within ms0
	for (int i = 0; i < ms0->len; i++) {
		Stack_push(ms0->stacks[i], symbol);
	}
}