#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "auto.h"
#include "regex.h"

int flag_verbose = 0;

struct State *State_create(char *name)
{
	struct State *state = malloc(sizeof(struct State));
	if (state == NULL) {
		fprintf(stderr, "Error allocating memory for state\n");
		exit(EXIT_FAILURE);
	}
	state->num_trans = 0;
	state->max_trans = 2;
	state->trans = malloc(sizeof(struct Transition *) * state->max_trans);
	if (state->trans == NULL) {
		fprintf(stderr, "Error allocating memory for trans array within state\n");
		exit(EXIT_FAILURE);
	}
	state->name = strdup(name);
	state->start = 0;
	state->final = 0;
	return state;
}

struct Automaton *Automaton_create()
{
	struct Automaton *automaton = malloc(sizeof(struct Automaton));
	if (automaton == NULL) {
		fprintf(stderr, "Error allocating memory for automaton\n");
		exit(EXIT_FAILURE);
	}
	automaton->len = 0;
	automaton->max_len = 2;
	automaton->states = malloc(sizeof(struct State *) * automaton->max_len);
	if (automaton->states == NULL) {
		fprintf(stderr, "Error allocating memory for states array in automaton\n");
		exit(EXIT_FAILURE);
	}
	return automaton;
}

struct State* get_state(struct Automaton *automaton, char *name)
{
	for (int i = 0; i < automaton->len; i++) {
		if (strcmp(automaton->states[i]->name, name) == 0) {
			return automaton->states[i];
		}
	}
	return NULL;
}

void State_name_add(struct Automaton *automaton, char *name)
{
	struct State *test = get_state(automaton, name);
	if (test == NULL) {
		struct State *new_state = State_create(name);
		automaton->len++;
		if (automaton->len > automaton->max_len) { 
			automaton->max_len *= 2;
			automaton->states = realloc(automaton->states, sizeof(struct State *) * automaton->max_len);
			if (automaton->states == NULL) {
				fprintf(stderr, "Memory error adding state name to automaton\n");
				exit(EXIT_FAILURE);
			}
		}
		automaton->states[automaton->len-1] = new_state;

	}
}

void State_add(struct Automaton *automaton, struct State *state)
{
	for (int i = 0; i < automaton->len; i++) {
		if (automaton->states[i] == state) return;
	}
	automaton->len++;
	if (automaton->len > automaton->max_len) {
		automaton->max_len *= 2;
		automaton->states = realloc(automaton->states, sizeof(struct State *) * automaton->max_len);
		if (automaton->states == NULL) {
			fprintf(stderr, "Memory error adding state name to automaton\n");
			exit(EXIT_FAILURE);
		}
	}
	automaton->states[automaton->len-1] = state;
}

struct Transition *Transition_create(char symbol, struct State *state)
{
	struct Transition *trans = malloc(sizeof(struct Transition));
	if (trans == NULL) {
		fprintf(stderr, "Memory error creating transition\n");
		exit(EXIT_FAILURE);
	}
	trans->symbol = symbol;
	trans->state = state;
	return trans;
}

void Transition_add(struct State *state, struct Transition *trans)
{
	state->num_trans++;
	if (state->num_trans > state->max_trans) {
		state->max_trans *= 2;
		state->trans = realloc(state->trans, sizeof(struct Trans *) * state->max_trans);
		if (state->trans == NULL) {
			fprintf(stderr, "Memory error adding state transition\n");
			exit(EXIT_FAILURE);
		}
	}
	state->trans[state->num_trans-1] = trans;
}

void State_destroy(struct State *state)
{
	for (int i = 0; i < state->num_trans; i++) {
		free(state->trans[i]);
	}
	free(state->name);
	free(state->trans);
	free(state);
}

// Destroy automaton, states, and transitions
void Automaton_destroy(struct Automaton *automaton)
{
	for (int i = 0; i < automaton->len; i++) {
		State_destroy(automaton->states[i]);
	}
	free(automaton->states);
	free(automaton);
}

