#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>

#include <unistd.h>

#include "machine.h"
#include "auto.h"
#include "stack.h"
#include "file.h"
#include "tape.h"

size_t run_memory = 0;

void clear_n(int n)
{
	if (!n) { 
		return;
	}
	printf("\033[%dA\033[J", n);
}

// Convert double in seconds into struct timespec for use with nanosleep()
// > nanosecond resolution (lowest value 0.000000001)
//   --> likely practical lowest value more like 0.000001 (microseconds)
// > whole seconds stored in req.tv_sec
// > nanoseconds stored in req.tv_nsec
//
// Returns struct timespec
struct timespec nsleep_calc(double seconds)
{
	double fraction = seconds - ((long)seconds);
	long nanoseconds = (seconds+(long)fraction*1000000000) * 1000000000;
	//struct timespec req, rem;
	struct timespec req;
	if(nanoseconds > 999999999) {   
		req.tv_sec = (long)(nanoseconds / 1000000000); 
		req.tv_nsec = (nanoseconds - ((long)req.tv_sec * 1000000000));// * 1000; //* 1000000; 
	} else {   
		req.tv_sec = 0;
		req.tv_nsec = nanoseconds;// * 1000; //1000000;
	} 
	return req;	
	//return nanosleep(&req , &rem);
}

// Mark any epsilon loops (cycles) in an automaton
// > Detects number of unique cycles within a state's
//   epsilon closure.
// > Loops are not marked for the e-closure of 
//   every state within a loop; otherwise to do so would 
//   be a meaningless distinction; Loops are only marked 
//   within the e-closures of states with a minimal 
//   distance (height) from the start state
//
//   Returns void (automaton whose transitions properly
//   reflect the prescence of these epsilon loops)
void epsilon_loop_detect(struct Automaton *a0)
{
	struct Stack sym_states = Stack_init(STATEPTR);
	struct Stack e_states = Stack_init(STATEPTR);
	struct Stack e_trans = Stack_init(TRANSPTR);
	
	Stack_push(&sym_states, &a0->start);

	struct State *sym_top = NULL;
	while((sym_top = Stack_pop(&sym_states))) {

		if (sym_top->syms) {
			for (unsigned int i = 0; i < a0->alphabet.size; i++) {
				CELL_TYPE *input_sym = Stack_get(&a0->alphabet, i);
				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, sym_top, *input_sym, &offset, 0);	
				while (trans_ptr != NULL) {
					if (!trans_ptr->epsilon_mark) {
						trans_ptr->epsilon_mark = 1;
						Stack_push(&sym_states, &trans_ptr->dstate);
					}

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, sym_top, *input_sym, &offset, 0);
					else 
						break;
				}
			}
		}

		if (sym_top->epsilon) Stack_push(&e_states, &sym_top);

		struct State *e_top = NULL;
		while ((e_top = Stack_pop(&e_states))) {
			//printf("checking e-closure for state %s\n", e_top->name);

			XXH64_hash_t offset = a0->trans.hashmax;
			struct Trans *trans_ptr = Trans_get(a0, e_top, CELL_MAX, &offset, 1);
			while (trans_ptr != NULL) {
				if (!trans_ptr->epsilon_mark) {
					trans_ptr->epsilon_mark = 1;
					Stack_push(&e_states, &trans_ptr->dstate);
					Stack_push(&e_trans, &trans_ptr);
				}

				if (trans_ptr->strans) 
					trans_ptr = Trans_get(a0, e_top, CELL_MAX, &offset, 1);
				else 
					break;
			}
		}

		for (unsigned int i = 0; i < e_trans.size; i++) {
			struct Trans *t0 = Stack_get(&e_trans, i);
			if (t0->dstate->epsilon_mark) continue;
			for (unsigned int j = i; j < e_trans.size; j++) {
				struct Trans *t1 = Stack_get(&e_trans, j);
				if (t1->pstate->epsilon_mark) continue;
				if (t0->pstate == t1->dstate || t1->pstate == t1->dstate) {
					//printf("t0->pstate: %s, t1->dstate: %s loop set\n", 
							//t0->pstate->name, t1->dstate->name);
					t1->epsilon_loop = 1;
					a0->epsilon_loops = 1;
				}
			}
			t0->dstate->epsilon_mark = 1;
		}

		Stack_clear(&e_states);
		Stack_clear(&e_trans);
	}

	Stack_free(&sym_states);
	Stack_free(&e_states);
	Stack_free(&e_trans);
}

// ==========================
// NONDETERMINISTIC ENGINES
// ==========================
// 
//

// Simulates nondeterministic Turing machine
//
// > Rather than dealing with a single state and tape
//   at a time, this function uses runtime stacks to keep 
//   track of nondeterminstic branches.
// > Note: Only one branch need accept for the machine to
//   accept, but EVERY branch must reject for the machine
//   to reject.
// > The current_* stacks do the heavy lifting, while
//   the next_* stacks prevent 'epsilon loops' from
//   sending the machine down a depth-first rabbit hole.
// 
// > A stack of empty tapes is also used to 'recycle'
//   allocated tape objects as they reject or become unused
//   throughout the nondeterministic branching.
//
int NTM_run(struct Automaton *a0, struct Tape *input_tape)
{
	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_tapes = Stack_init(TAPEPTR);
	struct Stack next_tapes = Stack_init(TAPEPTR);

	struct Stack empty_tapes = Stack_init(TAPEPTR);
	
	Stack_push(&current_states, &a0->start);
	Stack_push(&current_tapes, &input_tape);

	struct State *state_ptr = NULL;
	struct Tape *tape_ptr = input_tape;

	size_t steps = 0;

	while(1) {

		while(1) {

			if (!current_states.size) {
				state_ptr = NULL;
				break;
			}

			state_ptr = Stack_pop(&current_states);
			tape_ptr = Stack_pop(&current_tapes);

			if (state_ptr->final) {
				goto run_end;
			}

			// For NFA,PDA: If tape end reached, only allow epsilon transitions
			if (state_ptr->syms) {
				CELL_TYPE input_sym = Tape_head(tape_ptr);

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);	

				while (trans_ptr) {


					if (!trans_ptr->dstate->reject) {
						struct Tape *tape_new = tape_ptr;
						
						tape_new = Stack_pop(&empty_tapes);
						if (!tape_new) tape_new = Tape_add(a0);
						Tape_copy(tape_new, tape_ptr);

						if (trans_ptr->write) Tape_write(tape_new, &trans_ptr->writesym);
						Tape_pos(tape_new, trans_ptr->dir);

						Stack_push(&current_states, &trans_ptr->dstate);
						Stack_push(&current_tapes, &tape_new);
					}

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);
					else 
						break;
				}
			}

			if (state_ptr->epsilon) {

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);	

				while (trans_ptr) {

					if (!trans_ptr->dstate->reject) {
						struct Tape *tape_new = tape_ptr;
						
						tape_new = Stack_pop(&empty_tapes);
						if (!tape_new) tape_new = Tape_add(a0);				
						Tape_copy(tape_new, tape_ptr);

						if (trans_ptr->write) Tape_write(tape_new, &trans_ptr->writesym);
						Tape_pos(tape_new, trans_ptr->dir);

						if (trans_ptr->epsilon_loop) {
							Stack_push(&next_states, &trans_ptr->dstate);
							Stack_push(&next_tapes, &tape_new);
						} else {
							Stack_push(&current_states, &trans_ptr->dstate);
							Stack_push(&current_tapes, &tape_new);
						}
					}

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);
					else 
						break;
				}
			} 

			Tape_clear(tape_ptr);
			Stack_push(&empty_tapes, &tape_ptr);

			steps++;
			state_ptr = NULL;
		}

		if (!next_states.size) {
			state_ptr = NULL;
			break;
		}

		struct Stack swap = current_states;
		current_states = next_states;
		next_states = swap;

		swap = current_tapes;
		current_tapes = next_tapes;
		next_tapes = swap;

		state_ptr = NULL;
	}

run_end:

	run_memory += 
		Stack_free(&current_states) +
		Stack_free(&next_states) +
		
		Stack_free(&current_tapes) +
		Stack_free(&next_tapes) +
		Stack_free(&empty_tapes);

	printf("STEPS: %zu\n", steps);

	return (state_ptr && state_ptr->final);
}

