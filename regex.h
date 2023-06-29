#ifndef REGEX_H_
#define REGEX_H_

struct Node {
	char value;
	struct Node *next;
};

struct Stack {
	int len;
	struct Node *top;
};

struct AutoNode {
	struct Automaton *automaton;
	struct AutoNode *next;
};

struct AutoStack {
	int len;
	struct AutoNode *top;
};

struct Stack *stack_create();
struct Node *node_create(char value);
void stack_push(struct Stack *stack, char value);
char stack_pop(struct Stack *stack);
char stack_peek(struct Stack *stack);
void stack_destroy(struct Stack *stack);
void stack_print(struct Stack *stack);
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
