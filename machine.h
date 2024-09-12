#ifndef MACHINE_H_
#define MACHINE_H_

#include "auto.h"
#include "stack.h"
#include "tape.h"

void epsilon_loop_detect(struct Automaton *a0);

// Nondeterministic functions
int NTM_run(struct Automaton *a0, struct Tape *input_tape);
int NTM_run_verbose(struct Automaton *a0, struct Tape *input_tape);
int NTM_stack_run(struct Automaton *a0, struct Tape *input_tape);
int NTM_stack_run_verbose(struct Automaton *a0, struct Tape *input_tape);
int PDA_run(struct Automaton *a0, struct Tape *input_tape);
int PDA_run_verbose(struct Automaton *a0, struct Tape *input_tape);
int NFA_run(struct Automaton *a0, struct Tape *input_tape);
int NFA_run_verbose(struct Automaton *a0, struct Tape *input_tape);

// Deterministic functions
int TM_run(struct Automaton *a0, struct Tape *input_tape);
int TM_run_verbose(struct Automaton *a0, struct Tape *input_tape);
int TM_stack_run(struct Automaton *a0, struct Tape *input_tape);
int TM_stack_run_verbose(struct Automaton *a0, struct Tape *input_tape);
int DPDA_run(struct Automaton *a0, struct Tape *input_tape);
int DPDA_run_verbose(struct Automaton *a0, struct Tape *input_tape);
int DFA_run(struct Automaton *a0, struct Tape *input_tape);
int DFA_run_verbose(struct Automaton *a0, struct Tape *input_tape);


#endif // MACHINE_H_