// Verbose version of NTM_run()
//
int NTM_run_verbose(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}

	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_tapes = Stack_init(TAPEPTR);
	struct Stack next_tapes = Stack_init(TAPEPTR);

	struct Stack empty_tapes = Stack_init(TAPEPTR);
	
	Stack_push(&current_states, &a0->start);
	Stack_push(&current_tapes, &input_tape);

	struct State *state_ptr = NULL;
	struct Tape *tape_ptr = input_tape;

	int newlines = 0;
	size_t steps = 0;

	while(1) {

		while(1) {

			if (!current_states.size) {
				state_ptr = NULL;
				break;
			}

			state_ptr = Stack_pop(&current_states);
			tape_ptr = Stack_pop(&current_tapes);

			if (state_ptr->final) {
				goto run_end;
			}

			// For NFA,PDA: If tape end reached, only allow epsilon transitions
			if (state_ptr->syms) {
				CELL_TYPE input_sym = Tape_head(tape_ptr);

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);	

				while (trans_ptr) {

					struct Tape *tape_new = tape_ptr;

					tape_new = Stack_pop(&empty_tapes);
					if (!tape_new) tape_new = Tape_add(a0);
					Tape_copy(tape_new, tape_ptr);

					if (trans_ptr->write) Tape_write(tape_new, &trans_ptr->writesym);
					Tape_pos(tape_new, trans_ptr->dir);

					if (!trans_ptr->dstate->reject) {
						Stack_push(&current_states, &trans_ptr->dstate);
						Stack_push(&current_tapes, &tape_new);
					}

					Trans_print(trans_ptr);
					printf("\n%*s tape:  ", longest_name, trans_ptr->dstate->name);
					Tape_print(tape_new);
					putchar('\n');
					newlines+=2;

					// reclaim rejected new tape
					if (trans_ptr->dstate->reject) {
						Tape_clear(tape_new);
						Stack_push(&empty_tapes, &tape_new);
					}	

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);
					else 
						break;
				}

				if (offset == a0->trans.hashmax) {
					printf("%*s: ", longest_name, state_ptr->name);
					Trans_sym_run_print(input_sym);
					printf(" >\n%*s tape:  ", longest_name, state_ptr->name);
					Tape_print(tape_ptr);
					putchar('\n');
					newlines+=2;
				}
			}

			if (state_ptr->epsilon) {

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);	

				while (trans_ptr) {

					struct Tape *tape_new = tape_ptr;

					tape_new = Stack_pop(&empty_tapes);
					if (!tape_new) tape_new = Tape_add(a0);				
					Tape_copy(tape_new, tape_ptr);

					if (trans_ptr->write) Tape_write(tape_new, &trans_ptr->writesym);
					Tape_pos(tape_new, trans_ptr->dir);

					if (!trans_ptr->dstate->reject) {
						if (trans_ptr->epsilon_loop) {
							Stack_push(&next_states, &trans_ptr->dstate);
							Stack_push(&next_tapes, &tape_new);
						} else {
							Stack_push(&current_states, &trans_ptr->dstate);
							Stack_push(&current_tapes, &tape_new);
						}
					}

					Trans_print(trans_ptr);
					printf("\n%*s tape:  ", longest_name, trans_ptr->dstate->name);
					Tape_print(tape_new);
					putchar('\n');
					newlines+=2;

					// reclaim rejected new stack/tape
					if (trans_ptr->dstate->reject) {
						Tape_clear(tape_new);
						Stack_push(&empty_tapes, &tape_new);
					}

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);
					else 
						break;
				}
			} 

			Tape_clear(tape_ptr);
			Stack_push(&empty_tapes, &tape_ptr);

			if (flag_verbose && newlines) {
				if (debug >= 2) {
					printf("current_states.size: %u\n", current_states.size);
					printf("current_tapes.size: %u\n", current_tapes.size);
					printf("empty_tapes.size: %u\n", empty_tapes.size);
					printf("a0->tapes.size: %u\n", a0->tapes.size);
					newlines+=4;
				}

				if (delay) nanosleep(&req, &rem);

				if (!verbose_inline) printf("-----------\n");
				else {
					clear_n(newlines);
				}
				newlines = 0;
			} else if (newlines && delay) nanosleep(&req, &rem);

			steps++;
			state_ptr = NULL;
		}

		if (!next_states.size) {
			state_ptr = NULL;
			break;
		}

		// Print epsilon-cycling next tapes/stacks
		if (flag_verbose > 1) {
			int max_rows = 20;
			int num_pages = ((next_states.size * ((is_pda) ? 2 : 1)) / max_rows);
			num_pages = (((next_states.size + num_pages) * ((is_pda) ? 2 : 1)) / max_rows)
				+ ((((next_states.size + num_pages) * ((is_pda) ? 2 : 1)) % max_rows != 0));
			int current_page = 1;

			if (verbose_inline)
				printf("epsilon cycles (1/%u):\n", num_pages);
			else
				printf("epsilon cycles:\n");

			newlines++;

			for (unsigned int i = 0; i < next_states.size; i++) {
				struct State *s0 = Stack_get(&next_states, i);
				struct Tape *tp0 = Stack_get(&next_tapes, i);

				printf("%s tape:  ", s0->name); 
				Tape_print(tp0); 
				putchar('\n');
				newlines++;
				
				if (verbose_inline && newlines >= max_rows) {
					if (delay) nanosleep(&req, &rem);
					clear_n(newlines);
					printf("epsilon cycles (%u/%u):\n", ++current_page, num_pages);
					newlines = 1;
				}
			}

			if (delay) nanosleep(&req, &rem);

			if (verbose_inline) {
				clear_n(newlines);
			} else {
				printf("-----------\n");
			}
			newlines = 0;
		}

		struct Stack swap = current_states;
		current_states = next_states;
		next_states = swap;

		swap = current_tapes;
		current_tapes = next_tapes;
		next_tapes = swap;

		state_ptr = NULL;
	}

run_end:

	run_memory += 
		Stack_free(&current_states) +
		Stack_free(&next_states) +
		
		Stack_free(&current_tapes) +
		Stack_free(&next_tapes) +
		Stack_free(&empty_tapes);

	printf("STEPS: %zu\n", steps);

	return (state_ptr && state_ptr->final);
}

