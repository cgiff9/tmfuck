#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "auto.h"
#include "regex.h"
#include "ops.h"
#include "stack.h"

int main(int argc, char **argv)
{
	char *input_string_file = NULL;
	char *machine_file = NULL;
	char *input_string = NULL;
	char *regex = NULL;

	int deterministic = 0;
	int minimize = 0;

	int opt;
	int nonopt_index = 0;
	while ((opt = getopt (argc, argv, "-:vf:r:dm")) != -1)
	{
		switch (opt)
		{
			case 'v':
				flag_verbose = 1;
				break;
			case 'f':
				input_string_file = optarg;
				break;
			case 'r':
				regex = optarg;
				break;
			case 'd':
				deterministic = 1;
				break;
			case 'm':
				minimize = 1;
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

	if (!input_string && !input_string_file) {
		fprintf(stderr, "No input string supplied\n");
		exit(EXIT_FAILURE);
	}
		
	//Automaton_print(a0);
	struct Automaton *a0;
	if (regex) {
		if (machine_file)
			a0 = Automaton_import(machine_file);
		else
			a0 = regex_to_nfa(regex);
	} else if (machine_file)
		a0 = Automaton_import(machine_file);

	// 0 for NFA
	// 1 for DFA
	// 2 for PDA
	int machine_code = isDFA(a0);
	
	if (deterministic) {
		if (machine_code == 0) {
			if (flag_verbose) {
				printf("[ NFA: ]\n");
				Automaton_print(a0);
				printf("[ CONVERTED TO DFA: ]\n");
			}
			struct Automaton *a1 = nfa_to_dfa(a0);
			struct Automaton *a2 = DFA_minimize(a1);
			Automaton_destroy(a0);
			Automaton_destroy(a1);
			a0 = a2;
		} else if (machine_code == 1) {
			if (flag_verbose) {
				printf("[ DFA: ]\n");
				Automaton_print(a0);
				printf("[ MINIMIZED TO DFA: ]\n");
			}
			struct Automaton *a1 = DFA_minimize(a0);
			Automaton_destroy(a0);
			a0 = a1;
		}
	}
	
	if (minimize && !deterministic) {
		if (machine_code == 1) {
			if (flag_verbose) {
				printf("[ DFA: ]\n");
				Automaton_print(a0);
				printf("[ MINIMIZED TO DFA: ]\n");
			}
			struct Automaton *a1 = DFA_minimize(a0);
			Automaton_destroy(a0);
			a0 = a1;
		}
	}
	
	if (input_string_file) {
		if (input_string) {
			if (machine_code == 1) {
				if (flag_verbose) Automaton_print(a0);
				DFA_run(a0, input_string);
			} else { 
				if (flag_verbose) Automaton_print(a0);
				Automaton_run(a0, input_string);
			}
		} else {
			if (flag_verbose) Automaton_print(a0);
			Automaton_run_file(a0, input_string_file);
		}
	} else if (input_string) {
		if (machine_code == 1) {
			if (flag_verbose) Automaton_print(a0);
			DFA_run(a0, input_string);
		} else {
			if (flag_verbose) Automaton_print(a0);
			Automaton_run(a0, input_string);
		}
	}

	Automaton_destroy(a0);
}