// Destroy automaton struct only
void Automaton_clear(struct Automaton *automaton)
{
	free(automaton->states);
	free(automaton);
}

void State_print(struct State *state)
{
	printf("%s: ", state->name);
	if (state->num_trans != 0) {
		for (int i = 0; i < state->num_trans-1; i++) {
			if (isspace(state->trans[i]->symbol))
				printf("'%c'>%s, ", state->trans[i]->symbol, state->trans[i]->state->name);
			else
				printf("%c>%s, ", state->trans[i]->symbol, state->trans[i]->state->name);
		}
		if (isspace(state->trans[state->num_trans-1]->symbol))
			printf("'%c'>%s", state->trans[state->num_trans-1]->symbol, state->trans[state->num_trans-1]->state->name);
		else
			printf("%c>%s", state->trans[state->num_trans-1]->symbol, state->trans[state->num_trans-1]->state->name);
	}
	if (state->final) printf(" (F)");
	if (state->start) printf(" (S)");

	printf("\n");
}

void Automaton_print(struct Automaton *automaton)
{
	for (int i = 0; i < automaton->len; i++) {
		State_print(automaton->states[i]);
	}
}

int isnamechar(char c)
{
	char *syms="~`!@$%^&*()-_+=[{}]\\|\"<./?";
	int res = 0;
	for (int i = 0; syms[i] != '\0'; i++) {
		if (c == syms[i]) {
			res = 1;
			break;
		}
	}
	return res || isalnum(c);

}