// Identical to NTM_run, except:
//
// > Performs checks on pop/push symbols
// > Enforces stack top match for pop transition to 
//   proceed to destination state
// > Has additional runtime stack to keep track
//   of nondeterminstic stack branches (stack of stacks o_0)
// 
// > Has an additional stack of empty stacks, for the same
//   purpose of recycling rejected/unused stacks throughout
//   the nondeterministic branching.
//
int NTM_stack_run(struct Automaton *a0, struct Tape *input_tape)
{
	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_tapes = Stack_init(TAPEPTR);
	struct Stack next_tapes = Stack_init(TAPEPTR);
	struct Stack current_stacks = Stack_init(STACKPTR);
	struct Stack next_stacks = Stack_init(STACKPTR);

	struct Stack empty_tapes = Stack_init(TAPEPTR);
	struct Stack empty_stacks = Stack_init(STACKPTR);
	
	Stack_push(&current_states, &a0->start);
	Stack_push(&current_tapes, &input_tape);

	struct Stack *first_stack = Stack_add(a0, CELL);
	Stack_push(&current_stacks, &first_stack);

	struct State *state_ptr = NULL;
	struct Tape *tape_ptr = input_tape;
	struct Stack *stack_ptr = NULL;

	size_t steps = 0;

	while(1) {

		while(1) {

			if (!current_states.size) {
				state_ptr = NULL;
				break;
			}

			state_ptr = Stack_pop(&current_states);
			tape_ptr = Stack_pop(&current_tapes);
			stack_ptr = Stack_pop(&current_stacks);

			if (state_ptr->final) {
				goto run_end;
			}

			// For NFA,PDA: If tape end reached, only allow epsilon transitions
			if (state_ptr->syms) {
				CELL_TYPE input_sym = Tape_head(tape_ptr);

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);	

				while (trans_ptr) {

					struct Stack *stack_new = stack_ptr;
					struct Tape *tape_new = tape_ptr;

					stack_new = Stack_pop(&empty_stacks);	
					if (!stack_new) stack_new = Stack_add(a0, CELL);
					Stack_copy(stack_new, stack_ptr);

					int pda = 1;
					if (trans_ptr->pop) {
						CELL_TYPE *peek = NULL;
						if ((pda = (peek = Stack_peek(stack_new)) && *peek == trans_ptr->popsym)) {	
							Stack_pop(stack_new);
							if (trans_ptr->push) {
								Stack_push(stack_new, &trans_ptr->pushsym);
							}
						}
					} else if (trans_ptr->push) {
						Stack_push(stack_new, &trans_ptr->pushsym);
					}

					if (pda && !trans_ptr->dstate->reject) {
						tape_new = Stack_pop(&empty_tapes);
						if (!tape_new) tape_new = Tape_add(a0);
						Tape_copy(tape_new, tape_ptr);

						if (trans_ptr->write) Tape_write(tape_new, &trans_ptr->writesym);
						Tape_pos(tape_new, trans_ptr->dir);

						if (!trans_ptr->dstate->reject) {
							Stack_push(&current_states, &trans_ptr->dstate);
							Stack_push(&current_tapes, &tape_new);
							Stack_push(&current_stacks, &stack_new);
						}
					} else {
						// reclaim rejected new stack/tape
						Stack_clear(stack_new);
						Stack_push(&empty_stacks, &stack_new);
					}	

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);
					else 
						break;
				}
			}

			if (state_ptr->epsilon) {

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);	
				
				while (trans_ptr) {

					struct Stack *stack_new = stack_ptr;
					struct Tape *tape_new = tape_ptr;

					stack_new = Stack_pop(&empty_stacks);	
					if (!stack_new) stack_new = Stack_add(a0, CELL);
					Stack_copy(stack_new, stack_ptr);

					int pda = 1;
					if (trans_ptr->pop) {
						CELL_TYPE *peek = NULL;
						if ((pda = (peek = Stack_peek(stack_new)) && *peek == trans_ptr->popsym)) {	
							Stack_pop(stack_new);
							if (trans_ptr->push) {
								Stack_push(stack_new, &trans_ptr->pushsym);
							}
						}
					} else if (trans_ptr->push) {
						Stack_push(stack_new, &trans_ptr->pushsym);
					}

					if (pda && !trans_ptr->dstate->reject) {

						tape_new = Stack_pop(&empty_tapes);
						if (!tape_new) tape_new = Tape_add(a0);				
						Tape_copy(tape_new, tape_ptr);

						if (trans_ptr->write) Tape_write(tape_new, &trans_ptr->writesym);
						Tape_pos(tape_new, trans_ptr->dir);

						if (trans_ptr->epsilon_loop) {
							Stack_push(&next_states, &trans_ptr->dstate);
							Stack_push(&next_tapes, &tape_new);
							Stack_push(&next_stacks, &stack_new);
						} else {
							Stack_push(&current_states, &trans_ptr->dstate);
							Stack_push(&current_tapes, &tape_new);
							Stack_push(&current_stacks, &stack_new);
						}
					} else {
						// reclaim rejected new stack/tape
						Stack_clear(stack_new);
						Stack_push(&empty_stacks, &stack_new);
					}

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);
					else 
						break;
				}
			} 

			Tape_clear(tape_ptr);
			Stack_push(&empty_tapes, &tape_ptr);
			Stack_clear(stack_ptr);
			Stack_push(&empty_stacks, &stack_ptr);

			steps++;
			state_ptr = NULL;
		}

		if (!next_states.size) {
			state_ptr = NULL;
			break;
		}

		struct Stack swap = current_states;
		current_states = next_states;
		next_states = swap;

		swap = current_tapes;
		current_tapes = next_tapes;
		next_tapes = swap;

		swap = current_stacks;
		current_stacks = next_stacks;
		next_stacks = swap;

		state_ptr = NULL;
	}

run_end:

	run_memory += 
		Stack_free(&current_states) +
		Stack_free(&next_states) +
		
		Stack_free(&current_tapes) +
		Stack_free(&next_tapes) +
		Stack_free(&empty_tapes) +

		Stack_free(&current_stacks) +
		Stack_free(&next_stacks) +
		Stack_free(&empty_stacks);

	printf("STEPS: %zu\n", steps);

	return (state_ptr && state_ptr->final);
}

