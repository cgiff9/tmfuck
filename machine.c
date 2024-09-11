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
	if (!n) return;
	//while (n--) {
		//printf("\r\033[K");
		printf("\033[%dA\033[J", n);
	//}
	//printf("\r\033[K");
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
void set_marks(struct Automaton *a0)
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

int Automaton_run_nd(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}

	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_tapes = Stack_init(TAPEPTR);
	struct Stack next_tapes = Stack_init(TAPEPTR);
	struct Stack current_heads = Stack_init(PTRDIFFT);
	struct Stack next_heads = Stack_init(PTRDIFFT);
	struct Stack current_stacks;
	struct Stack next_stacks;

	struct Stack empty_tapes = Stack_init(TAPEPTR);
	struct Stack empty_tapes_ghost = Stack_init(TAPEPTR);
	struct Stack empty_stacks;

	struct Stack *first_stack = NULL;
	if (is_pda) {
		next_stacks = Stack_init(STACKPTR);
		current_stacks = Stack_init(STACKPTR);
		empty_stacks = Stack_init(STACKPTR);
		first_stack = Stack_add(a0, CELL);
		Stack_push(&current_stacks, &first_stack);
	}

	if (!is_tm) Stack_push(&current_heads, &input_tape->head);

	Stack_push(&current_states, &a0->start);
	Stack_push(&current_tapes, &input_tape);

		if (!is_tm && input_tape->tape.size == input_tape->tape.max) {
		Stack_push(&input_tape->tape, &tm_blank);
		input_tape->tape.size--;
	}
	long int tapesize = input_tape->tape.size;

	struct State *state_ptr = NULL;
	struct Tape *tape_ptr = input_tape;
	struct Stack *stack_ptr = NULL;
	unsigned int head = 0;

	int sym_trans = 0;
	int newlines = 0;
	size_t steps = 0;
	int run_cond = 0;
	int loop_halt = 0;

	while (current_states.size || next_states.size) {

		state_ptr = Stack_pop(&current_states);
		if (is_tm) {
			tape_ptr = Stack_pop(&current_tapes);
		} else {
			tape_ptr->head = head = *(ptrdiff_t *)Stack_pop(&current_heads);
			//if (head > max_head) max_head = head;
		}
		if (is_pda) stack_ptr = Stack_pop(&current_stacks);

		run_cond = !is_tm && tape_ptr->head < tapesize;

		if (state_ptr->final && (is_tm || !run_cond)) {
			break;
		}

		// For NFA,PDA: If tape end reached, only allow epsilon transitions
		if (state_ptr->syms && (is_tm || run_cond)) {
			CELL_TYPE input_sym = Tape_head(tape_ptr);

			XXH64_hash_t offset = a0->trans.hashmax;
			struct Trans *trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);	
			int strans = 0;

			while (trans_ptr) {
				strans += trans_ptr->strans;

				struct Stack *stack_new = stack_ptr;
				struct Tape *tape_new = tape_ptr;
				
				CELL_TYPE *pda = &trans_ptr->popsym;
				if (is_pda) {
					stack_new = Stack_pop(&empty_stacks);	
					if (!stack_new) stack_new = Stack_add(a0, CELL);
					Stack_copy(stack_new, stack_ptr);

					if (trans_ptr->pop) {
						if (((pda = Stack_peek(stack_new)) && *pda == trans_ptr->popsym)) {
							Stack_pop(stack_new);
							if (trans_ptr->push) {
								Stack_push(stack_new, &trans_ptr->pushsym);
							}
						}
					} else if (trans_ptr->push) {
						Stack_push(stack_new, &trans_ptr->pushsym);
					}
				}

				if (pda) {

					if (is_tm) {						
						if (0&&trans_ptr->strans && !trans_ptr->write) { 
							tape_new = Stack_pop(&empty_tapes_ghost);
							if (!tape_new) tape_new = Tape_add(a0);
							Tape_ghost(tape_new, tape_ptr);
						} else {
							tape_new = Stack_pop(&empty_tapes);
							if (!tape_new) tape_new = Tape_add(a0);
							Tape_copy(tape_new, tape_ptr);
						}
						
						if (trans_ptr->write) Tape_write(tape_new, &trans_ptr->writesym);
						Tape_pos(tape_new, trans_ptr->dir);
					} else {
						//Tape_ghost(tape_new, tape_ptr);
						Tape_pos(tape_new, 2);
					}

					if (!trans_ptr->dstate->reject) {
						sym_trans = 1;
						
						/*if (1||is_tm) {
						  Stack_push(&next_states, &trans_ptr->dstate);
						  Stack_push(&next_tapes, &tape_new);
						  if (is_pda) {
						  Stack_push(&next_stacks, &stack_new);
						  }
						  } else {
						  Stack_push(&current_states, &trans_ptr->dstate);
						  Stack_push(&current_tapes, &tape_new);
						  if (is_pda) {
						  Stack_push(&current_stacks, &stack_new);
						  }
						  }*/

						Stack_push(&current_states, &trans_ptr->dstate);
						if (is_tm) {
							Stack_push(&current_tapes, &tape_new);
						} else { 
							Stack_push(&current_heads, &tape_new->head);
							//tape_ptr->head = head; // 'reset' head for other transitions
						}

						if (is_pda) {
							Stack_push(&current_stacks, &stack_new);
						}
					} 

					if (flag_verbose) {
						Trans_print(trans_ptr);
						putchar('\n');
						newlines++;
						printf("%*s tape:  ", longest_name, trans_ptr->dstate->name);
						Tape_print(tape_new);
						putchar('\n');
						newlines++;
						if (is_pda) {
							printf("%*s stack: ", longest_name, trans_ptr->dstate->name);
							Stack_print(stack_new);
							putchar('\n');
							newlines++;
						}
					}

					if (!is_tm) tape_ptr->head = head;
				}

				if (trans_ptr->strans) 
					trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);
				else 
					break;
			}

			if (flag_verbose && offset == a0->trans.hashmax) {
				printf("%s:%*s", state_ptr->name, longest_name, " ");
				Trans_sym_print(input_sym);
				printf(" >\n");
				newlines++;

				//if (is_tm) {
					//printf("   tape: ");
					printf(" %*s: ", longest_name, "rejected");
					Tape_print(tape_ptr);
					putchar('\n');
					newlines++;
				//}
				if (is_pda) {
					printf(" %*s: ", longest_name, "stack");
					Stack_print(&empty_stacks);
					putchar('\n');
					newlines++;
				}
			}
		}

		if (state_ptr->epsilon) {

			XXH64_hash_t offset = a0->trans.hashmax;
			struct Trans *trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);	
			int strans = 0;
			while (trans_ptr) {
				/*for (unsigned int i = 0; i < state_ptr->e_closure->size; i++) 
				  struct Trans *trans_ptr = Stack_Transptr_get(state_ptr->e_closure, i);
				  if (trans_ptr->pstate != state_ptr) break;*/

				strans += trans_ptr->strans;
				struct Stack *stack_new = stack_ptr;
				struct Tape *tape_new = tape_ptr;
				
				CELL_TYPE *pda = &trans_ptr->popsym;
				if (is_pda) {
					stack_new = Stack_pop(&empty_stacks);	
					if (!stack_new) stack_new = Stack_add(a0, CELL);
					Stack_copy(stack_new, stack_ptr);

					if (trans_ptr->pop) {
						if (((pda = Stack_peek(stack_new)) && *pda == trans_ptr->popsym)) {
							Stack_pop(stack_new);
							if (trans_ptr->push) {
								Stack_push(stack_new, &trans_ptr->pushsym);
							}
						}
					} else if (trans_ptr->push) {
						Stack_push(stack_new, &trans_ptr->pushsym);
					}
				}

				if (pda) {

					if (is_tm) {
						//tape_new = Stack_Tapeptr_pop(&empty_tapes);
						//if (!tape_new) tape_new = Tape_add(a0);				
						
						if (0&&trans_ptr->strans && !trans_ptr->write) {
							tape_new = Stack_pop(&empty_tapes_ghost);
							if (!tape_new) tape_new = Tape_add(a0);				
							Tape_ghost(tape_new, tape_ptr);
						} else {
							tape_new = Stack_pop(&empty_tapes);
							if (!tape_new) tape_new = Tape_add(a0);				
							Tape_copy(tape_new, tape_ptr);
						}
						
						if (trans_ptr->write) Tape_write(tape_new, &trans_ptr->writesym);
						Tape_pos(tape_new, trans_ptr->dir);
					} /*else {
						//Tape_ghost(tape_new, tape_ptr);
						Tape_pos(tape_ptr, 2);
					}*/

					if (trans_ptr->epsilon_loop) {

						if (!trans_ptr->dstate->reject) {
							if (is_tm) {
								Stack_push(&next_states, &trans_ptr->dstate);
								Stack_push(&next_tapes, &tape_new);
								if (is_pda) {
									Stack_push(&next_stacks, &stack_new);
								}
							} else {
								Stack_push(&next_states, &trans_ptr->dstate);
								//Stack_push(&next_tapes, &tape_new);
								Stack_push(&next_heads, &tape_new->head);
								if (is_pda) {
									Stack_push(&next_stacks, &stack_new);
								}
								if (tape_new->head >= tapesize) loop_halt = 1;
							}
						}

						if (flag_verbose) {
							Trans_print(trans_ptr);
							putchar('\n');
							newlines++;
							printf("%*s tape:  ", longest_name, trans_ptr->dstate->name);
							Tape_print(tape_new);
							putchar('\n');
							newlines++;
							if (is_pda) {
								printf("%*s stack: ", longest_name, trans_ptr->dstate->name);
								Stack_print(stack_new);
								putchar('\n');
								newlines++;
							}
						}

					} else if (!trans_ptr->dstate->reject) {

						Stack_push(&current_states, &trans_ptr->dstate);
						if (is_tm) Stack_push(&current_tapes, &tape_new);
						else Stack_push(&current_heads, &tape_new->head);

						if (is_pda) {
							Stack_push(&current_stacks, &stack_new);
						}

						if (flag_verbose) {
							Trans_print(trans_ptr);
							putchar('\n');
							newlines++;
							printf("%*s tape:  ", longest_name, trans_ptr->dstate->name);
							Tape_print(tape_new);
							putchar('\n');
							newlines++;
							if (is_pda) {
								printf("%*s stack: ", longest_name, trans_ptr->dstate->name);
								Stack_print(stack_new);
								putchar('\n');
								newlines++;
							}
						}
					}

				}

				if (trans_ptr->strans) 
					trans_ptr = Trans_get(a0, state_ptr, CELL_MAX, &offset, 1);
				else 
					break;
			}
		} 

		if (is_tm) {
			/*if (tape_ptr->ghost) Stack_push(&empty_tapes_ghost, &tape_ptr);
			else*/ Stack_push(&empty_tapes, &tape_ptr);
			Tape_clear(tape_ptr);
		}

		if (is_pda) {
			Stack_clear(stack_ptr);
			Stack_push(&empty_stacks, &stack_ptr);
		}

		if (flag_verbose && newlines) {
			if (debug >= 2) {
				printf("current_states.size: %u\n", current_states.size);
				printf("current_tapes.size: %u\n", current_tapes.size);
				if (is_pda) { 
					printf("current_stacks.size: %u\n", current_stacks.size);
					newlines++;
				}
				printf("empty_tapes.size: %u\n", empty_tapes.size);
				if (is_pda) { 
					printf("empty_stacks.size: %u\n", empty_stacks.size);
					newlines++;
				}
				printf("a0->tapes.size: %u\n", a0->tapes.size);
				printf("a0->stacks.size: %u\n", a0->stacks.size);
				newlines+=5;
			}

			if (delay) nanosleep(&req, &rem);

			if (!verbose_inline) printf("-----------\n");
			else {
				//if (!is_tm && !(loop_halt || !sym_trans))
				clear_n(newlines);
			}
			newlines = 0;
		} else if (newlines && delay) nanosleep(&req, &rem);


		if (!current_states.size && next_states.size) {
			
			if (!is_tm) {
				/*int halt = 0;
				for (unsigned int i = 0; i < next_heads.size; i++) {
					ptrdiff_t headtmp = Stack_ptrdifft_get(&next_heads, i);
					if (headtmp >= tapesize) {
						halt = 1;
						break;
					}
				}*/
				if (loop_halt || !sym_trans) {
					state_ptr = NULL;
					break;
				}
				loop_halt = 0;
				sym_trans = 0;
			}


			/*if (!is_tm) {
				if (max_head_prev >= max_head) {
					state_ptr = NULL;
					break;
				}
				max_head_prev = max_head;
			 }*/

			/*struct State *state_tmp = Stack_Stateptr_pop(&next_states);
			  struct Tape *tape_tmp = Stack_Tapeptr_pop(&next_tapes);
			  struct Stack *stack_tmp = Stack_Stackptr_pop(&next_stacks);

			  while (state_tmp) {
			  Stack_push(&current_states, &state_tmp);
			  Stack_push(&current_tapes, &tape_tmp);

			  if (0&&flag_verbose) {
			  printf("%s tape:  ", state_tmp->name);
			  Tape_print(tape_tmp);
			  putchar('\n');
			  newlines++;
			  }

			  if (is_pda) {
			  if (0&&flag_verbose) {
			  printf("%s stack: ", state_tmp->name);
			  Stack_print(stack_tmp);
			  putchar('\n');
			  newlines++;
			  }

			  Stack_push(&current_stacks, &stack_tmp);
			  stack_tmp = Stack_Stackptr_pop(&next_stacks);
			  }

			  state_tmp = Stack_Stateptr_pop(&next_states);
			  tape_tmp = Stack_Tapeptr_pop(&next_tapes);
			  }

			  if (delay) nanosleep(&req, &rem);
			  if (flag_verbose && !verbose_inline) printf("-----------\n");
			  else {
			  clear_n(newlines);
			  }
			  newlines = 0;*/

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
					//struct Tape *tp0 = Stack_Tapeptr_get(&next_tapes, i);
					struct Tape *tp0 = NULL;
					if (!is_tm) {
						tp0 = tape_ptr;
						tp0->head = *(ptrdiff_t *)Stack_get(&next_heads, i);
					} else {
						tp0 = Stack_get(&next_tapes, i);
					}
					
					printf("%s tape:  ", s0->name); Tape_print(tp0); putchar('\n');
					newlines++;
					if (is_pda) {
						struct Stack *stk0 = Stack_get(&next_stacks, i);
						printf("%s stack: ", s0->name); Stack_print(stk0); putchar('\n');
						newlines++;
					}
					if (verbose_inline && newlines >= max_rows) {
						if (delay) nanosleep(&req, &rem);
						//sleep(1);
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

			if (is_tm) {
				swap = current_tapes;
				current_tapes = next_tapes;
				next_tapes = swap;
			} else {
				swap = current_heads;
				current_heads = next_heads;
				next_heads = swap;
			}

			if (is_pda) {
				swap = current_stacks;
				current_stacks = next_stacks;
				next_stacks = swap;
			}
		}

			steps++;
			state_ptr = NULL;
		}

		run_memory += 
			Stack_free(&current_states) +
			Stack_free(&current_tapes) +
			
			Stack_free(&next_states) +
			Stack_free(&next_tapes) +

			Stack_free(&current_heads) +
			Stack_free(&next_heads) +

			Stack_free(&empty_tapes) +
			Stack_free(&empty_tapes_ghost);

		if (is_pda) {
			run_memory += 
				Stack_free(&current_stacks) +
				Stack_free(&next_stacks) +
				Stack_free(&empty_stacks);
			//Stack_free(&first_stack);
		}

		printf("STEPS: %zu\n", steps);
		if (state_ptr && state_ptr->final) return 1;
	return 0;

}

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

					CELL_TYPE *pda = &trans_ptr->popsym; // just need a non-null ptr here
					if (trans_ptr->pop) {
						//if ((pda = Stack_cell_peek(stack_new) == trans_ptr->popsym)) {
						if ((pda = Stack_peek(stack_new)) && *pda == trans_ptr->popsym) {
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
						//Tape_clear(tape_new);
						//Stack_push(&empty_tapes, &tape_new);
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

					CELL_TYPE *pda = &trans_ptr->popsym;
					if (trans_ptr->pop) {
						//if ((pda = Stack_cell_peek(stack_new) == trans_ptr->popsym)) {
						if ((pda = Stack_peek(stack_new)) && *pda == trans_ptr->popsym) {
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
						//Tape_clear(tape_new);
						//Stack_push(&empty_tapes, &tape_new);
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

					CELL_TYPE *pda = &trans_ptr->popsym;
					if (trans_ptr->pop) {
						//if ((pda = Stack_cell_peek(stack_new) == trans_ptr->popsym)) {
						if ((pda = Stack_peek(stack_new)) && *pda == trans_ptr->popsym) {
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

					CELL_TYPE *pda = &trans_ptr->popsym;
					if (trans_ptr->pop) {
						//if ((pda = Stack_cell_peek(stack_new) == trans_ptr->popsym)) {
						if ((pda = Stack_peek(stack_new)) && *pda == trans_ptr->popsym) {
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

int PDA_run(struct Automaton *a0, struct Tape *input_tape)
{
	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_heads = Stack_init(PTRDIFFT);
	struct Stack next_heads = Stack_init(PTRDIFFT);
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
	unsigned int head = 0;

	size_t steps = 0;
	int run_cond = 0;

	while(1) {
		
		while(1) {
			
			if (!current_states.size) { 
				state_ptr = NULL;
				break;
			}

			state_ptr = Stack_pop(&current_states);
			tape_ptr->head = head = *(ptrdiff_t *)Stack_pop(&current_heads);
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

					CELL_TYPE *pda = &trans_ptr->popsym;
					if (trans_ptr->pop) {
						//if ((pda = Stack_cell_peek(stack_new) == trans_ptr->popsym)) {
						if ((pda = Stack_peek(stack_new)) && *pda == trans_ptr->popsym) {
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

					CELL_TYPE *pda = &trans_ptr->popsym;
					if (trans_ptr->pop) {
						//if ((pda = Stack_cell_peek(stack_new) == trans_ptr->popsym)) {
						if ((pda = Stack_peek(stack_new)) && *pda == trans_ptr->popsym) {
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

int PDA_run_verbose(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}

	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_heads = Stack_init(PTRDIFFT);
	struct Stack next_heads = Stack_init(PTRDIFFT);
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
	unsigned int head = 0;

	int newlines = 0;
	size_t steps = 0;
	int run_cond = 0;

	while(1) {

		while(1) {

			if (!current_states.size) {
				state_ptr = NULL;
				break;
			}

			state_ptr = Stack_pop(&current_states);
			tape_ptr->head = head = *(ptrdiff_t *)Stack_pop(&current_heads);
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

					CELL_TYPE *pda = &trans_ptr->popsym;
					if (trans_ptr->pop) {
						//if ((pda = Stack_cell_peek(stack_new) == trans_ptr->popsym)) {
						if ((pda = Stack_peek(stack_new)) && *pda == trans_ptr->popsym) {
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

					CELL_TYPE *pda = &trans_ptr->popsym;
					if (trans_ptr->pop) {
						//if ((pda = Stack_cell_peek(stack_new) == trans_ptr->popsym)) {
						if ((pda = Stack_peek(stack_new)) && *pda == trans_ptr->popsym) {
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

int NFA_run(struct Automaton *a0, struct Tape *input_tape)
{
	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_heads = Stack_init(PTRDIFFT);
	struct Stack next_heads = Stack_init(PTRDIFFT);

	Stack_push(&current_states, &a0->start);
	Stack_push(&current_heads, &input_tape->head);

	long int tapesize = input_tape->tape.size;

	struct State *state_ptr = NULL;
	struct Tape *tape_ptr = input_tape;
	unsigned int head = 0;

	size_t steps = 0;
	int run_cond = 0;

	while (1) {

		while(1) {

			if (!current_states.size) {
				state_ptr = NULL;
				break;
			}

			state_ptr = Stack_pop(&current_states);
			tape_ptr->head = head = *(ptrdiff_t *)Stack_pop(&current_heads);

			run_cond = tape_ptr->head < tapesize;

			if (state_ptr->final && !run_cond) {
				goto run_end;
				//break;
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

int NFA_run_verbose(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}

	struct Stack current_states = Stack_init(STATEPTR);
	struct Stack next_states = Stack_init(STATEPTR);
	struct Stack current_heads = Stack_init(PTRDIFFT);
	struct Stack next_heads = Stack_init(PTRDIFFT);

	Stack_push(&current_states, &a0->start);
	Stack_push(&current_heads, &input_tape->head);

	long int tapesize = input_tape->tape.size;

	struct State *state_ptr = NULL;
	struct Tape *tape_ptr = input_tape;
	unsigned int head = 0;

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
			tape_ptr->head = head = *(ptrdiff_t *)Stack_pop(&current_heads);

			run_cond = tape_ptr->head < tapesize;

			if (state_ptr->final && !run_cond) {
				goto run_end;
				//break;
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
				//if (peek == trans_ptr->popsym) {
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

/*
int TM_stack_run_verbose_poo(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}

	// Nondeterministic machine flag
	int nd = a0->epsilon + a0->strans;
	//int nd = a0->strans;

	// DEBUG: input tape contents
	//for (size_t i = 0; i < input_tape.size; i++) {
	//  printf("%td,", Stack_cell_get(&input_tape, i));
	//}

	//struct Stack current_states = Stack_init(STATEPTR);
	//struct Stack next_states = Stack_init(STATEPTR);
	//struct Stack tape_initial = Stack_init(TAPEPTR);
	//struct Stack empty_tapes = Stack_init(TAPEPTR);

	// For deterministic stack machines, empty_stacks will
	// simply contain a single stack of cells, rather than
	// a stack of empty stacks
	struct Stack empty_stacks = (nd) ? Stack_init(STACKPTR) : Stack_init(CELL);

	struct State *state_ptr = a0->start;

	CELL_TYPE input_sym = 0;
	size_t steps = 0;
	int res = 0;
	int newlines = 0;
	size_t origsize = input_tape->tape.size;

	struct Trans *trans_ptr = NULL;

	while (!state_ptr->final) {

		//if (state_ptr->final) break;

		if (verbose_inline) {
			clear_n(newlines);
			newlines = 0;
		}

		input_sym = Tape_head(input_tape);

		XXH64_hash_t offset = a0->trans.hashmax;
		trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);		

		if (trans_ptr) {	
			if (is_pda) {
				if (trans_ptr->pop) {
					CELL_TYPE peek = Stack_cell_peek(&empty_stacks);
					if (peek == trans_ptr->popsym) {
						Stack_cell_pop(&empty_stacks);
						if (trans_ptr->push) {
							Stack_push(&empty_stacks, &trans_ptr->pushsym);
						}

						if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
						Tape_pos(input_tape, trans_ptr->dir);

						if (!trans_ptr->dstate->reject) {
							if (flag_verbose) {
								Trans_print(trans_ptr); putchar('\n');
								//printf("%s: ", state_ptr->name);
								//Trans_sym_print(input_sym);
								//printf(" > %s\n", trans_ptr->dstate->name);
								newlines++;

									printf("   tape: ");
									Tape_print(input_tape);
									putchar('\n');
									newlines++;
								
								if (is_pda) {
									printf("  stack: ");
									Stack_print(&empty_stacks);
									putchar('\n');
									newlines++;
								}
							}
							state_ptr = trans_ptr->dstate;
						} else {
							if (flag_verbose) {
								Trans_print(trans_ptr); putchar('\n');
								//printf("%s: ", state_ptr->name);
								//Trans_sym_print(input_sym);
								//printf(" > \n");
								newlines++;

									printf("   tape: ");
									Tape_print(input_tape);
									putchar('\n');
									newlines++;
								
									if (is_pda) {
									printf("  stack: ");
									Stack_print(&empty_stacks);
									putchar('\n');
									newlines++;
								}

							}
							state_ptr = NULL;
							break;
						}	
					} else { 
						if (flag_verbose) {
							Trans_print(trans_ptr); putchar('\n');
							//printf("%s: ", state_ptr->name);
							//Trans_sym_print(input_sym);
							//printf(" > \n");
							newlines++;

								printf("   tape: ");
								Tape_print(input_tape);
								putchar('\n');
								newlines++;
							if (is_pda) {
								printf("  stack: ");
								Stack_print(&empty_stacks);
								putchar('\n');
								newlines++;
							}

						}
						state_ptr = NULL;
						break;
					}
				} else if (trans_ptr->push) {
					Stack_push(&empty_stacks, &trans_ptr->pushsym);
					if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
					Tape_pos(input_tape, trans_ptr->dir);

					if (!trans_ptr->dstate->reject) {
						if (flag_verbose) {
							Trans_print(trans_ptr); putchar('\n');
							//printf("%s: ", state_ptr->name);
							//Trans_sym_print(input_sym);
							//printf(" > %s\n", trans_ptr->dstate->name);
							newlines++;

								printf("   tape: ");
								Tape_print(input_tape);
								putchar('\n');
								newlines++;
							if (is_pda) {
								printf("  stack: ");
								Stack_print(&empty_stacks);
								putchar('\n');
								newlines++;
							}

						}
						state_ptr = trans_ptr->dstate;
					} else { 
						if (flag_verbose) {
							Trans_print(trans_ptr); putchar('\n');
							//printf("%s: ", state_ptr->name);
							//Trans_sym_print(input_sym);
							//printf(" > \n");
							newlines++;

								printf("   tape: ");
								Tape_print(input_tape);
								putchar('\n');
								newlines++;
							if (is_pda) {
								printf("  stack: ");
								Stack_print(&empty_stacks);
								putchar('\n');
								newlines++;
							}

						}
						state_ptr = NULL;
						break; 
					}	
				} else { 
					if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
					Tape_pos(input_tape, trans_ptr->dir);
					
					if (!trans_ptr->dstate->reject) {
						if (flag_verbose) {
							Trans_print(trans_ptr); putchar('\n');
							//printf("%s: ", state_ptr->name);
							//Trans_sym_print(input_sym);
							//printf(" > %s\n", trans_ptr->dstate->name);
							newlines++;

								printf("   tape: ");
								Tape_print(input_tape);
								putchar('\n');
								newlines++;
							if (is_pda) {
								printf("  stack: ");
								Stack_print(&empty_stacks);
								putchar('\n');
								newlines++;
							}

						}
						state_ptr = trans_ptr->dstate;
					} else { 
						if (flag_verbose) {
							Trans_print(trans_ptr); putchar('\n');
							//printf("%s: ", state_ptr->name);
							//Trans_sym_print(input_sym);
							//printf(" >\n");
							newlines++;

								printf("   tape: ");
								Tape_print(input_tape);
								putchar('\n');
								newlines++;
							if (is_pda) {
								printf("  stack: ");
								Stack_print(&empty_stacks);
								putchar('\n');
								newlines++;
							}
						}
						state_ptr = NULL;
						break; 
					}
				}
				// NOT PDA
			} else {
				if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
				Tape_pos(input_tape, trans_ptr->dir);

				if (!trans_ptr->dstate->reject) {
					if (flag_verbose) {
						Trans_print(trans_ptr); putchar('\n');
						//printf("%s: ", state_ptr->name);
						//Trans_sym_print(input_sym);
						//printf(" > %s\n", trans_ptr->dstate->name);
						newlines++;

							printf("   tape: ");
							Tape_print(input_tape);
							putchar('\n');
							newlines++;
						
							if (is_pda) {
							printf("  stack: ");
							Stack_print(&empty_stacks);
							putchar('\n');
							newlines++;
						}

					}
					state_ptr = trans_ptr->dstate;
				} else { 
					if (flag_verbose) {
						Trans_print(trans_ptr); putchar('\n');
						//printf("%s: ", state_ptr->name);
						//Trans_sym_print(input_sym);
						//printf(" >\n");
						newlines++;

							printf("   tape: ");
							Tape_print(input_tape);
							putchar('\n');
							newlines++;
						if (is_pda) {
							printf("  stack: ");
							Stack_print(&empty_stacks);
							putchar('\n');
							newlines++;
						}
					}
					state_ptr = NULL;
					break; 
				}
			}
			// NO TRANS EXISTS
		} else { 
			if (flag_verbose) {
				printf("%s: ", state_ptr->name);
				Trans_sym_print(input_sym);
				printf(" >\n");
				newlines++;

					printf("   tape: ");
					Tape_print(input_tape);
					putchar('\n');
					newlines++;
				if (is_pda) {
					printf("  stack: ");
					Stack_print(&empty_stacks);
					putchar('\n');
					newlines++;
				}
			}

			state_ptr = NULL;
			break;
		}

		if (flag_verbose && !verbose_inline) printf("--------------\n");
		steps++;
		if (delay) nanosleep(&req, &rem);

	}

	if (verbose_inline || (flag_verbose && !state_ptr)) printf("--------------\n");
	if (state_ptr && state_ptr->final) res = 1;	
	fprintf(stderr, "STEPS: %zu\n", steps);

	return res;
}
*/

// Generic automaton run function
// Takes an automaton and an input string
//
// Returns int: 1 on accept, 0 on reject
//int Automaton_run(struct Automaton *a0, char *input)
int Automaton_run(struct Automaton *a0, struct Tape *input_tape)
{
	struct timespec req, rem;
	if (delay) {
		req = nsleep_calc(delay);
	}

	struct Stack empty_stacks = Stack_init(CELL);

	struct State *state_ptr = a0->start;

	CELL_TYPE input_sym = 0;
	size_t steps = 0;
	int res = 0;
	int newlines = 0;
	size_t origsize = input_tape->tape.size;
	struct Trans *trans_ptr = NULL;

	while (origsize-- || is_tm) {

		if (is_tm && state_ptr->final) break;

		if (verbose_inline) {
			clear_n(newlines);
			newlines = 0;
		}

		input_sym = Tape_head(input_tape);

		XXH64_hash_t offset = a0->trans.hashmax;
		trans_ptr = Trans_get(a0, state_ptr, input_sym, &offset, 0);		

		if (trans_ptr) {	
			if (is_pda) {
				if (trans_ptr->pop) {
					CELL_TYPE *peek = Stack_peek(&empty_stacks);
					if (peek && *peek == trans_ptr->popsym) {
						Stack_pop(&empty_stacks);
						if (trans_ptr->push) {
							Stack_push(&empty_stacks, &trans_ptr->pushsym);
						}

						if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
						if (is_tm) Tape_pos(input_tape, trans_ptr->dir);
						else Tape_pos(input_tape, 2);

						if (!trans_ptr->dstate->reject) {
							if (flag_verbose) {
								Trans_print(trans_ptr); putchar('\n');
								/*printf("%s: ", state_ptr->name);
								Trans_sym_print(input_sym);
								printf(" > %s\n", trans_ptr->dstate->name);*/
								newlines++;

								if (is_tm) {
									printf("   tape: ");
									Tape_print(input_tape);
									putchar('\n');
									newlines++;
								}
								if (is_pda) {
									printf("  stack: ");
									Stack_print(&empty_stacks);
									putchar('\n');
									newlines++;
								}
							}
							state_ptr = trans_ptr->dstate;
						} else {
							if (flag_verbose) {
								Trans_print(trans_ptr); putchar('\n');
								/*printf("%s: ", state_ptr->name);
								Trans_sym_print(input_sym);
								printf(" > \n");*/
								newlines++;

								if (is_tm) {
									printf("   tape: ");
									Tape_print(input_tape);
									putchar('\n');
									newlines++;
								}
								if (is_pda) {
									printf("  stack: ");
									Stack_print(&empty_stacks);
									putchar('\n');
									newlines++;
								}

							}
							state_ptr = NULL;
							break;
						}	
					} else { 
						if (flag_verbose) {
							Trans_print(trans_ptr); putchar('\n');
							/*printf("%s: ", state_ptr->name);
							Trans_sym_print(input_sym);
							printf(" > \n");*/
							newlines++;

							if (is_tm) {
								printf("   tape: ");
								Tape_print(input_tape);
								putchar('\n');
								newlines++;
							}
							if (is_pda) {
								printf("  stack: ");
								Stack_print(&empty_stacks);
								putchar('\n');
								newlines++;
							}

						}
						state_ptr = NULL;
						break;
					}
				} else if (trans_ptr->push) {
					Stack_push(&empty_stacks, &trans_ptr->pushsym);
					if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
					if (is_tm) Tape_pos(input_tape, trans_ptr->dir);
					else Tape_pos(input_tape, 2);

					if (!trans_ptr->dstate->reject) {
						if (flag_verbose) {
							Trans_print(trans_ptr); putchar('\n');
							/*printf("%s: ", state_ptr->name);
							Trans_sym_print(input_sym);
							printf(" > %s\n", trans_ptr->dstate->name);*/
							newlines++;

							if (is_tm) {
								printf("   tape: ");
								Tape_print(input_tape);
								putchar('\n');
								newlines++;
							}
							if (is_pda) {
								printf("  stack: ");
								Stack_print(&empty_stacks);
								putchar('\n');
								newlines++;
							}

						}
						state_ptr = trans_ptr->dstate;
					} else { 
						if (flag_verbose) {
							Trans_print(trans_ptr); putchar('\n');
							/*printf("%s: ", state_ptr->name);
							Trans_sym_print(input_sym);
							printf(" > \n");*/
							newlines++;

							if (is_tm) {
								printf("   tape: ");
								Tape_print(input_tape);
								putchar('\n');
								newlines++;
							}
							if (is_pda) {
								printf("  stack: ");
								Stack_print(&empty_stacks);
								putchar('\n');
								newlines++;
							}

						}
						state_ptr = NULL;
						break; 
					}	
				} else { 
					if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
					if (is_tm) Tape_pos(input_tape, trans_ptr->dir);
					else Tape_pos(input_tape, 2);

					if (!trans_ptr->dstate->reject) {
						if (flag_verbose) {
							Trans_print(trans_ptr); putchar('\n');
							/*printf("%s: ", state_ptr->name);
							Trans_sym_print(input_sym);
							printf(" > %s\n", trans_ptr->dstate->name);*/
							newlines++;

							if (is_tm) {
								printf("   tape: ");
								Tape_print(input_tape);
								putchar('\n');
								newlines++;
							}
							if (is_pda) {
								printf("  stack: ");
								Stack_print(&empty_stacks);
								putchar('\n');
								newlines++;
							}

						}
						state_ptr = trans_ptr->dstate;
					} else { 
						if (flag_verbose) {
							Trans_print(trans_ptr); putchar('\n');
							/*printf("%s: ", state_ptr->name);
							Trans_sym_print(input_sym);
							printf(" >\n");*/
							newlines++;

							if (is_tm) {
								printf("   tape: ");
								Tape_print(input_tape);
								putchar('\n');
								newlines++;
							}
							if (is_pda) {
								printf("  stack: ");
								Stack_print(&empty_stacks);
								putchar('\n');
								newlines++;
							}
						}
						state_ptr = NULL;
						break; 
					}
				}
				// NOT PDA
			} else {
				if (trans_ptr->write) Tape_write(input_tape, &trans_ptr->writesym);
				if (is_tm) Tape_pos(input_tape, trans_ptr->dir);
				else Tape_pos(input_tape, 2);

				if (!trans_ptr->dstate->reject) {
					if (flag_verbose) {
						Trans_print(trans_ptr); putchar('\n');
						/*printf("%s: ", state_ptr->name);
						Trans_sym_print(input_sym);
						printf(" > %s\n", trans_ptr->dstate->name);*/
						newlines++;

						if (is_tm) {
							printf("   tape: ");
							Tape_print(input_tape);
							putchar('\n');
							newlines++;
						}
						if (is_pda) {
							printf("  stack: ");
							Stack_print(&empty_stacks);
							putchar('\n');
							newlines++;
						}

					}
					state_ptr = trans_ptr->dstate;
				} else { 
					if (flag_verbose) {
						Trans_print(trans_ptr); putchar('\n');
						/*printf("%s: ", state_ptr->name);
						Trans_sym_print(input_sym);
						printf(" >\n");*/
						newlines++;

						if (is_tm) {
							printf("   tape: ");
							Tape_print(input_tape);
							putchar('\n');
							newlines++;
						}
						if (is_pda) {
							printf("  stack: ");
							Stack_print(&empty_stacks);
							putchar('\n');
							newlines++;
						}
					}
					state_ptr = NULL;
					break; 
				}
			}
			// NO TRANS EXISTS
		} else { 
			if (flag_verbose) {
				printf("%s: ", state_ptr->name);
				Trans_sym_print(input_sym);
				printf(" >\n");
				newlines++;

				if (is_tm) {
					printf("   tape: ");
					Tape_print(input_tape);
					putchar('\n');
					newlines++;
				}
				if (is_pda) {
					printf("  stack: ");
					Stack_print(&empty_stacks);
					putchar('\n');
					newlines++;
				}
			}

			state_ptr = NULL;
			break;
		}

		if (flag_verbose && !verbose_inline) printf("--------------\n");
		steps++;
		if (delay) nanosleep(&req, &rem);

	}

	if (verbose_inline || (flag_verbose && !state_ptr)) printf("--------------\n");
	if (state_ptr && state_ptr->final) res = 1;	
	fprintf(stderr, "STEPS: %zu\n", steps);

	return res;
}
