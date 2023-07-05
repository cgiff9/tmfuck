#ifndef REGEX_H_
#define REGEX_H_

struct CharNode {
	char value;
	struct CharNode *next;
};

struct CharStack {
	int len;
	struct CharNode *top;
};

struct AutoNode {
	struct Automaton *automaton;
	struct AutoNode *next;
};

struct AutoStack {
	int len;
	struct AutoNode *top;
};

struct CharStack *CharStack_create();
struct CharNode *Node_create(char value);
void CharStack_push(struct CharStack *stack, char value);
char CharStack_pop(struct CharStack *stack);
char CharStack_peek(struct CharStack *stack);
void CharStack_destroy(struct CharStack *stack);
void CharStack_print(struct CharStack *stack);
void remove_spaces(char *s);
char *infix(char *regex);
int is_valid_regex(char *regex);

char *infix(char *regex);
struct Automaton *Automaton_concat(struct Automaton *a0, struct Automaton *a1);
struct Automaton *Automaton_union(struct Automaton *a0, struct Automaton *a1);
struct Automaton *Automaton_star(struct Automaton *a0);
struct Automaton *Automaton_plus(struct Automaton *a0);
struct Automaton *Automaton_char(char symbol);
struct AutoStack *AutoStack_create();
struct AutoNode *AutoNode_create(struct Automaton *a0);
void AutoStack_push(struct AutoStack *stack, struct Automaton *a0);
struct Automaton *AutoStack_pop(struct AutoStack *stack);
void AutoStack_destroy(struct AutoStack *stack);
struct Automaton *regex_to_nfa(char *regex);
struct Automaton *automaton_dup(struct Automaton *automaton);
#endif // REGEX_H_
