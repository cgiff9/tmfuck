#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "auto.h"
#include "regex.h"

struct CharStack *CharStack_create()
{
	struct CharStack *new_stack = malloc(sizeof(struct CharStack));
	return new_stack;
}

struct CharNode *CharNode_create(char value)
{
	struct CharNode *new_node = malloc(sizeof(struct CharNode));
	new_node->value = value;
	new_node->next = NULL;
	return new_node;
}

void CharStack_push(struct CharStack *stack, char value)
{
	struct CharNode *new_top = CharNode_create(value);
	new_top->next = stack->top;
	stack->top = new_top;
	stack->len++;
}

char CharStack_pop(struct CharStack *stack)
{
	if (stack->top == NULL) return '\0';
	char value = stack->top->value;
	struct CharNode *new_top = stack->top->next;
	free(stack->top);
	stack->top = new_top;
	stack->len--;
	return value;
}

char CharStack_peek(struct CharStack *stack)
{
	if (stack->top == NULL) return '\0';
	else return stack->top->value;
}

void CharStack_destroy(struct CharStack *stack)
{
	char c = CharStack_pop(stack);
	while (c != '\0') c = CharStack_pop(stack);
	free(stack);
}

void CharStack_print(struct CharStack *stack)
{
	struct CharNode *n = stack->top;
	while (n != NULL) {
		printf("%c", n->value);
		n = n->next;
	}
	printf("\n");
}

void remove_spaces(char* s) {
	char* d = s;
	do {
		while (*d == ' ') {
			++d;
		}
	} while (*s++ = *d++);
}

int is_valid_regex(char *regex)
{
	int state = 1;
	int num_parens = 0;
	for (int i=0; regex[i] != '\0'; i++) {
		char c = regex[i];
		switch (state) {
			case 1:
				if (isalnum(c)) state = 2;
				//else if (c == '*' || c == '|') state = 0;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else if (c == ')') state = 0;
				else state = 0;
				break;
			case 2:
				if (isalnum(c)) state = 2;
				else if (c == '*' || c == '+') state = 3;
				else if (c == '|') state = 4;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 3:
				if (isalnum(c)) state = 2;
				//else if (c == '*') state = 0;
				else if (c == '|') state = 4;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 4:
				if (isalnum(c)) state = 2;
				//else if (c == '*' || c == '|') state = 0;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 5:
				if (isalnum(c)) state = 6;
				//else if (c == '*' || c == '|') state = 0;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 6:
				if (isalnum(c)) state = 6;
				else if (c  == '*' || c == '+') state = 7;
				else if (c == '|') state = 8;
				else if (c == ')') {
					state = 9;
					num_parens--;
				}
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 7:
				if (isalnum(c)) state = 6;
				//else if (c == '*') state = 0;
				else if (c == '|') state = 8;
				else if (c == ')') {
					state = 9;
					num_parens--;
				}
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 8:
				if (isalnum(c)) state = 6;
				//else if (c == '|' || c == '*' || c == ')') state = 0;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 9:
				if (isalnum(c)) state = 6;
				else if (c == '*' || c == '+') state = 7;
				else if (c == ')') {
					state = 9;
					num_parens--;
				} else if (c == '|') state = 8;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
		}
		if (state == 0) {
			return 0;
		}
	}
	if ((state == 1 || state == 2 || state == 3 || 
			state == 6 || state == 7 || state == 9) && num_parens == 0) return 1;
	else if (state != 0) {
		return 0;
	}

}