// Verbose version of NTM_stack_run()
//
int NTM_stack_run_verbose(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}

	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_tapes = Stack_init(TAPEPTR);
	struct Stack next_tapes = Stack_init(TAPEPTR);
	struct Stack current_stacks = Stack_init(STACKPTR);
	struct Stack next_stacks = Stack_init(STACKPTR);

	struct Stack empty_tapes = Stack_init(TAPEPTR);
	struct Stack empty_stacks = Stack_init(STACKPTR);
	
	Stack_push(&current_states, &a0->start);
	Stack_push(&current_tapes, &input_tape);

	struct Stack *first_stack = Stack_add(a0, CELL);
	Stack_push(&current_stacks, &first_stack);

	struct State *state_ptr = NULL;
	struct Tape *tape_ptr = input_tape;
	struct Stack *stack_ptr = NULL;

	int newlines = 0;
	size_t steps = 0;

	while(1) {

		while(1) {

			if (!current_states.size) {
				state_ptr = NULL;
				break;
			}

			state_ptr = Stack_pop(&current_states);
			tape_ptr = Stack_pop(&current_tapes);
			stack_ptr = Stack_pop(&current_stacks);

			if (state_ptr->final) {
				goto run_end;
			}

			// For NFA,PDA: If tape end reached, only allow epsilon transitions
			if (state_ptr->syms) {
				CELL_TYPE input_sym = Tape_head(tape_ptr);

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);	

				while (trans_ptr) {

					struct Stack *stack_new = stack_ptr;
					struct Tape *tape_new = tape_ptr;

					stack_new = Stack_pop(&empty_stacks);	
					if (!stack_new) stack_new = Stack_add(a0, CELL);
					Stack_copy(stack_new, stack_ptr);

					int pda = 1;
					if (trans_ptr->pop) {
						CELL_TYPE *peek = NULL;
						if ((pda = (peek = Stack_peek(stack_new)) && *peek == trans_ptr->popsym)) {	
							Stack_pop(stack_new);
							if (trans_ptr->push) {
								Stack_push(stack_new, &trans_ptr->pushsym);
							}
						}
					} else if (trans_ptr->push) {
						Stack_push(stack_new, &trans_ptr->pushsym);
					}

					if (pda) {
						tape_new = Stack_pop(&empty_tapes);
						if (!tape_new) tape_new = Tape_add(a0);
						Tape_copy(tape_new, tape_ptr);

						if (trans_ptr->write) Tape_write(tape_new, &trans_ptr->writesym);
						Tape_pos(tape_new, trans_ptr->dir);

						if (!trans_ptr->dstate->reject) {
							Stack_push(&current_states, &trans_ptr->dstate);
							Stack_push(&current_tapes, &tape_new);
							Stack_push(&current_stacks, &stack_new);
						}
					} 

					Trans_print(trans_ptr);
					if (!pda) printf(" (pop fail)");
					printf("\n%*s tape:  ", longest_name, trans_ptr->dstate->name);
					Tape_print(tape_new);
					printf("\n%*s stack: ", longest_name, trans_ptr->dstate->name);
					Stack_print(stack_new);
					putchar('\n');
					newlines+=3;

					// reclaim rejected new stack/tape
					if (!pda || trans_ptr->dstate->reject) {
						Tape_clear(tape_new);
						Stack_push(&empty_tapes, &tape_new);
						Stack_clear(stack_new);
						Stack_push(&empty_stacks, &stack_new);
					}	

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);
					else 
						break;
				}

				if (offset == a0->trans.hashmax) {
					printf("%*s: ", longest_name, state_ptr->name);
					Trans_sym_run_print(input_sym);
					printf(" >\n%*s tape:  ", longest_name, state_ptr->name);
					Tape_print(tape_ptr);
					printf("\n%*s stack: ", longest_name, state_ptr->name);
					Stack_print(stack_ptr);
					putchar('\n');
					newlines+=3;
				}
				
			}

			if (state_ptr->epsilon) {

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);	

				while (trans_ptr) {

					struct Stack *stack_new = stack_ptr;
					struct Tape *tape_new = tape_ptr;

					stack_new = Stack_pop(&empty_stacks);	
					if (!stack_new) stack_new = Stack_add(a0, CELL);
					Stack_copy(stack_new, stack_ptr);

					int pda = 1;
					if (trans_ptr->pop) {
						CELL_TYPE *peek = NULL;
						if ((pda = (peek = Stack_peek(stack_new)) && *peek == trans_ptr->popsym)) {	
							Stack_pop(stack_new);
							if (trans_ptr->push) {
								Stack_push(stack_new, &trans_ptr->pushsym);
							}
						}
					} else if (trans_ptr->push) {
						Stack_push(stack_new, &trans_ptr->pushsym);
					}

					if (pda) {

						tape_new = Stack_pop(&empty_tapes);
						if (!tape_new) tape_new = Tape_add(a0);				
						Tape_copy(tape_new, tape_ptr);

						if (trans_ptr->write) Tape_write(tape_new, &trans_ptr->writesym);
						Tape_pos(tape_new, trans_ptr->dir);

						if (!trans_ptr->dstate->reject) {
							if (trans_ptr->epsilon_loop) {
								Stack_push(&next_states, &trans_ptr->dstate);
								Stack_push(&next_tapes, &tape_new);
								Stack_push(&next_stacks, &stack_new);
							} else {
								Stack_push(&current_states, &trans_ptr->dstate);
								Stack_push(&current_tapes, &tape_new);
								Stack_push(&current_stacks, &stack_new);
							}
						}
					}

					Trans_print(trans_ptr);
					printf("\n%*s tape:  ", longest_name, trans_ptr->dstate->name);
					Tape_print(tape_new);
					printf("\n%*s stack: ", longest_name, trans_ptr->dstate->name);
					Stack_print(stack_new);
					putchar('\n');
					newlines+=3;

					// reclaim rejected new stack/tape
					if (!pda || trans_ptr->dstate->reject) {
						Tape_clear(tape_new);
						Stack_push(&empty_tapes, &tape_new);
						Stack_clear(stack_new);
						Stack_push(&empty_stacks, &stack_new);
					}

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);
					else 
						break;
				}
			} 

			Tape_clear(tape_ptr);
			Stack_push(&empty_tapes, &tape_ptr);
			Stack_clear(stack_ptr);
			Stack_push(&empty_stacks, &stack_ptr);

			if (flag_verbose && newlines) {
				if (debug >= 2) {
					printf("current_states.size: %u\n", current_states.size);
					printf("current_tapes.size: %u\n", current_tapes.size);
					printf("current_stacks.size: %u\n", current_stacks.size);
					printf("empty_tapes.size: %u\n", empty_tapes.size);
					printf("empty_stacks.size: %u\n", empty_stacks.size);
					printf("a0->tapes.size: %u\n", a0->tapes.size);
					printf("a0->stacks.size: %u\n", a0->stacks.size);
					newlines+=7;
				}

				if (delay) nanosleep(&req, &rem);

				if (!verbose_inline) printf("-----------\n");
				else {
					clear_n(newlines);
				}
				newlines = 0;
			} else if (newlines && delay) nanosleep(&req, &rem);

			steps++;
			state_ptr = NULL;
		}

		if (!next_states.size) {
			state_ptr = NULL;
			break;
		}

		// Print epsilon-cycling next tapes/stacks
		if (flag_verbose > 1) {
			int max_rows = 20;
			int num_pages = ((next_states.size * ((is_pda) ? 2 : 1)) / max_rows);
			num_pages = (((next_states.size + num_pages) * ((is_pda) ? 2 : 1)) / max_rows)
				+ ((((next_states.size + num_pages) * ((is_pda) ? 2 : 1)) % max_rows != 0));
			int current_page = 1;

			if (verbose_inline)
				printf("epsilon cycles (1/%u):\n", num_pages);
			else
				printf("epsilon cycles:\n");

			newlines++;

			for (unsigned int i = 0; i < next_states.size; i++) {
				struct State *s0 = Stack_get(&next_states, i);
				struct Tape *tp0 = Stack_get(&next_tapes, i);
				struct Stack *stk0 = Stack_get(&next_stacks, i);

				printf("%s tape:  ", s0->name); Tape_print(tp0); 
				printf("\n%s stack: ", s0->name); Stack_print(stk0); putchar('\n');
				newlines+=2;
				
				if (verbose_inline && newlines >= max_rows) {
					if (delay) nanosleep(&req, &rem);
					clear_n(newlines);
					printf("epsilon cycles (%u/%u):\n", ++current_page, num_pages);
					newlines = 1;
				}
			}

			if (delay) nanosleep(&req, &rem);

			if (verbose_inline) {
				clear_n(newlines);
			} else {
				printf("-----------\n");
			}
			newlines = 0;
		}

		struct Stack swap = current_states;
		current_states = next_states;
		next_states = swap;

		swap = current_tapes;
		current_tapes = next_tapes;
		next_tapes = swap;

		swap = current_stacks;
		current_stacks = next_stacks;
		next_stacks = swap;

		state_ptr = NULL;
	}

run_end:

	run_memory += 
		Stack_free(&current_states) +
		Stack_free(&next_states) +
		
		Stack_free(&current_tapes) +
		Stack_free(&next_tapes) +
		Stack_free(&empty_tapes) +

		Stack_free(&current_stacks) +
		Stack_free(&next_stacks) +
		Stack_free(&empty_stacks);

	printf("STEPS: %zu\n", steps);

	return (state_ptr && state_ptr->final);
}