struct Automaton *Automaton_import(char *filename) 
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	fp = fopen(filename, "r");
	if (fp == NULL) exit(EXIT_FAILURE);

	char name[STATE_NAME_MAX], 
		 state[STATE_NAME_MAX],
		 start[STATE_NAME_MAX],
		 final[STATE_NAME_MAX * 100];
	char symbol;
	char alphabet[100];
	int linenum=1;

	struct Automaton *automaton = Automaton_create();
	while ((read = getline(&line, &len, fp)) != -1) {
			int i = 0;
			int j = 0;
			int spaces = 0;
			int mystate=1;
			for (i = 0; line[i] != '\0'; i++) {

				switch (mystate) {
					// Ensure state name is at least one char
					case 1:
						if(isnamechar(line[i])) {
							mystate = 2;
						} else if (line[i] == '#' || line[i] == '\n') {
							mystate = 7;	
						} else if (isspace(line[i])) {
							mystate = 11;
						} else mystate = 8;
						break;;
					// Finish acquiring state name string
					case 2:
						if(isnamechar(line[i])) {
							mystate = 2;
						} else if (line[i] == ':') {
							*name = '\0';
							strncat(name, line+j, i-j);
							
							if (!strcmp(name, "start")) {
								j = i + 1;
								mystate = 20;
							} else if (!strcmp(name, "final")) {
								j = i + 1;
								mystate = 21;
							} else {
								State_name_add(automaton, name);
								mystate = 3;
							}
						} else if (isspace(line[i])) {
							mystate = 12;
						} else mystate = 8;
						break;;
					// Begin parsing state transitions
					case 3:
						if (isnamechar(line[i])) {
							symbol = line[i];
							mystate = 4;
						// For empty string transitions
						} else if (line[i] == '>') {
							j = i + 1;
							symbol = '\0';
							mystate = 5;
						} else if (line[i] == '#') {
							mystate = 7;
						} else if (line[i] == '\'') {
							mystate = 9;
						} else if (isspace(line[i])) {
							mystate = 13;
						}
						break;;
					// Set symbol for state transition
					case 4:
						if (line[i] == '>') {
							j = i + 1;
							mystate = 5;
						} else if (isspace(line[i])) {
							mystate = 14;
						} else mystate = 8;
						break;;
					// Ensure destination state in transition consists of at least one char
					case 5:
						if (isnamechar(line[i])) {
							j = i;
							mystate = 6;
						} else if (isspace(line[i])) {
							mystate = 15;
						} else mystate = 8;
						break;;
					// Finish acquiring state name string for the transition
					case 6:
						if (isnamechar(line[i])) {
							mystate = 6;
						} else if (line[i] == ';') {
							*state = '\0';
							strncat(state, line+j, i-j);

							State_name_add(automaton, state);
							struct State *from = get_state(automaton, name);
							struct State *to = get_state(automaton, state);
							struct Transition *new_trans = Transition_create(symbol, to);
							Transition_add(from, new_trans);

							mystate = 3;
						// Allow multiple state transitions per symbol
						} else if (line[i] == ',') {
							*state = '\0';
							strncat(state, line+j, i-j);
							j=i+1;

							State_name_add(automaton, state);
							struct State *from = get_state(automaton, name);
							struct State *to = get_state(automaton, state);
							struct Transition *new_trans = Transition_create(symbol, to);
							Transition_add(from, new_trans);

							mystate = 5;
						} else if (isspace(line[i])) {
							mystate = 16;
						} else mystate=8;
						break;;
					// state 7 is an accept state for comments
					// Error
					case 8:
						fprintf(stderr,"Error on line %d\n", linenum);
						free(line);
						fclose(fp);
						Automaton_destroy(automaton);
						exit(EXIT_FAILURE);
					// char specified by single quotes, ie. 'a',' '
					case 9:
						symbol = line[i];
						mystate = 10;
						break;
					// end single quote
					case 10:
						if (line[i] == '\'') {
							mystate = 4;
						} else
							mystate = 8;
						break;
					// space case for state 1
					case 11:
						if (isspace(line[i]))
							mystate = 11;
						else if (isnamechar(line[i])) {
							j = i;
							mystate = 2;
						} else if (line[i] == '#' || line[i] == '\n')
							mystate = 7;
						break;
					// space case for state 2
					case 12:
						spaces++;
						if (isspace(line[i]))
							mystate = 12;
						else if (line[i] == ':') {
							*name = '\0';
							strncat(name, line+j, i-j-spaces);
							
							if (!strcmp(name, "start")) {
								mystate = 20;
								j = i + 1;
							} else if (!strcmp(name, "final")) {
								mystate = 21;
								j = i + 1;
							} else {
								State_name_add(automaton, name);
								mystate = 3;
							}
							spaces = 0;
						} else 
							mystate = 8;
						break;
					// space case for state 3
					case 13:
						spaces++;
						if (isspace(line[i]))
							mystate = 13;
						else if (line[i] == '\'') {
							spaces = 0;
							mystate = 9;
						} else if (isnamechar(line[i])) {
							symbol = line[i];
							spaces = 0;
							mystate = 4;
						} else if (line[i] == '>') {
							j = i + 1;
							symbol = '\0';
							spaces = 0;
							mystate = 5;
						} else if (line[i] == '#')
							mystate = 7;
						else
							mystate = 8;
						break;
					// space case for state 4
					case 14:
						spaces++;
						if (isspace(line[i]))
							mystate = 14;
						else if (line[i] == '>') {
							j = i + 1;
							mystate = 5;
							spaces = 0;
						} else
							mystate = 8;
						break;
					// space case for state 5
					case 15:
						spaces++;
						if (isspace(line[i]))
							mystate = 15;
						else if (isnamechar(line[i])) {
							j = i;
							spaces = 0;
							mystate = 6;
						} else 
							mystate = 8;
						break;
					// space case for state 6
					case 16:
						spaces++;
						if (isspace(line[i]))
							mystate = 16;
						else if (line[i] == ',') {
							*state = '\0';
							strncat(state, line+j, i-j-spaces);
							j = i + 1;

							State_name_add(automaton, state);
							struct State *from = get_state(automaton, name);
							struct State *to = get_state(automaton, state);
							struct Transition *new_trans = Transition_create(symbol, to);
							Transition_add(from, new_trans);
							
							spaces = 0;
							mystate = 5;
						} else if (line[i] == ';') {
							*state = '\0';
							strncat(state, line+j, i-j-spaces);

							State_name_add(automaton, state);
							struct State *from = get_state(automaton, name);
							struct State *to = get_state(automaton, state);
							struct Transition *new_trans = Transition_create(symbol, to);
							Transition_add(from, new_trans);
							
							spaces = 0;							
							mystate = 3;
						}
						else 
							mystate = 8;
						break;
					// line represents the start state
					case 20:
						if (line[i] == ';') {
							*start = '\0';
							strncat(start, line+j, i-j);
							remove_spaces(start);
						} else mystate = 20;
						break;
					// line represents the final state(s)
					case 21:
						if (line[i] == ';' ) {
							*final = '\0';
							strncat(final, line+j, i-j);
							remove_spaces(final);
						} else mystate = 21;
				}
				
				if (mystate == 7) break;
			}
			
			if (mystate != 3 && mystate !=13 && mystate != 20 && mystate != 21 && mystate != 7) {
				fprintf(stderr, "Error on line %d\n", linenum);
				free(line);
				fclose(fp);
				Automaton_destroy(automaton);
				exit(EXIT_FAILURE);
			}
			
	linenum++;
	}

	free(line);
	
	// Set final states
	char *s;
	s = strtok(final, ",");
	while (s != NULL) {
		struct State *f = get_state(automaton, s);
		if (f == NULL) {
			fprintf(stderr,"No final state %s detected\n", s);
			Automaton_destroy(automaton);
			exit(EXIT_FAILURE);
		}
		f->final = 1;
		s = strtok(NULL, ",");
	}

	// Set start state
	struct State *f = get_state(automaton, start);
	if (f == NULL) {
		fprintf(stderr, "No start state %s detected\n", start);
		Automaton_destroy(automaton);
		exit(EXIT_FAILURE);
	} else {
		f->start = 1;
		automaton->start = f;
	}
	
	fclose(fp);
	return automaton;
}