char *infix(char *regex)
{
	remove_spaces(regex);
	if (!is_valid_regex(regex)) {
		fprintf(stderr, "Invalid regex\n");
		exit(EXIT_FAILURE);
	}

	struct CharStack *stack = CharStack_create();
	char *tmp = malloc(sizeof(char) * strlen(regex)*2);
	*tmp = '\0';
	int concat_detect = 0;
	for (int i = 0; regex[i] != '\0'; i++) {
		if (concat_detect) {
			char peek = CharStack_peek(stack);
			while (peek == '*' || peek == '_' || peek == '+' && peek != '\0') {
				char c = CharStack_pop(stack);
				strncat(tmp, &c, 1);
				peek = CharStack_peek(stack);
			}
			CharStack_push(stack, '_');
			concat_detect = 0;
		}
		if (isalnum(regex[i]) && isalnum(regex[i+1]) || 
				regex[i] == '*' && isalnum(regex[i+1]) ||
				regex[i] == '+' && isalnum(regex[i+1]) ||
				regex[i] == '*' && regex[i+1] == '(' ||
				regex[i] == '+' && regex[i+1] == '(' ||
				regex[i] == ')' && regex[i+1] == '(' ||
				isalnum(regex[i]) && regex[i+1] == '(' ||
				regex[i] == ')' && isalnum(regex[i+1])) {
			concat_detect = 1;
		}
		
		if (isalnum(regex[i])) {
			strncat(tmp, &regex[i], 1);
		} else if (regex[i] == '*' || regex[i] == '+') {
			char peek = CharStack_peek(stack);
			while (peek == '*' || peek == '+' && peek != '\0') {
				char c = CharStack_pop(stack);
				strncat(tmp, &c, 1);
				peek = CharStack_peek(stack);
			}
			if (regex[i] == '*') 
				CharStack_push(stack, '*');
			else CharStack_push(stack, '+');
		} else if (regex[i] == '|') {
			char peek = CharStack_peek(stack);
			while (peek == '*' || peek == '+' || peek == '_' || peek == '|' && peek != '\0') {
				char c = CharStack_pop(stack);
				strncat(tmp, &c, 1);
				peek = CharStack_peek(stack);
			}
			CharStack_push(stack, '|');
		} else if (regex[i] == '(') { 
			CharStack_push(stack, '(');
		} else if (regex[i] == ')') {
			char peek = CharStack_peek(stack);
			while (peek != '(' && peek != '\0') {
				char c = CharStack_pop(stack);
				strncat(tmp, &c, 1);
				peek = CharStack_peek(stack);
			}
			if (peek == '(') CharStack_pop(stack);
			else if (peek == '\0') {
				fprintf(stderr, "Parentheses mismatch\n");
				exit(EXIT_FAILURE);
			}
		
		}
		//Stack_print(stack);
	}
	char peek = CharStack_peek(stack);
	while (peek != '\0') {
		if (peek == '(') {
			fprintf(stderr, "Parentheses mismatch\n");
			exit(EXIT_FAILURE);
		}

		char c = CharStack_pop(stack);
		strncat(tmp, &c, 1);
		peek = CharStack_peek(stack);
	}
	CharStack_destroy(stack);
	return tmp;
}

struct Automaton *Automaton_concat(struct Automaton *a0, struct Automaton *a1)
{
	//struct Automaton *a0 = automaton_dup(auto0);
	//struct Automaton *a1 = automaton_dup(auto1);
	struct Automaton *new_auto = Automaton_create();

	new_auto->start = a0->start;
	for (int i = 0; i < a0->len; i++) {
		if (a0->states[i]->final) {
			struct Transition *new_trans = Transition_create('\0', a1->start, '\0', '\0', '\0');
			a0->states[i]->final = 0;
			Transition_add(a0->states[i], new_trans);
		}
		State_add(new_auto, a0->states[i]);
	}
	
	for (int i = 0; i < a1->len; i++) {
		if (a1->states[i]->start) a1->states[i]->start = 0;
		State_add(new_auto, a1->states[i]);
	}

	for (int i = 0; i < new_auto->len; i++) {
		char name[STATE_NAME_MAX];
		snprintf(name, STATE_NAME_MAX, "q%d", i);
		free(new_auto->states[i]->name);
		new_auto->states[i]->name = strdup(name);
	}
	
	Automaton_clear(a0);
	Automaton_clear(a1);
	
	return new_auto;
}

struct Automaton *Automaton_union(struct Automaton *a0, struct Automaton *a1)
{
	//struct Automaton *a0 = automaton_dup(auto0);
	//struct Automaton *a1 = automaton_dup(auto1);
	struct Automaton *new_auto = Automaton_create();

	struct State *new_start = State_create("q0");
	struct Transition *new_trans0 = Transition_create('\0', a0->start, '\0', '\0', '\0');
	struct Transition *new_trans1 = Transition_create('\0', a1->start, '\0', '\0', '\0');
	Transition_add(new_start, new_trans0);
	Transition_add(new_start, new_trans1);
	new_start->start = 1;
	State_add(new_auto, new_start);
	new_auto->start = new_start;

	for (int i = 0; i < a0->len; i++) {
		State_add(new_auto, a0->states[i]);
		if (a0->states[i]->start) a0->states[i]->start = 0;
	}
	for (int i = 0; i < a1->len; i++) {
		State_add(new_auto, a1->states[i]);
		if (a1->states[i]->start) a1->states[i]->start = 0;
	}

	for (int i = 1; i < new_auto->len; i++) {
		char name[STATE_NAME_MAX];
		snprintf(name, STATE_NAME_MAX, "q%d", i);
		free(new_auto->states[i]->name);
		new_auto->states[i]->name = strdup(name);
	}

	Automaton_clear(a0);
	Automaton_clear(a1);
	
	return new_auto;
}

struct Automaton *Automaton_star(struct Automaton *a0)
{
	//struct Automaton *a0 = automaton_dup(auto0);
	struct Automaton *new_auto = Automaton_create();

	struct State *new_start = State_create("q0");
	new_start->start = 1;
	new_start->final = 1;
	
	struct Transition *new_trans = Transition_create('\0', a0->start, '\0', '\0', '\0');
	Transition_add(new_start, new_trans);
	State_add(new_auto, new_start);
	new_auto->start = new_start;