// Similar to NTM_stack_run(), except:
//
// > Rather than use a runtime stack of Tape copies,
//   a runtime stack of tape head positions is used,
//   because a PDA never changes the content of the tape.
// > Tape direction is hard-coded to move right.
// 
// See the comments above the NFA_run() function below
// for more information on how this function operates.
//
int PDA_run(struct Automaton *a0, struct Tape *input_tape)
{
	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_heads = Stack_init(HEAD);
	struct Stack next_heads = Stack_init(HEAD);
	struct Stack current_stacks = Stack_init(STACKPTR);
	struct Stack next_stacks = Stack_init(STACKPTR);

	struct Stack empty_stacks = Stack_init(STACKPTR);

	struct Stack *first_stack = Stack_add(a0, CELL);
	Stack_push(&current_stacks, &first_stack);

	Stack_push(&current_states, &a0->start);
	Stack_push(&current_heads, &input_tape->head);

	long int tapesize = input_tape->tape.size;

	struct State *state_ptr = NULL;
	struct Tape *tape_ptr = input_tape;
	struct Stack *stack_ptr = NULL;
	HEAD_TYPE head = 0;

	size_t steps = 0;
	int run_cond = 0;

	while(1) {
		
		while(1) {
			
			if (!current_states.size) { 
				state_ptr = NULL;
				break;
			}

			state_ptr = Stack_pop(&current_states);
			tape_ptr->head = head = *(HEAD_TYPE *)Stack_pop(&current_heads);
			stack_ptr = Stack_pop(&current_stacks);

			run_cond = tape_ptr->head < tapesize;

			if (state_ptr->final && !run_cond) {
				goto run_end;
			}

			// For NFA,PDA: If tape end reached, only allow epsilon transitions
			if (state_ptr->syms && run_cond) {
				CELL_TYPE input_sym = Tape_head(tape_ptr);

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);	

				while (trans_ptr) {

					struct Stack *stack_new = stack_ptr;
					stack_new = Stack_pop(&empty_stacks);	
					if (!stack_new) stack_new = Stack_add(a0, CELL);
					Stack_copy(stack_new, stack_ptr);

					int pda = 1;
					if (trans_ptr->pop) {
						CELL_TYPE *peek = NULL;
						if ((pda = (peek = Stack_peek(stack_new)) && *peek == trans_ptr->popsym)) {	
							Stack_pop(stack_new);
							if (trans_ptr->push) {
								Stack_push(stack_new, &trans_ptr->pushsym);
							}
						}
					} else if (trans_ptr->push) {
						Stack_push(stack_new, &trans_ptr->pushsym);
					}

					if (pda && !trans_ptr->dstate->reject) {

						//Tape_pos(tape_ptr, 2);
						tape_ptr->head++;

						if (!trans_ptr->dstate->reject) {
							Stack_push(&current_states, &trans_ptr->dstate);
							Stack_push(&current_heads, &tape_ptr->head);
							Stack_push(&current_stacks, &stack_new);
						} 

					} else {
						// reclaim rejected new stack
						Stack_clear(stack_new);
						Stack_push(&empty_stacks, &stack_new);
					}	

					tape_ptr->head = head;

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);
					else 
						break;
				}
			}

			if (state_ptr->epsilon) {

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);	
				while (trans_ptr) {

					struct Stack *stack_new = stack_ptr;
					stack_new = Stack_pop(&empty_stacks);	
					if (!stack_new) stack_new = Stack_add(a0, CELL);
					Stack_copy(stack_new, stack_ptr);

					int pda = 1;
					if (trans_ptr->pop) {
						CELL_TYPE *peek = NULL;
						if ((pda = (peek = Stack_peek(stack_new)) && *peek == trans_ptr->popsym)) {	
							Stack_pop(stack_new);
							if (trans_ptr->push) {
								Stack_push(stack_new, &trans_ptr->pushsym);
							}
						}
					} else if (trans_ptr->push) {
						Stack_push(stack_new, &trans_ptr->pushsym);
					}

					if (pda && !trans_ptr->dstate->reject) {
						if (trans_ptr->epsilon_loop) {
							Stack_push(&next_states, &trans_ptr->dstate);
							Stack_push(&next_heads, &tape_ptr->head);
							Stack_push(&next_stacks, &stack_new);
						} else {
							Stack_push(&current_states, &trans_ptr->dstate);
							Stack_push(&current_heads, &tape_ptr->head);
							Stack_push(&current_stacks, &stack_new);
						}
					} else { 	
						// reclaim rejected new stack
						Stack_clear(stack_new);
						Stack_push(&empty_stacks, &stack_new);
					}	

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);
					else 
						break;
				}
			} 

			Stack_clear(stack_ptr);
			Stack_push(&empty_stacks, &stack_ptr);

			steps++;
			state_ptr = NULL;

		}

		if (!next_states.size) {
			state_ptr = NULL;
			break;
		}

		struct Stack swap = current_states;
		current_states = next_states;
		next_states = swap;

		swap = current_heads;
		current_heads = next_heads;
		next_heads = swap;

		swap = current_stacks;
		current_stacks = next_stacks;
		next_stacks = swap;
		
		state_ptr = NULL;
	}

run_end:

	run_memory += 
		Stack_free(&current_states) +
		Stack_free(&next_states) +
		Stack_free(&current_heads) +
		Stack_free(&next_heads) +
		Stack_free(&current_stacks) +
		Stack_free(&next_stacks) +
		Stack_free(&empty_stacks);

	printf("STEPS: %zu\n", steps);

	return (state_ptr && state_ptr->final);
}

// Verbose version of PDA_run()
//
int PDA_run_verbose(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}

	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_heads = Stack_init(HEAD);
	struct Stack next_heads = Stack_init(HEAD);
	struct Stack current_stacks = Stack_init(STACKPTR);
	struct Stack next_stacks = Stack_init(STACKPTR);

	struct Stack empty_stacks = Stack_init(STACKPTR);

	struct Stack *first_stack = Stack_add(a0, CELL);
	Stack_push(&current_stacks, &first_stack);

	Stack_push(&current_states, &a0->start);
	Stack_push(&current_heads, &input_tape->head);

	long int tapesize = input_tape->tape.size;

	struct State *state_ptr = NULL;
	struct Tape *tape_ptr = input_tape;
	struct Stack *stack_ptr = NULL;
	int head = 0;

	int newlines = 0;
	size_t steps = 0;
	int run_cond = 0;

	while(1) {

		while(1) {

			if (!current_states.size) {
				state_ptr = NULL;
				goto run_end;
			}

			state_ptr = Stack_pop(&current_states);
			tape_ptr->head = head = *(HEAD_TYPE *)Stack_pop(&current_heads);
			stack_ptr = Stack_pop(&current_stacks);

			run_cond = tape_ptr->head < tapesize;

			if (state_ptr->final && !run_cond) {
				goto run_end;
			}

			if (verbose_inline) { 
				clear_n(newlines);
			}
			newlines = 0;

			// For NFA,PDA: If tape end reached, only allow epsilon transitions
			if (state_ptr->syms && run_cond) {
				CELL_TYPE input_sym = Tape_head(tape_ptr);

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);	

				while (trans_ptr) {

					struct Stack *stack_new = stack_ptr;
					stack_new = Stack_pop(&empty_stacks);	
					if (!stack_new) stack_new = Stack_add(a0, CELL);
					Stack_copy(stack_new, stack_ptr);

					int pda = 1;
					if (trans_ptr->pop) {
						CELL_TYPE *peek = NULL;
						if ((pda = (peek = Stack_peek(stack_new)) && *peek == trans_ptr->popsym)) {
							Stack_pop(stack_new);
							if (trans_ptr->push) {
								Stack_push(stack_new, &trans_ptr->pushsym);
							}
						}
					} else if (trans_ptr->push) {
						Stack_push(stack_new, &trans_ptr->pushsym);
					}

					if (pda) {
						//Tape_pos(tape_ptr, 2);
						tape_ptr->head++;

						if (!trans_ptr->dstate->reject) {
							Stack_push(&current_states, &trans_ptr->dstate);
							Stack_push(&current_heads, &tape_ptr->head);
							Stack_push(&current_stacks, &stack_new);
						} 
					} 

					Trans_print(trans_ptr);
					if (!pda) printf(" (pop fail)");
					printf("\n%*s tape:  ", longest_name, trans_ptr->dstate->name);
					Tape_print(tape_ptr);
					printf("\n%*s stack: ", longest_name, trans_ptr->dstate->name);
					Stack_print(stack_new);
					putchar('\n');
					newlines+=3;

					// reclaim rejected new stack
					if (!pda || trans_ptr->dstate->reject) {
						Stack_clear(stack_new);
						Stack_push(&empty_stacks, &stack_new);
					}	

					tape_ptr->head = head;

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);
					else 
						break;
				}

				if (offset == a0->trans.hashmax) {
					printf("%*s: ", longest_name, state_ptr->name);
					Trans_sym_run_print(input_sym);
					printf(" >\n%*s tape:  ", longest_name, state_ptr->name);
					Tape_print(tape_ptr);
					printf("\n%*s stack: ", longest_name, state_ptr->name);
					Stack_print(stack_ptr);
					putchar('\n');
					newlines+=3;
				}

			}

			if (state_ptr->epsilon) {

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);	
				while (trans_ptr) {

					struct Stack *stack_new = stack_ptr;
					stack_new = Stack_pop(&empty_stacks);	
					if (!stack_new) stack_new = Stack_add(a0, CELL);
					Stack_copy(stack_new, stack_ptr);

					int pda = 1;
					if (trans_ptr->pop) {
						CELL_TYPE *peek = NULL;
						if ((pda = (peek = Stack_peek(stack_new)) && *peek == trans_ptr->popsym)) {
							Stack_pop(stack_new);
							if (trans_ptr->push) {
								Stack_push(stack_new, &trans_ptr->pushsym);
							}
						}
					} else if (trans_ptr->push) {
						Stack_push(stack_new, &trans_ptr->pushsym);
					}

					if (pda && !trans_ptr->dstate->reject) {
						if (trans_ptr->epsilon_loop) {
							Stack_push(&next_states, &trans_ptr->dstate);
							Stack_push(&next_heads, &tape_ptr->head);
							Stack_push(&next_stacks, &stack_new);
						} else {
							Stack_push(&current_states, &trans_ptr->dstate);
							Stack_push(&current_heads, &tape_ptr->head);
							Stack_push(&current_stacks, &stack_new);
						}
					} 	

					Trans_print(trans_ptr);
					if (!pda) printf(" (pop fail)");
					printf("\n%*s tape:  ", longest_name, trans_ptr->dstate->name);
					Tape_print(tape_ptr);
					printf("\n%*s stack: ", longest_name, trans_ptr->dstate->name);
					Stack_print(stack_new);
					putchar('\n');
					newlines+=3;

					// reclaim rejected new stack
					if (!pda || trans_ptr->dstate->reject) {
						Stack_clear(stack_new);
						Stack_push(&empty_stacks, &stack_new);
					}	

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);
					else 
						break;
				}
			} 

			Stack_clear(stack_ptr);
			Stack_push(&empty_stacks, &stack_ptr);

			if (newlines) {
				if (debug >= 2) {
					printf("current_states.size: %u\n", current_states.size);
					printf("current_heads.size: %u\n", current_heads.size);
					printf("empty_stacks.size: %u\n", empty_stacks.size);
					printf("a0->stacks.size: %u\n", a0->stacks.size);
					newlines+=4;
				}

				if (delay) nanosleep(&req, &rem);

				if (!verbose_inline) { 
					printf("--------------\n");
				}
			}

			steps++;
			state_ptr = NULL;
		}

		if (!next_states.size) {
			state_ptr = NULL;
			break;
		}

		struct Stack swap = current_states;
		current_states = next_states;
		next_states = swap;

		swap = current_heads;
		current_heads = next_heads;
		next_heads = swap;

		swap = current_stacks;
		current_stacks = next_stacks;
		next_stacks = swap;

		state_ptr = NULL;
	}

