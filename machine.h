#ifndef MACHINE_H_
#define MACHINE_H_

#include "auto.h"
#include "stack.h"
#include "tape.h"

void set_marks(struct Automaton *a0);
void init_epsilon(struct Automaton *a0);
void e_closure_print(struct Automaton *a0, struct State *s0);
unsigned int e_closure(struct Automaton *a0, struct State *state_ptr, 
		struct Stack *current_states, struct Stack *next_states, struct Tape *tape, 
		int dstack, int *any_eps, int *loops_max,
		struct Stack *empty_stacks, struct Stack *empty_tapes);
unsigned int sym_closure(struct Automaton *a0, struct State *state, ptrdiff_t sym,
		struct Stack *stateptrs, struct Tape *tape, 
		struct Stack *empty_stacks, struct Stack *empty_tapes, int *any_syms, int *any_eps);
int Automaton_run_nd(struct Automaton *a0, struct Tape *input_tape);

int NTM_run(struct Automaton *a0, struct Tape *input_tape);
int NTM_run_verbose(struct Automaton *a0, struct Tape *input_tape);
int NTM_stack_run(struct Automaton *a0, struct Tape *input_tape);
int NTM_stack_run_verbose(struct Automaton *a0, struct Tape *input_tape);
int PDA_run(struct Automaton *a0, struct Tape *input_tape);
int PDA_run_verbose(struct Automaton *a0, struct Tape *input_tape);
int NFA_run(struct Automaton *a0, struct Tape *input_tape);
int NFA_run_verbose(struct Automaton *a0, struct Tape *input_tape);

int TM_run(struct Automaton *a0, struct Tape *input_tape);
int TM_run_verbose(struct Automaton *a0, struct Tape *input_tape);
int TM_stack_run(struct Automaton *a0, struct Tape *input_tape);
int TM_stack_run_verbose(struct Automaton *a0, struct Tape *input_tape);
int DFA_run(struct Automaton *a0, struct Tape *input_tape);
int DFA_run_verbose(struct Automaton *a0, struct Tape *input_tape);

int Automaton_run(struct Automaton *a0, struct Tape *input_tape);

#endif // MACHINE_H_

