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
	stack->pos = 0;
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

int Stack_change_pos(struct Stack *stack, char direction)
{
	if (direction == 'L') {
		if (stack->pos > 0) { 
			stack->pos--;
		} else if (tm_bound != 'L') {
			stack->stack = realloc(stack->stack, sizeof(char) * stack->max_len  * 2 + 1);
			if (stack->stack == NULL) {
				fprintf(stderr, "Error reallocating memory for the left end of stack\n");
				exit(EXIT_FAILURE);
			}
			memmove(stack->stack+stack->max_len, stack->stack, stack->max_len+1);
			memset(stack->stack, tm_blank, stack->max_len);
			stack->len = stack->len + stack->max_len;
			stack->pos = stack->max_len-1;
			stack->max_len *= 2;
			
		} else if (tm_bound_halt) {
			return 1;
		}
	} else if (direction == 'R') {
		
		if (stack->pos < stack->len-1) {
			stack->pos++;
		} else if (stack->pos == stack->len-1) {
			if (tm_bound == 'R') { 
				if (tm_bound_halt) return 1;
			} else {
				stack->pos++;
				Stack_push(stack, tm_blank);
			}
		}
	}
	return 0;
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
	new_stack->pos = stack->pos;
	new_stack->len = stack->len;
	new_stack->max_len = stack->max_len;
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

int Stack_equiv(struct Stack *st0, struct Stack *st1)
{
	if (strcmp(st0->stack, st1->stack) == 0 && st0->pos == st1->pos) {
		return 1;
	} else return 0;
}

void Stack_add(struct MultiStack *ms0, struct Stack *stack)
{
	for (int i = 0; i < ms0->len; i++) {
		if (Stack_equiv(ms0->stacks[i], stack)) return;
	}
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

void Stack_print(struct Stack *stack)
{
	// does not print leading or trailing blanks
	int leading_pos = 0;
	int trailing_pos = stack->len- 1;
	for (int i = 0; i < stack->len && i < stack->pos; i++) {
		if (stack->stack[i] == tm_blank) leading_pos++;
		else break;
	}
	for (int i = stack->len-1; i > -1 && i > stack->pos; i--) {
		if (stack->stack[i] == tm_blank) trailing_pos--;
		else break;
	}
	for (int i = leading_pos; i <= trailing_pos; i++) {
		if (i == stack->pos)
			printf("[%c]", stack->stack[i]);
		else
			putchar(stack->stack[i]);
	}
	//putchar('\n');
}

void MultiStack_print(struct MultiStack *ms0)
{
	printf("%s: ", ms0->state->name);
	for (int i = 0; i < ms0->len; i++) {
		Stack_print(ms0->stacks[i]);
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