run_end:

	if (verbose_inline) {
		printf("--------------\n");
	}

	run_memory += 
		Stack_free(&current_states) +
		Stack_free(&next_states) +
		Stack_free(&current_heads) +
		Stack_free(&next_heads) +
		Stack_free(&current_stacks) +
		Stack_free(&next_stacks) +
		Stack_free(&empty_stacks);

	printf("STEPS: %zu\n", steps);

	return (state_ptr && state_ptr->final);
}

// Simulates a nondeterministic finite automaton
// 
// Like PDA_run(), this uses a stack of head positions
// rather than makes entire tape copies. However, 
// this one is subtly different from the other nondeterministic
// functions, in that it will only push unique destination
// states to the current_* or next_* stack of states.
//   
//   --> This prevents an explosion of 'backtracking' that
//       occurs in the PDA_run() function for a string that
//       is ultimately rejected.
//   
//   --> Despite one branch reaching the end of the input string, 
//       existing stacks and strings in the runtime stacks must be 
//       confirmed to also reject. This is because PDA_run() does 
//       not (yet) keep track of failed string prefixes, nor is it using
//       a generative approach (ie. converting the PDA to a CFG)
//       to more efficiently accept/reject the string; at the
//       end of the day, I'm more interested inshowing the 
//       *behavior* of the PDA than necessarily accepting/rejecting 
//       the string most efficiently... for now.
//
//   --> So, in NFA_run(), this is backtracking on rejection is somewhat 
//       mitigated by taking the 'state-set' approach to some degree by 
//       only pushing unique states onto the runtime stacks.
//
int NFA_run(struct Automaton *a0, struct Tape *input_tape)
{
	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_heads = Stack_init(HEAD);
	struct Stack next_heads = Stack_init(HEAD);

	Stack_push(&current_states, &a0->start);
	Stack_push(&current_heads, &input_tape->head);

	long int tapesize = input_tape->tape.size;

	struct State *state_ptr = NULL;
	struct Tape *tape_ptr = input_tape;
	HEAD_TYPE head = 0;

	size_t steps = 0;
	int run_cond = 0;

	while (1) {

		while(1) {

			if (!current_states.size) {
				state_ptr = NULL;
				break;
			}

			state_ptr = Stack_pop(&current_states);
			tape_ptr->head = head = *(HEAD_TYPE *)Stack_pop(&current_heads);

			run_cond = tape_ptr->head < tapesize;

			if (state_ptr->final && !run_cond) {
				goto run_end;
			}

			// For NFA,PDA: If tape end reached, only allow epsilon transitions
			if (state_ptr->syms && run_cond) {
				CELL_TYPE input_sym = Tape_head(tape_ptr);

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);	

				while (trans_ptr) {

					//Tape_pos(tape_ptr, 2);
					tape_ptr->head++;

					if (!trans_ptr->dstate->reject) {
						if (Stack_push_unique(&next_states, &trans_ptr->dstate) != -1) 
							Stack_push(&next_heads, &tape_ptr->head);
					} 

					tape_ptr->head = head;

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);
					else 
						break;
				}
			}

			if (state_ptr->epsilon) {

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);	
				while (trans_ptr) {

					if (!trans_ptr->dstate->reject && !trans_ptr->epsilon_loop) {
						if (Stack_push_unique(&current_states, &trans_ptr->dstate) != -1)
							//Stack_push(&current_states, &trans_ptr->dstate);
							Stack_push(&current_heads, &tape_ptr->head);
					}

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);
					else 
						break;
				}
			} 

			steps++;
			state_ptr = NULL;

		}

		if (!next_states.size) {
			state_ptr = NULL;
			break;
		}

		struct Stack swap = current_states;
		current_states = next_states;
		next_states = swap;

		swap = current_heads;
		current_heads = next_heads;
		next_heads = swap;

	}

run_end:

	run_memory += 
		Stack_free(&current_states) +
		Stack_free(&next_states) +
		Stack_free(&current_heads) +
		Stack_free(&next_heads);

	printf("STEPS: %zu\n", steps);

	return (state_ptr && state_ptr->final);
}

// Verbose version of NFA_run()
//
int NFA_run_verbose(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}

	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_heads = Stack_init(HEAD);
	struct Stack next_heads = Stack_init(HEAD);

	Stack_push(&current_states, &a0->start);
	Stack_push(&current_heads, &input_tape->head);

	long int tapesize = input_tape->tape.size;

	struct State *state_ptr = NULL;
	struct Tape *tape_ptr = input_tape;
	HEAD_TYPE head = 0;

	int newlines = 0;
	size_t steps = 0;
	int run_cond = 0;

	while (1) {

		while(1) {

			if (!current_states.size) {
				state_ptr = NULL;
				break;
			}

			state_ptr = Stack_pop(&current_states);
			tape_ptr->head = head = *(HEAD_TYPE *)Stack_pop(&current_heads);

			run_cond = tape_ptr->head < tapesize;

			if (state_ptr->final && !run_cond) {
				goto run_end;
			}

			if (verbose_inline) {
				clear_n(newlines);
			}
			newlines = 0;

			// For NFA,PDA: If tape end reached, only allow epsilon transitions
			if (state_ptr->syms && run_cond) {
				CELL_TYPE input_sym = Tape_head(tape_ptr);

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);	

				while (trans_ptr) {
					//Tape_pos(tape_ptr, 2);
					tape_ptr->head++;

					if (!trans_ptr->dstate->reject) {
						if (Stack_push_unique(&next_states, &trans_ptr->dstate) != -1)	
							Stack_push(&next_heads, &tape_ptr->head);
					} 


					Trans_print(trans_ptr);
					printf("\n%*s tape:  ", longest_name, trans_ptr->dstate->name);
					Tape_print(tape_ptr);
					putchar('\n');
					newlines+=2;

					tape_ptr->head = head;

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);
					else 
						break;
				}

				if (offset == a0->trans.hashmax) {
					printf("%*s: ", longest_name, state_ptr->name);
					Trans_sym_run_print(input_sym);
					printf(" >\n%*s tape:  ", longest_name, state_ptr->name);
					Tape_print(tape_ptr);
					putchar('\n');
					newlines+=2;
				}
			}

			if (state_ptr->epsilon) {

				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);	
				while (trans_ptr) {

					if (!trans_ptr->dstate->reject && !trans_ptr->epsilon_loop) {
						if (Stack_push_unique(&current_states, &trans_ptr->dstate) != -1)
							//Stack_push(&current_states, &trans_ptr->dstate);
							Stack_push(&current_heads, &tape_ptr->head);
					}

					Trans_print(trans_ptr);
					printf("\n%*s tape:  ", longest_name, trans_ptr->dstate->name);
					Tape_print(tape_ptr);
					putchar('\n');
					newlines+=2;

					if (trans_ptr->strans) 
						trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);
					else 
						break;
				}
			} 

			if (newlines) {

				if (debug >= 2) {
					printf("current_states.size: %u\n", current_states.size);
					printf("current_heads.size: %u\n", current_heads.size);
					printf("next_states.size: %u\n", next_states.size);
					printf("next_heads.size: %u\n", next_heads.size);
					newlines+=4;
				}

				if (delay) nanosleep(&req, &rem);

				if (!verbose_inline) { 
					printf("--------------\n");
				}
			}

			steps++;
			state_ptr = NULL;

		}

		if (!next_states.size) {
			state_ptr = NULL;
			break;
		}

		struct Stack swap = current_states;
		current_states = next_states;
		next_states = swap;

		swap = current_heads;
		current_heads = next_heads;
		next_heads = swap;

	}

