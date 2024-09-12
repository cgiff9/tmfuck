#ifndef VAR_H_
#define VAR_H_

#include "stack.h"
#include "auto.h"

struct Var {
	struct Stack syms;
	struct Stack edits;
	char *name;
};

struct Var *Var_get(struct Automaton *a0, char *name);
struct Var *Var_add(struct Automaton *a0, char *name);
void Var_edit_add(struct Automaton *a0, struct Var *var, char *edit);
size_t Var_free(struct Var *var);

#endif // VAR_H_