int isDFA(struct Automaton *automaton)
{
	char alphabet[256];
	*alphabet = '\0';
	for (int i = 0; i < automaton->len; i++) {
		struct State *state = automaton->states[i];
		for (int j = 0; j < state->num_trans; j++) {
			if (state->trans[j]->symbol == '\0') return 0;
			if (strchr(alphabet, state->trans[j]->symbol) == NULL)
				strncat(alphabet, &state->trans[j]->symbol, 1);

		}
	}

	for (int i = 0; i < automaton->len; i++) {
		struct State *state = automaton->states[i];
		if (strlen(alphabet) == state->num_trans) {
			for (int j = 0; alphabet[j] != '\0'; j++) {
			//for (int j = 0; j < strlen(alphabet); j++) {
				int num_chars = 0;
				for (int k = 0; k < state->num_trans; k++) {
					if (state->trans[k]->symbol == alphabet[j])
						num_chars += 1;
					if (num_chars > 1) return 0;
				}
				if (num_chars == 0) return 0;
			}
		} else return 0;
	}
	return 1;

}

int DFA_run(struct Automaton *automaton, char *input)
{
	//if (flag_verbose) Automaton_print(automaton);

	struct State *state = automaton->start;
	for (int i = 0; input[i] != '\0'; i++) {
		if (flag_verbose) printf("[%c]%s:\n", input[i], input+i+1);
		int none = 1;
		for (int j = 0; j < state->num_trans; j++) {
			if (input[i] == state->trans[j]->symbol) {
				none = 0;
				if (flag_verbose) {
					printf("\t%s > %s", state->name, state->trans[j]->state->name);
					if (state->trans[j]->state->final) printf(" (F)\n"); else printf("\n");
				}
				state = state->trans[j]->state;
				break;
			}
		}
		if (none) {
			printf("==>%s\n\tREJECTED\n", input);
			return 0;
		}
	}
	if (state->final) {
		printf("==>%s\n\tACCEPTED\n", input);
		return 1;
	} else {
		printf("==>%s\n\tREJECTED\n", input);
		return 0;
	}
}

