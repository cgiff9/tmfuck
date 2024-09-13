#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include <ctype.h>
#include <xxhash.h>

#include <time.h>
#include <math.h>
#include <inttypes.h>

#include "auto.h"
#include "file.h"
#include "machine.h"
#include "regex.h"
#include "tape.h"

#include "block.h"
#include "stack.h"
#include "var.h"
#include "file.h"

CELL_TYPE CELL_MIN;
CELL_TYPE CELL_MAX;

#define DIM(x) (sizeof(x)/sizeof(*(x)))

static const char     *sizes[]   = { "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" };
static const uint64_t  exbibytes = 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;

void *calculateSize(uint64_t size, char *result)
{
	//char     *result = (char *) malloc(sizeof(char) * 20);
	uint64_t  multiplier = exbibytes;
	long unsigned int i;

	for (i = 0; i < DIM(sizes); i++, multiplier /= 1024)
	{
		if (size < multiplier)
			continue;
		if (size % multiplier == 0)
			sprintf(result, "%" PRIu64 " %s", size / multiplier, sizes[i]);
		else
			sprintf(result, "%.1f %s", (float) size / multiplier, sizes[i]);
		return result;
	}
	strcpy(result, "0");
	return result;
}

int main(int argc, char **argv)
{
	CELL_MAX = maxof(CELL_TYPE);
	CELL_MIN = (issigned(CELL_TYPE)) ? -CELL_MAX-1 : 0;

	char *input_string_file = NULL;
	char *machine_file = NULL;
	char *input_string = NULL;
	char *regex = NULL;
	char *delim_string = NULL;

	//int deterministic = 0;
	//int minimize = 0;
	//int config_neat = 0;
	int config_only = 0;
	int nd_engine = 0;
	
	struct Stack delims_tmp = Stack_init(CELL);

	int opt;
	int nonopt_index = 0;
	while ((opt = getopt (argc, argv, "-:vpxcnzw:t:i:r:d:ms:")) != -1)
	{
		switch (opt)
		{
			case 'v':
				flag_verbose++;
				break;
			case 'p':
				verbose_inline = 1;
				flag_verbose++;
				break;
			case 'x':
				execute = 1;
				break;
			case 'c':
				config_only++;
				break;
			case 'n':
				nd_engine = 1;
				break;
			case 'z':
				debug++;
				break;
			case 'w':
				tape_print_width = atol(optarg);
				break;
			case 't':
				initial_tape_size = atol(optarg);
				break;
			case 'i':
				input_string_file = optarg;
				break;
			case 'r':
				regex = optarg;
				break;
			case 'd':
				CELL_TYPE delim_char = -1;
				delim_string = optarg;
				if (!strcmp(delim_string, "whitespace") ||
						!strcmp(delim_string, "white") ||
						!strcmp(delim_string, "space") ||
						!strcmp(delim_string, "isspace") ||
						!strcmp(delim_string, "ws")) {
					delim_char = 9;
					Stack_push(&delims_tmp, &delim_char);
					delim_char = 10;
					Stack_push(&delims_tmp, &delim_char);
					delim_char = 11;
					Stack_push(&delims_tmp, &delim_char);
					delim_char = 12;
					Stack_push(&delims_tmp, &delim_char);
					delim_char = 13;
					Stack_push(&delims_tmp, &delim_char);
					delim_char = 32;
					Stack_push(&delims_tmp, &delim_char);
					break;
				}

				int delim_len = strlen(delim_string);
				if (delim_len == 1) {
					delim_char = delim_string[0];
					Stack_push(&delims_tmp, &delim_char);
				} else if (delim_len < 4) { // longest ASCII char string: 126 (3 chars)
					char *endptr;
					int i = 0;
					if (delim_string[0] == '-') i = 1;
					for (i = i; i < delim_len; i++) {
						if (!isdigit(delim_string[i])) {
							fprintf(stderr, "Error: input string/tape delimiter must be single ASCII char or a decimal value representing one.\n");
							exit(EXIT_FAILURE);
						}
					}
					if (sign) 
						delim_char = strtoimax(delim_string, &endptr, 10);
					else
						delim_char = strtoumax(delim_string, &endptr, 10);	

					if (!(delim_char > 32 && delim_char < 127) && !isspace(delim_char)) {
						fprintf(stderr, "Error: input string/tape delimiter must be printable ASCII or whitespace.\n");
						exit(EXIT_FAILURE);
					}
					Stack_push(&delims_tmp, &delim_char);
				} else {
					fprintf(stderr, "Error: input string/tape delimiter must be printable ASCII or whitespace.\n");
					exit(EXIT_FAILURE);
				}
				
				//deterministic = 1;
				break;
			case 'm':
				//minimize = 1;
				break;
			case 's':
				delay = atof(optarg);
				//flag_verbose = 1;
				break;
			case '?':
				fprintf(stderr, "Unknown option '-%c'\n", optopt);
				exit(EXIT_FAILURE);
			case ':':
				fprintf(stderr, "Option -%c requires an argument\n", optopt);
				exit(EXIT_FAILURE);
			case 1:
				switch (nonopt_index)
				{
					case 0:
						machine_file = optarg;
						break;
					case 1:
						input_string = optarg;
						break;
				}
				nonopt_index++;
		}
	}

	// if only one nonopt_index with regex assume it's the string
	if (nonopt_index == 1 && regex) {
		input_string = machine_file;
		machine_file = NULL;
	}

	struct Tape input_tape;
	if (!input_string && !input_string_file && !config_only) {
		fprintf(stderr, "No input string supplied, assuming empty string\n");
		input_tape = Tape_init();
	}

/*	char buff[30];
	print_max = (sign) ? 
		snprintf(buff, 30, "%" SIGNED_PRINT_FMT, (SIGNED_PRINT_TYPE)CELL_MIN) :
		snprintf(buff, 30, "%" UNSIGNED_PRINT_FMT, (UNSIGNED_PRINT_TYPE)CELL_MAX);*/

	print_max = num_places((sign) ? CELL_MIN : CELL_MAX);

	//Automaton_print(a0);
	struct Automaton a0;
	clock_t begin = clock();
	if (regex) {
		if (machine_file)
			a0 = Automaton_import(machine_file);
		else {
			a0 = regex_to_nfa(regex);
		}
	} else if (machine_file) {
		a0 = Automaton_import(machine_file);
	}

	// Consolidate and sort tape delimiters
	if (delims_tmp.size) {
		qsort(delims_tmp.elem, delims_tmp.size, sizeof(CELL_TYPE), symcmp);
		CELL_TYPE prev_d = 0;
		for (unsigned int i = 0; i < delims_tmp.size; i++) {
			CELL_TYPE d = *(CELL_TYPE *)Stack_get(&delims_tmp, i);
			if (!i || d != prev_d) Stack_push(&a0.delims, &d);
			prev_d = d;
		}
		Stack_free(&delims_tmp);
	}

	// Detect and mark any epsilon cycles
	int nd = a0.epsilon + a0.strans;
	if (a0.epsilon) {
		epsilon_loop_detect(&a0);
		if (debug) {
			int first = 1;
			struct Stack *datastacks = a0.trans.data.elem;
			for (unsigned int i = 0; i < datastacks->size; i++) {	
				struct Stack *strip = &datastacks[i];
				struct Trans *strip_elem = strip->elem;
				for (unsigned int j = 0; j < strip->size; j++) {

					struct Trans *trans = &strip_elem[j];
					if (trans->epsilon_loop) {
						if (first) {
							//printf("Epsilon Loops:\n");
							first = 0;
						}
						//Trans_print(trans); putchar('\n');
					}
				}
			}
			//if (!first) printf("--------------\n");
		}
	}

	clock_t end = clock();
	double machine_import_time = (double)(end - begin) / CLOCKS_PER_SEC;

	if (config_only) {
		if (config_only > 1) Automaton_print(&a0, 1);
		else Automaton_print(&a0, 0);
		if (!input_string && !input_string_file) { 
			Automaton_free(&a0);
			return 0;
		}
	}

	begin = clock();
	if (input_string) {
		input_tape = Tape_import(&a0, input_string, 0);
	} else if (input_string_file) {
		input_tape = Tape_import(&a0, input_string_file, 1);
	}
	end = clock();
	double tape_import_time = (double)(end - begin) / CLOCKS_PER_SEC;

	if (config_only) printf("--------------\n");
	
	int res = 0;
	begin = clock();
	
	char *machine_type = NULL;

	// NONDETERMINISTIC FUNCTIONS
	if (nd_engine || nd) {
	
		// Turing machine
		if (is_tm) {
			if (is_pda) {
				if (nd) machine_type = "NTM with stack";
				else machine_type = "TM with stack";

				if (flag_verbose) res = NTM_stack_run_verbose(&a0, &input_tape);
				else res = NTM_stack_run(&a0, &input_tape);
			} else {
				if (nd) machine_type = "NTM";
				else machine_type = "TM";

				if (flag_verbose) res = NTM_run_verbose(&a0, &input_tape);
				else res = NTM_run(&a0, &input_tape);
			}
		
		// PDA
		} else if (is_pda) {
			if (nd) machine_type = "PDA";
			else machine_type = "DPDA";

			if (flag_verbose) res = PDA_run_verbose(&a0, &input_tape);
			else res = PDA_run(&a0, &input_tape);
		
		// NFA
		} else {
			if (nd) machine_type = "NFA";
			else machine_type = "DFA";

			if (flag_verbose) res = NFA_run_verbose(&a0, &input_tape);
			else res = NFA_run(&a0, &input_tape);
		}
	
	// DETERMINISTIC FUNCTIONS
	} else {
		
		// Turing machine
		if (is_tm) {
			if (is_pda) {
				machine_type = "TM with stack";
				if (flag_verbose) res = TM_stack_run_verbose(&a0, &input_tape);
				else res = TM_stack_run(&a0, &input_tape);
			} else {
				machine_type = "TM";
				if (flag_verbose) res = TM_run_verbose(&a0, &input_tape);
				else res = TM_run(&a0, &input_tape);
			}
	
		// DPDA
		} else if (is_pda) {
			machine_type = "DPDA";
			if (flag_verbose) res = DPDA_run_verbose(&a0, &input_tape);
			else res = DPDA_run(&a0, &input_tape);
		
		// DFA
		} else {
			machine_type = "DFA";
			if (flag_verbose) res = DFA_run_verbose(&a0, &input_tape);
			else res = DFA_run(&a0, &input_tape);
		}
	}

	end = clock();
	double run_time = (double)(end - begin) / CLOCKS_PER_SEC;
	fprintf(stderr, "%s\n", (res) ? "ACCEPTED" : "REJECTED");

	double sec;
	double msec = modf(run_time, &sec);
	if (debug) fprintf(stderr, "+----------[RUNTIME INFO]%s\n",
			"-------------------------------------------------------------+");
	fprintf(stderr, "%sMachine:   %s%s\n", 
			(debug) ? " " : "",
			(debug) ? "            " : "",
			machine_type);
	fprintf(stderr, "%sRuntime:   %s%ldm%ld.%06lds (%s engine)\n", 
			(debug) ? " " : "",
			(debug) ? "            " : "",
			(long int)sec / 60, (long int)sec % 60, (long int)(msec * 1000000),
			(nd_engine || nd) ? "nondeterministic" : "deterministic");

	size_t mem_obj = 0;
	if (debug) {
		if (machine_file) { 
			msec = modf(machine_import_time, &sec);
			fprintf(stderr, " Machine import time:   %ldm%ld.%06lds (%s)\n",
					(long int)sec / 60, (long int)sec % 60, (long int)(msec * 1000000),
					machine_file);
		}
	
		msec = modf(tape_import_time, &sec);
		fprintf(stderr, " Tape import time:      %ldm%ld.%06lds",
				(long int)sec / 60, (long int)sec % 60, (long int)(msec * 1000000));
		if (input_string_file) 
			fprintf(stderr, " (%s)\n", input_string_file);
		else 
			fprintf(stderr, "\n");

		fprintf(stderr, " Tape delimiters:       ");
		int spaces = 0;
		int symlen = 0;
		for (unsigned int i = 0; i < a0.delims.size; i++) {
			CELL_TYPE d = *(CELL_TYPE *)Stack_get(&a0.delims, i);
			if (symlen > 59) {
				fprintf(stderr, "\n                        ");
				symlen = 0;
			}
			
			if (d > 31 && d < 127) {
				if (isnamechar(d)) {
					symlen += fprintf(stderr, "%c", (char)d);
				} else {
					symlen += fprintf(stderr, "'%c'", (char)d);
				}
			} else if (sign) {
				symlen += fprintf(stderr, "%02" SIGNED_PRINT_FMT, (SIGNED_PRINT_TYPE)d);
			} else {
				symlen += fprintf(stderr, "%02" UNSIGNED_PRINT_FMT, (UNSIGNED_PRINT_TYPE)d);
			}
			if (isspace(d)) spaces++;
			if (spaces == 6) {
				symlen += fprintf(stderr, "(all whitespace)");
				spaces = 0;
			}
			if (i+1 != a0.delims.size) symlen += fprintf(stderr, ",");
		}
		if (a0.delims.size) fprintf(stderr, "\n");
		else fprintf(stderr, "none\n");

		char bytes_a[20];
		char bytes_b[20];

		fprintf(stderr, "+-----------[BLOCK INFO]%s\n", 
				"--------------------------------------------------------------+");
		
		calculateSize(a0.states.size * sizeof(struct State), bytes_a);
		calculateSize(a0.states.max * sizeof(struct State), bytes_b);
		fprintf(stderr, " states data load:    %10.6f%% | %10u / %-10u | %10s / %-10s\n", 
				a0.states.size / (float)a0.states.max * 100, 
				a0.states.size, a0.states.max,
				bytes_a, bytes_b);
		calculateSize(a0.states.size * sizeof(struct State *), bytes_a);
		calculateSize(a0.states.hashmax * sizeof(struct State *), bytes_b);
		fprintf(stderr, " states hash load:    %10.6f%% | %10u / %-10u | %10s / %-10s\n", 
				a0.states.size / (float)a0.states.hashmax * 100, 
				a0.states.size, a0.states.hashmax,
				bytes_a, bytes_b);
		fprintf(stderr, " states hash thresh:  %10.6f%% | %10u / %-10u |\n", 
				a0.states.max / (float)a0.states.hashmax * 100,
				a0.states.max, a0.states.hashmax);
		
		calculateSize(a0.trans.size * sizeof(struct Trans), bytes_a);
		calculateSize(a0.trans.max * sizeof(struct Trans), bytes_b);
		fprintf(stderr, " trans data load:     %10.6f%% | %10u / %-10u | %10s / %-10s\n", 
				a0.trans.size / (float)a0.trans.max * 100, 
				a0.trans.size, a0.trans.max,
				bytes_a, bytes_b);
		calculateSize(a0.trans.size * sizeof(struct Trans *), bytes_a);
		calculateSize(a0.trans.hashmax * sizeof(struct Trans *), bytes_b);
		fprintf(stderr, " trans hash load:     %10.6f%% | %10u / %-10u | %10s / %-10s\n", 
				a0.trans.size / (float)a0.trans.hashmax * 100,
				a0.trans.size, a0.trans.hashmax,
				bytes_a, bytes_b);
		fprintf(stderr, " trans hash thresh:   %10.6f%% | %10u / %-10u |\n", 
				a0.trans.max / (float)a0.trans.hashmax * 100,
				a0.trans.max, a0.trans.hashmax);
		
		calculateSize(a0.vars.size * sizeof(struct Var), bytes_a);
		calculateSize(a0.vars.max * sizeof(struct Var), bytes_b);
		fprintf(stderr, " vars data load:      %10.6f%% | %10u / %-10u | %10s / %-10s\n", 
				a0.vars.size / (float)a0.vars.max * 100, 
				a0.vars.size, a0.vars.max,
				bytes_a, bytes_b);
		calculateSize(a0.vars.size * sizeof(struct Var *), bytes_a);
		calculateSize(a0.vars.hashmax * sizeof(struct Var *), bytes_b);
		fprintf(stderr, " vars hash load:      %10.6f%% | %10u / %-10u | %10s / %-10s\n", 
				a0.vars.size / (float)a0.vars.hashmax * 100,
				a0.vars.size, a0.vars.hashmax,
				bytes_a, bytes_b);
		fprintf(stderr, " vars hash thresh:    %10.6f%% | %10u / %-10u |\n", 
				a0.vars.max / (float)a0.vars.hashmax * 100,
				a0.vars.max, a0.vars.hashmax);
		
		calculateSize(a0.names.size * sizeof(char), bytes_a);
		calculateSize(a0.names.max * sizeof(char), bytes_b);
		fprintf(stderr, " names data load:     %10.6f%% | %10u / %-10u | %10s / %-10s\n", 
				a0.names.size / (float)a0.names.max * 100,
				a0.names.size, a0.names.max,
				bytes_a, bytes_b);
		
		calculateSize(a0.stacks.size * sizeof(struct Stack), bytes_a);
		calculateSize(a0.stacks.max * sizeof(struct Stack), bytes_b);
		fprintf(stderr, " stacks data load:    %10.6f%% | %10u / %-10u | %10s / %-10s\n", 
				a0.stacks.size / (float)a0.stacks.max * 100,
				a0.stacks.size, a0.stacks.max,
				bytes_a, bytes_b);
		
		calculateSize(a0.tapes.size * sizeof(struct Tape), bytes_a);
		calculateSize(a0.tapes.max * sizeof(struct Tape), bytes_b);
		fprintf(stderr, " tapes data load:     %10.6f%% | %10u / %-10u | %10s / %-10s\n", 
				a0.tapes.size / (float)a0.tapes.max * 100,
				a0.tapes.size, a0.tapes.max,
				bytes_a, bytes_b);
		
		calculateSize(a0.states.size * sizeof(struct State) + 
				a0.states.hash.size * sizeof(struct State *) +
				a0.trans.size * sizeof(struct Trans) + 
				a0.trans.hash.size * sizeof(struct Trans *) +
				a0.vars.size * sizeof(struct Var) + 
				a0.vars.hash.size * sizeof(struct Var *) +
				a0.names.size * sizeof(char) + 
				a0.stacks.size * sizeof (struct Stack) + 
				a0.tapes.size * sizeof (struct Tape),
				bytes_a);
		calculateSize(mem_obj = a0.states.max * sizeof(struct State) + 
				a0.states.hashmax * sizeof(struct State *) +
				a0.trans.max * sizeof(struct Trans) + 
				a0.trans.hashmax * sizeof(struct Trans *) +
				a0.vars.max * sizeof (struct Var) + 
				a0.vars.hashmax * sizeof(struct Var *) +
				a0.names.max * sizeof(char) + 
				a0.stacks.max * sizeof(struct Stack) + 
				a0.tapes.max * sizeof (struct Tape),
				bytes_b);
		fprintf(stderr, " USED/ALLOC'D:%46s| %10s / %-10s\n", 
				"", bytes_a, bytes_b);

		/*
			fprintf(stderr, " states.count: %u\n", a0.states.count);
			fprintf(stderr, " trans.count: %u\n", a0.trans.count);
			fprintf(stderr, " vars.count: %u\n", a0.vars.count);
			fprintf(stderr, " states.countmax: %u\n", a0.states.countmax);
			fprintf(stderr, " trans.countmax: %u\n", a0.trans.countmax);
			fprintf(stderr, " vars.countmax: %u\n", a0.vars.countmax);


			fprintf(stderr, "names.count: %u\n", a0.names.count);
			fprintf(stderr, "names.countmax: %u\n", a0.names.countmax);
			*/

		fprintf(stderr, "+-----------[STRUCT INFO]%s\n",
				"-------------------------------------------------------------+");
		fprintf(stderr, " sizeof(struct Trans):      %zu B\n", sizeof(struct Trans));
		
		if (debug > 1) {
			fprintf(stderr, " sizeof(struct State):      %zu B\n", sizeof(struct State));
			fprintf(stderr, " sizeof(struct Automaton):  %zu B\n", sizeof(struct Automaton));
			fprintf(stderr, " sizeof(struct Block):      %zu B\n", sizeof(struct Block));
			fprintf(stderr, " sizeof(struct Stack):      %zu B\n", sizeof(struct Stack));
			fprintf(stderr, " sizeof(struct Tape):       %zu B\n", sizeof(struct Tape));
			fprintf(stderr, " sizeof(struct Var):        %zu B\n", sizeof(struct Var));
		}

		fprintf(stderr, " sizeof(CELL_%sTYPE):         %zu B\n", "", sizeof(CELL_TYPE));
		/*SHOW_DEFINE_FULL(CELL_TYPE);*/
		//fprintf(stderr, " CELL_%sTYPE:                 ", "");
		fprintf(stderr, " CELL_TYPE:                 ");
		SHOW_DEFINE_VALUE(CELL_TYPE);
		fprintf(stderr, " (defined in auto.h)\n");

		if (issigned(CELL_TYPE)) {
			fprintf(stderr, " CELL_MIN:                  %td\n", (ptrdiff_t)CELL_MIN);
		} else {
			fprintf(stderr, " CELL_MIN:                  %zu\n", (size_t)CELL_MIN);
		}
		fprintf(stderr, " CELL_MAX:                  %zu\n", (size_t)CELL_MAX);
		fprintf(stderr, " Longest possible symbol:   %d %s\n", 
				print_max, (print_max == 1) ? "char" : "chars");
		fprintf(stderr, " Longest input symbol:      %d %s\n", 
				longest_sym, (longest_sym == 1) ? "char" : "chars");
		fprintf(stderr, " Longest state name:        %d %s\n", 
				longest_name, (longest_name == 1) ? "char" : "chars");

	}


	begin = clock();

	size_t mem_tape = Tape_free(&input_tape);
	size_t mem_dyn = Automaton_free(&a0);
	
	end = clock();
	double free_time = (double)(end - begin) / CLOCKS_PER_SEC;

	if (debug) {
		fprintf(stderr, "+---------[DYNAMIC MEMORY]%s\n" ,
				"------------------------------------------------------------+");

		char bytes_format[20];
		calculateSize(mem_obj, bytes_format);
		fprintf(stderr, " Block storage:             %s\n", bytes_format);
		calculateSize(mem_dyn, bytes_format);
		fprintf(stderr, " Struct internal:           %s\n", bytes_format);
		calculateSize(run_memory, bytes_format);
		fprintf(stderr, " Runtime stacks:            %s\n", bytes_format);
		calculateSize(mem_tape, bytes_format);
		fprintf(stderr, " End initial tape size:     %s\n", bytes_format);
		calculateSize(mem_obj + mem_dyn + run_memory + mem_tape, bytes_format);
		fprintf(stderr, " Total alloc'd:             %s\n", bytes_format);
		msec = modf(free_time, &sec);
		fprintf(stderr, " Memory free time:          %ldm%ld.%06lds\n",
				(long int)sec / 60, (long int)sec % 60, (long int)(msec * 1000000));

	}

	return 0;
}