run_end:

	if (verbose_inline) {
		printf("--------------\n");
	}


	run_memory += 
		Stack_free(&current_states) +
		Stack_free(&next_states) +
		Stack_free(&current_heads) +
		Stack_free(&next_heads);

	printf("STEPS: %zu\n", steps);

	return (state_ptr && state_ptr->final);
}


// ==========================
// DETERMINISTIC ENGINES
// ==========================


// Simulates a deterministic Turing machine
//
int TM_run(struct Automaton *a0, struct Tape *input_tape)
{
	struct State *state_ptr = a0->start;
	CELL_TYPE input_sym = 0;
	size_t steps = 0;

	struct Trans *trans_ptr = NULL;
	while (1) {

		if (state_ptr->final) break;

		input_sym = Tape_head(input_tape);
		XXH64_hash_t offset = a0->trans.hashmax;
		trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);		

		if (trans_ptr) {	
			if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
			Tape_pos(input_tape, trans_ptr->dir);
			
			state_ptr = trans_ptr->dstate;
			if (state_ptr->reject) {
				state_ptr = NULL;
				break; 
			}
			// NO TRANS EXISTS
		} else { 
			state_ptr = NULL;
			break;
		}

		steps++;
	}

	fprintf(stderr, "STEPS: %zu\n", steps);

	return (state_ptr && state_ptr->final);
}

// Verbose version of TM_run()
//
int TM_run_verbose(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}

	struct State *state_ptr = a0->start;
	CELL_TYPE input_sym = 0;
	size_t steps = 0;

	struct Trans *trans_ptr = NULL;
	while (1) {

		if (state_ptr->final) break;

		input_sym = Tape_head(input_tape);
		XXH64_hash_t offset = a0->trans.hashmax;
		trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);		

		if (trans_ptr) {	
			if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
			Tape_pos(input_tape, trans_ptr->dir);
		
			Trans_print(trans_ptr); 
			putchar('\n');
			printf("   tape: ");
			Tape_print(input_tape);
			putchar('\n');

			state_ptr = trans_ptr->dstate;
			if (state_ptr->reject) {
				state_ptr = NULL;
				break; 
			}
			// NO TRANS EXISTS
		} else { 			
			printf("%*s: ", longest_name, state_ptr->name);
			Trans_sym_run_print(input_sym);
			printf(" >\n   tape: ");
			Tape_print(input_tape);
			putchar('\n');

			state_ptr = NULL;
			break;
		}

		steps++;
		
		if (delay) nanosleep(&req, &rem);
		
		if (verbose_inline) {
			if (!state_ptr->final) clear_n(2);
		} else {
			printf("--------------\n");
		}

	}

	if (verbose_inline || !state_ptr) printf("--------------\n");
	
	fprintf(stderr, "STEPS: %zu\n", steps);

	return (state_ptr && state_ptr->final);
}

// Same as TM_run(), except:
// > Includes pop/push symbol checks
// > Enforces stack top match for pop transition to 
//   proceed to destination state
//
int TM_stack_run(struct Automaton *a0, struct Tape *input_tape)
{
	struct Stack stack = Stack_init(CELL);
	struct State *state_ptr = a0->start;

	CELL_TYPE input_sym = 0;
	size_t steps = 0;

	struct Trans *trans_ptr = NULL;
	while (1) {

		if (state_ptr->final) break;

		input_sym = Tape_head(input_tape);
		XXH64_hash_t offset = a0->trans.hashmax;
		trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);		

		if (trans_ptr) {	
			// POP
			if (trans_ptr->pop) {
				CELL_TYPE *peek = Stack_peek(&stack);
				if (peek && *peek == trans_ptr->popsym) {
					Stack_pop(&stack);
					if (trans_ptr->push) {
						Stack_push(&stack, &trans_ptr->pushsym);
					}

					if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
					Tape_pos(input_tape, trans_ptr->dir);

					state_ptr = trans_ptr->dstate;
					if (state_ptr->reject) {
						state_ptr = NULL;
						break;
					}	
				} else { 
					state_ptr = NULL;
					break;
				}
			// PUSH
			} else if (trans_ptr->push) {

				Stack_push(&stack, &trans_ptr->pushsym);
				if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
				Tape_pos(input_tape, trans_ptr->dir);

				state_ptr = trans_ptr->dstate;
				if (state_ptr->reject) {
					state_ptr = NULL;
					break; 
				}
			// NO STACK OPS	
			} else { 
				if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
				Tape_pos(input_tape, trans_ptr->dir);

				state_ptr = trans_ptr->dstate;
				if (state_ptr->reject) {
					state_ptr = NULL;
					break; 
				}
			}
		// NO TRANS EXISTS
		} else { 
			state_ptr = NULL;
			break;
		}

		steps++;
	}

	fprintf(stderr, "STEPS: %zu\n", steps);

	run_memory += Stack_free(&stack);

	return (state_ptr && state_ptr->final);
}

// Verbose version of TM_stack_run()
//
int TM_stack_run_verbose(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}

	struct Stack stack = Stack_init(CELL);
	struct State *state_ptr = a0->start;

	CELL_TYPE input_sym = 0;
	size_t steps = 0;

	struct Trans *trans_ptr = NULL;
	while (1) {

		if (state_ptr->final) break;

		input_sym = Tape_head(input_tape);
		XXH64_hash_t offset = a0->trans.hashmax;
		trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);		

		if (trans_ptr) {	
			// POP
			if (trans_ptr->pop) {
				CELL_TYPE *peek = Stack_peek(&stack);
				if (peek && *peek == trans_ptr->popsym) {
					Stack_pop(&stack);
					if (trans_ptr->push) {
						Stack_push(&stack, &trans_ptr->pushsym);
					}

					if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
					Tape_pos(input_tape, trans_ptr->dir);

					Trans_print(trans_ptr);
					printf("\n   tape: ");
					Tape_print(input_tape);
					printf("\n  stack: ");
					Stack_print(&stack);
					putchar('\n');

					state_ptr = trans_ptr->dstate;
					if (state_ptr->reject) {
						state_ptr = NULL;
						break;
					}	
				} else { 
					Trans_print(trans_ptr); printf(" (pop fail)");
					printf("\n   tape: ");
					Tape_print(input_tape);
					printf("\n  stack: ");
					Stack_print(&stack);
					putchar('\n');

					state_ptr = NULL;
					break;
				}
			// PUSH
			} else if (trans_ptr->push) {

				Stack_push(&stack, &trans_ptr->pushsym);
				if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
				Tape_pos(input_tape, trans_ptr->dir);

				Trans_print(trans_ptr);
				printf("\n   tape: ");
				Tape_print(input_tape);
				printf("\n  stack: ");
				Stack_print(&stack);
				putchar('\n');

				state_ptr = trans_ptr->dstate;
				if (state_ptr->reject) {
					state_ptr = NULL;
					break; 
				}
			// NO STACK OPS	
			} else { 
				if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
				Tape_pos(input_tape, trans_ptr->dir);

				Trans_print(trans_ptr);
				printf("\n   tape: ");
				Tape_print(input_tape);
				printf("\n  stack: ");
				Stack_print(&stack);
				putchar('\n');
				
				state_ptr = trans_ptr->dstate;
				if (state_ptr->reject) {
					state_ptr = NULL;
					break; 
				}
			}
		// NO TRANS EXISTS
		} else { 
			printf("%*s: ", longest_name, state_ptr->name);
			Trans_sym_run_print(input_sym);
			printf(" >\n   tape: ");
			Tape_print(input_tape);
			printf("\n  stack: ");
			Stack_print(&stack);
			putchar('\n');
			
			state_ptr = NULL;
			break;
		}

		steps++;
		
		if (delay) nanosleep(&req, &rem);

		if (verbose_inline) {
			if (!state_ptr->final) clear_n(3);
		} else {
			printf("--------------\n");
		}
	}

	if (verbose_inline || !state_ptr) printf("--------------\n");
	
	fprintf(stderr, "STEPS: %zu\n", steps);

	run_memory += Stack_free(&stack);

	return (state_ptr && state_ptr->final);
}