int Automaton_run(struct Automaton *automaton, char *input)
{
	//if (flag_verbose) Automaton_print(automaton);
	
	struct Automaton *next = Automaton_create();
	struct Automaton *current = Automaton_create();
	State_add(current, automaton->start);
	
	// Take care of empty string transitions on null input	
	if (input[0] == '\0') {
		if (flag_verbose) printf("[]:\n");
		for (int j = 0; j < current->len; j++) {
			struct State *state = current->states[j];
			for (int k = 0; k < state->num_trans; k++) {
				if (state->trans[k]->symbol == '\0') {
					State_add(current, state->trans[k]->state);
					if (flag_verbose) {
						printf("\t%s > %s", state->name, state->trans[k]->state->name);
						if (state->trans[k]->state->final) printf(" (F)\n"); else printf("\n");
					}

				}
			}
		}
	}
	
	// Iterate through each input char
	for (int i = 0; input[i] != '\0'; i++) {
		if (flag_verbose) printf("[%c]%s:\n", input[i], input+i+1);
		for (int j = 0; j < current->len; j++) {
			struct State *state = current->states[j];
			for (int k = 0; k < state->num_trans; k++) {
				// Empty string match
				if (state->trans[k]->symbol == '\0') {
					State_add(current, state->trans[k]->state);
				}
				// Input char match 
				if (state->trans[k]->symbol == input[i]) {
					State_add(next, state->trans[k]->state);
					if (flag_verbose) {
						printf("\t%s > %s", state->name, state->trans[k]->state->name);
						if (state->trans[k]->state->final) printf(" (F)\n"); else printf("\n");
					}
				}
			}
		}
		
		// Prep next array with empty string transitions
		for (int j = 0; j < next->len; j++) {
			struct State *state = next->states[j];
			for (int k = 0; k < state->num_trans; k++) {
				if (state->trans[k]->symbol == '\0') {
					State_add(next, state->trans[k]->state);
					if (flag_verbose) {
						printf("\t%s > %s", state->name, state->trans[k]->state->name);
						if (state->trans[k]->state->final) printf(" (F)\n"); else printf("\n");
					}
				}
			}
		}
			
		Automaton_clear(current);
		current = next;
		if (current->len == 0) {
			printf("=>%s\n\tREJECTED\n",input);
			Automaton_clear(current);
			return 1;
		}
		next = Automaton_create();
	}
	
	Automaton_clear(next);
	for (int i = 0; i < current->len; i++) {
		if (current->states[i]->final) { 
			printf("=>%s\n\tACCEPTED\n",input);
			Automaton_clear(current);
			return 0;
		}
	}
	Automaton_clear(current);
	printf("=>%s\n\tREJECTED\n",input);
	return 1;

}

void Automaton_run_file(struct Automaton *automaton, char *input_string_file)
{
	FILE *input_string_fp;
	char *input_string = NULL;
	size_t len = 0;
	ssize_t read;

	input_string_fp = fopen(input_string_file, "r");
	if (input_string_fp == NULL) {
		fprintf(stderr, "Error opening %s\n", input_string_file);
		exit(EXIT_FAILURE);
	}
	
	int is_dfa = isDFA(automaton);
	while ((read = getline(&input_string, &len, input_string_fp)) != -1)
	{
		input_string[strcspn(input_string, "\r\n")] = 0;
		if (is_dfa)
			DFA_run(automaton, input_string);
		else
			Automaton_run(automaton, input_string);
	}
}