	for (int i = 0; i < a0->len; i++) {
		if (a0->states[i]->final) {
			new_trans = Transition_create('\0', a0->start, '\0', '\0', '\0');
			Transition_add(a0->states[i], new_trans);
		}
		if (a0->states[i]->start) a0->states[i]->start = 0;

		char name[STATE_NAME_MAX];
		snprintf(name, STATE_NAME_MAX, "q%d", i+1);
		free(a0->states[i]->name);
		a0->states[i]->name = strdup(name);
		State_add(new_auto, a0->states[i]);
	}

	Automaton_clear(a0);
	return new_auto;
}

struct Automaton *Automaton_plus(struct Automaton *a0)
{
	struct Automaton *new_auto = Automaton_create();
	new_auto->start = a0->start;
	for (int i = 0; i < a0->len; i++) {
		if (a0->states[i]->final) {
			struct Transition *new_trans = Transition_create('\0', a0->start, '\0', '\0', '\0');
			Transition_add(a0->states[i], new_trans);
		}
		State_add(new_auto, a0->states[i]);
	}
	
	Automaton_clear(a0);
	
	return new_auto;
}

struct Automaton *Automaton_char(char symbol)
{
	struct Automaton *a0 = Automaton_create();
	struct State *q0 = State_create("q0");
	struct State *q1 = State_create("q1");
	
	q0->start = 1;
	q1->final = 1;
	struct Transition *trans = Transition_create(symbol, q1, '\0', '\0', '\0');
	Transition_add(q0, trans);
	
	State_add(a0, q0);
	State_add(a0, q1);
	
	a0->start = q0;
	return a0;
}

struct AutoStack *AutoStack_create()
{
	struct AutoStack *as0 = malloc(sizeof(struct AutoStack));
	if (as0 == NULL) {
		fprintf(stderr, "Error allocating memory for AutoStack\n");
		exit(EXIT_FAILURE);
	}
	as0->len = 0;
	return as0;
}

struct AutoNode *AutoNode_create(struct Automaton *a0)
{
	struct AutoNode *n0 = malloc(sizeof(struct AutoNode));
	if (n0 == NULL ) {
		fprintf(stderr, "Error allocating memory for AutoNode\n");
		exit(EXIT_FAILURE);
	}
	n0->automaton = a0;
	n0->next = NULL;
	return n0;
}

void AutoCharStack_push(struct AutoStack *stack, struct Automaton *a0)
{
	struct AutoNode *new_top = AutoNode_create(a0);
	new_top->next = stack->top;
	stack->top = new_top;
	stack->len++;
}

struct Automaton *AutoStack_pop(struct AutoStack *stack)
{
	if (stack->top == NULL) return NULL;
	struct Automaton *a0 = stack->top->automaton;
	struct AutoNode *new_top = stack->top->next;
	free(stack->top);
	stack->top = new_top;
	stack->len--;
	return a0;
}

void AutoStack_destroy(struct AutoStack *stack)
{
	struct Automaton *a0 = AutoStack_pop(stack);
	while(a0 != NULL) AutoStack_pop(stack);
	free(stack);
}

struct Automaton *regex_to_nfa(char *regex)
{
	char *regex_infix = infix(regex);
	
	// Regex = empty string;
	if (regex_infix[0] == '\0') {
		struct Automaton *a0 = Automaton_create();
		State_name_add(a0, "q0");
		a0->states[0]->start = 1;
		a0->states[0]->final = 1;
		a0->start = a0->states[0];
		return a0;
	}

	struct AutoStack *stack = AutoStack_create();
	for (int i=0; regex_infix[i] != '\0'; i++) {
		if (isalnum(regex_infix[i])) {
			struct Automaton *a0 = Automaton_char(regex_infix[i]);
			AutoCharStack_push(stack, a0);
		} else if (regex_infix[i] == '*') {
			struct Automaton *top = AutoStack_pop(stack);
			struct Automaton *new_top = Automaton_star(top);
			AutoCharStack_push(stack, new_top);	
		} else if (regex_infix[i] == '+') {
			struct Automaton *top = AutoStack_pop(stack);
			struct Automaton *new_top = Automaton_plus(top);
			AutoCharStack_push(stack, new_top);
		} else if (regex_infix[i] == '|') {
			struct Automaton *top = AutoStack_pop(stack);
			struct Automaton *next = AutoStack_pop(stack);
			struct Automaton *new_top = Automaton_union(next, top);
			AutoCharStack_push(stack, new_top);
		} else if (regex_infix[i] == '_') {
			struct Automaton *top = AutoStack_pop(stack);
			struct Automaton *next = AutoStack_pop(stack);
			struct Automaton *new_top = Automaton_concat(next, top);
			AutoCharStack_push(stack, new_top);
		}
	}
	struct Automaton *a0 = AutoStack_pop(stack);
	AutoStack_destroy(stack);
	free(regex_infix);
	return a0;
}