// Identical to TM_stack_run, except:
// > no checks for tape-write symbols
// > tape head only moves right
//
int DPDA_run(struct Automaton *a0, struct Tape *input_tape)
{
	struct Stack stack = Stack_init(CELL);
	struct State *state_ptr = a0->start;

	long int origsize = input_tape->tape.size;

	CELL_TYPE input_sym = 0;
	size_t steps = 0;

	struct Trans *trans_ptr = NULL;
	while (origsize--) {

		input_sym = Tape_head(input_tape);
		XXH64_hash_t offset = a0->trans.hashmax;
		trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);		

		if (trans_ptr) {	
			// POP
			if (trans_ptr->pop) {
				CELL_TYPE *peek = Stack_peek(&stack);
				if (peek && *peek == trans_ptr->popsym) {
					Stack_pop(&stack);
					if (trans_ptr->push) {
						Stack_push(&stack, &trans_ptr->pushsym);
					}

					//Tape_pos(input_tape, 2);
					input_tape->head++;

					state_ptr = trans_ptr->dstate;
					if (state_ptr->reject) {
						state_ptr = NULL;
						break;
					}	
				} else { 
					state_ptr = NULL;
					break;
				}
			// PUSH
			} else if (trans_ptr->push) {
				Stack_push(&stack, &trans_ptr->pushsym);
				
				//Tape_pos(input_tape, 2);
				input_tape->head++;

				state_ptr = trans_ptr->dstate;
				if (state_ptr->reject) {
					state_ptr = NULL;
					break; 
				}
			// NO STACK OPS	
			} else { 
				//Tape_pos(input_tape, 2);
				input_tape->head++;

				state_ptr = trans_ptr->dstate;
				if (state_ptr->reject) {
					state_ptr = NULL;
					break; 
				}
			}
		// NO TRANS EXISTS
		} else { 
			state_ptr = NULL;
			break;
		}

		steps++;
	}

	fprintf(stderr, "STEPS: %zu\n", steps);

	run_memory += Stack_free(&stack);

	return (state_ptr && state_ptr->final);
}

// Verbose version of DPDA_run()
//
int DPDA_run_verbose(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}

	struct Stack stack = Stack_init(CELL);
	struct State *state_ptr = a0->start;

	long int origsize = input_tape->tape.size;

	CELL_TYPE input_sym = 0;
	size_t steps = 0;

	struct Trans *trans_ptr = NULL;
	while (origsize--) {

		input_sym = Tape_head(input_tape);
		XXH64_hash_t offset = a0->trans.hashmax;
		trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);		

		if (trans_ptr) {	
			// POP
			if (trans_ptr->pop) {
				CELL_TYPE *peek = Stack_peek(&stack);
				if (peek && *peek == trans_ptr->popsym) {
					Stack_pop(&stack);
					if (trans_ptr->push) {
						Stack_push(&stack, &trans_ptr->pushsym);
					}

					//Tape_pos(input_tape, 2);
					input_tape->head++;

					Trans_print(trans_ptr);
					printf("\n   tape: ");
					Tape_print(input_tape);
					printf("\n  stack: ");
					Stack_print(&stack);
					putchar('\n');

					state_ptr = trans_ptr->dstate;
					if (state_ptr->reject) {
						state_ptr = NULL;
						break;
					}	
				} else { 
					Trans_print(trans_ptr);
					printf("\n   tape: ");
					Tape_print(input_tape);
					printf("\n  stack: ");
					Stack_print(&stack);
					putchar('\n');

					state_ptr = NULL;
					break;
				}
			// PUSH
			} else if (trans_ptr->push) {

				Stack_push(&stack, &trans_ptr->pushsym);
				
				//Tape_pos(input_tape, 2);
				input_tape->head++;

				Trans_print(trans_ptr);
				printf("\n   tape: ");
				Tape_print(input_tape);
				printf("\n  stack: ");
				Stack_print(&stack);
				putchar('\n');

				state_ptr = trans_ptr->dstate;
				if (state_ptr->reject) {
					state_ptr = NULL;
					break; 
				}
			// NO STACK OPS	
			} else { 
				//Tape_pos(input_tape, 2);
				input_tape->head++;

				Trans_print(trans_ptr);
				printf("\n   tape: ");
				Tape_print(input_tape);
				printf("\n  stack: ");
				Stack_print(&stack);
				putchar('\n');
				
				state_ptr = trans_ptr->dstate;
				if (state_ptr->reject) {
					state_ptr = NULL;
					break; 
				}
			}
		// NO TRANS EXISTS
		} else { 
			printf("%*s: ", longest_name, state_ptr->name);
			Trans_sym_run_print(input_sym);
			printf(" >\n   tape: ");
			Tape_print(input_tape);
			printf("\n  stack: ");
			Stack_print(&stack);
			putchar('\n');
			
			state_ptr = NULL;
			break;
		}

		steps++;
		
		if (delay) nanosleep(&req, &rem);

		if (verbose_inline) {
			if (!state_ptr->final) clear_n(3);
		} else {
			printf("--------------\n");
		}
	}

	if (verbose_inline || !state_ptr) printf("--------------\n");
	
	fprintf(stderr, "STEPS: %zu\n", steps);

	run_memory += Stack_free(&stack);

	return (state_ptr && state_ptr->final);
}

// Only checks for existing input symbol
//
int DFA_run(struct Automaton *a0, struct Tape *input_tape)
{
	struct State *state_ptr = a0->start;

	CELL_TYPE input_sym = 0;
	size_t steps = 0;
	
	size_t origsize = input_tape->tape.size;
	struct Trans *trans_ptr = NULL;
	
	while (origsize--) {

		input_sym = Tape_head(input_tape);
		XXH64_hash_t offset = a0->trans.hashmax;
		trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);		

		if (trans_ptr) {	
				//Tape_pos(input_tape, 2);
				input_tape->head++;

				state_ptr = trans_ptr->dstate;
				if (state_ptr->reject) {
					state_ptr = NULL;
					break; 
				}
			// NO TRANS EXISTS
		} else { 
			state_ptr = NULL;
			break;
		}

		steps++;
	}

	fprintf(stderr, "STEPS: %zu\n", steps);
	
	return (state_ptr && state_ptr->final);
}

// Verbose version of DFA_run()
//
int DFA_run_verbose(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}
	
	struct State *state_ptr = a0->start;

	CELL_TYPE input_sym = 0;
	size_t steps = 0;
	
	size_t origsize = input_tape->tape.size;
	struct Trans *trans_ptr = NULL;
	
	while (origsize--) {

		input_sym = Tape_head(input_tape);
		XXH64_hash_t offset = a0->trans.hashmax;
		trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);		

		if (trans_ptr) {	
				//Tape_pos(input_tape, 2);
				input_tape->head++;

				Trans_print(trans_ptr);
				printf("\n   tape: ");
				Tape_print(input_tape);
				putchar('\n');

				state_ptr = trans_ptr->dstate;
				if (state_ptr->reject) {
					state_ptr = NULL;
					break; 
				}
			// NO TRANS EXISTS
		} else { 
			printf("%*s: ", longest_name, state_ptr->name);
			Trans_sym_run_print(input_sym);
			printf(" >\n   tape: ");
			Tape_print(input_tape);
			putchar('\n');

			state_ptr = NULL;
			break;
		}

		steps++;

		if (delay) nanosleep(&req, &rem);

		if (verbose_inline && origsize) {
			clear_n(2);
		} else {
			printf("--------------\n");
		}
	}

	fprintf(stderr, "STEPS: %zu\n", steps);
	
	return (state_ptr && state_ptr->final);
}

