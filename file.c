#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
/*#include <inttypes.h>*/

#include <limits.h>

#include "file.h"
#include "auto.h"
#include "stack.h"
#include "var.h"

#define SYMS_LEN 21
CELL_TYPE syms[] = { '!','"','%','&','*','+','-','.','/','<','?','@','[',']','^','_','`','{','|','}','~' };

int flag_verbose;// = 0;
int verbose_inline;
double delay = 0;
int execute = 0;
CELL_TYPE tm_blank = '_';
char tm_bound = '\0';
int tm_bound_halt = 0;
int is_tm = 0;
int is_pda = 0;
int sign = (issigned(CELL_TYPE));
int print_max = 1;
int debug = 0;
unsigned int longest_name = 0;
unsigned int longest_sym = 0;
unsigned int tape_print_width = TAPE_PRINT_MAX;

size_t initial_tape_size = 0;

int symcmp(const void *a, const void *b)
{
	return *(CELL_TYPE *)a - *(CELL_TYPE *)b;
}

int isnamechar(CELL_TYPE c)
{
	if (isalnum(c)) return 1;
	CELL_TYPE *exists = bsearch(&c, syms, SYMS_LEN, sizeof(CELL_TYPE), symcmp);
	if (exists) return 1;
	/*
	//for (unsigned int i = 0; syms[i] != '\0'; i++)
	for (unsigned int i = 0; i < SYMS_LEN; i++)
		if (c == syms[i])
			return 1; */
	return 0;
}

// Code adapted from paxdiablo's answer
// on StackOverflow:
//
// https://stackoverflow.com/a/1068937
//
// Adapted to work for 'generic' CELL_TYPE
//
// Returns number of characters in a 
// printed symbol
int num_places (CELL_TYPE n) 
{
	// Printable ASCII always has length 1
	if (n > 31 && n < 127) return 1;
	
	int neg = 0;
	if ((neg = (n < 0))) {
		n = (n == CELL_MIN) ? CELL_MAX : -n;
		//neg = 1;
	}

	CELL_TYPE tens = 10;
	CELL_TYPE tens_prev = 0;
	int num_chars = 1;
	
	while(tens_prev < tens) {
		if (n < tens) return num_chars + neg;
		tens_prev = tens;
		tens *= 10;
		num_chars++;
	}

	return num_chars + neg;
}


// Parses a tmf machine file (*.tmf) into an
// Automaton struct that consists of:
// > States
// > Transitions
// > Transition symbol variables
//
// A large switch(case) statement parses the file
// character-by-character.
//
// Returns an Automaton struct or exits on
// parse error.
struct Automaton Automaton_import(char *filename) 
{
	FILE *fp = NULL;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	fp = fopen(filename, "r");
	if (fp == NULL) { 
		fprintf(stderr, "Error opening machine file %s\n", filename);
		exit(EXIT_FAILURE);
	}

	// Transition symbols
	//ptrdiff_t symbol = PTRDIFF_MAX,       // symbol in input string/tape 
	CELL_TYPE symbol = CELL_MAX,       // symbol in input string/tape 
		       popsym = CELL_MAX,       // symbol to be matched and popped from top of stack
		       pushsym = CELL_MAX,      // symbol to push to the top of stack
		       direction = CELL_MAX,    // direction to move tape head (0=stay, 1=L, 2=R)
		       writesym = CELL_MAX,     // symbol to write to current head position of TM tape
				 extra = CELL_MAX;

	CELL_TYPE *symptr = &symbol,
				 *popptr = NULL,
				 *pushptr = NULL,
				 *dirptr = NULL,
				 *writeptr = NULL,
				 *extraptr = &extra;

	// For operation of switch statement
	unsigned int mystate = 1,                 // which 'state' or case block to go to next
					 linenum = 1,                 // the line number of the machine file
					 linechar = 1,						// current char index of line
					 mlcmt_line = 0,					// error info for unmatched multi-line comment
					 mlcmt_char = 0,					// ''                                       ''
					 comment_state = UINT_MAX,    // comment return state placeholder
					 quote_state = UINT_MAX;      // quoted char (in ext ops) return state placeholder
					 //symgroup = 0;						// tracks syms listed together

	//int brackets = 0;    // counting 'stack' for matching brackets in command arg parsing

	// Name placeholders, dumped into by Stack(CHAR) accumulators further below
	char *name = NULL,				// state declaration/modification 
		  *state = NULL,           // destination state in transition
		  *special = NULL,         // state listed after start, final, reject directives
		  *newedit = NULL;	      // variable edits

	// Character/symbol accumulators (static)
	struct Stack symst = Stack_init(CELL),//(PTRDIFFT),
					 vsymst = Stack_init(CELL),//(PTRDIFFT),
					 minu = Stack_init(CELL),
					 subtra = Stack_init(CELL),
					 characcumulator = Stack_init(CHAR),
					 specialaccumulator = Stack_init(CHAR),
					 alpha = Stack_init(CELL);

	// Pointers to char/sym accumulators
	struct Stack *symstack = &symst,		  // accumulates list of syms before Trans_add
					 *vsymstack = &vsymst,    // separate symstack for variables (var syms added first)
					 *minuend = &minu,		  // variable set difference = minuend \ subtrahend
					 *subtrahend = &subtra,   // > eventually pushed into vsymstack
					 *characc = &characcumulator,       // general-purpose char (string) accumulator
					 *specacc = &specialaccumulator,    // secondary char (string) accumulator
                *alphabet = &alpha;

	// Pointer placeholders
	struct State *newstate = NULL,     // state resulting from a state definition: q0:...
					 *deststate = NULL;    // state on the receiving end of a transition: a>q1
	struct Trans *newtrans = NULL;
	struct Var   *newvar = NULL,       // variable definition: VARNAME = ...
				    *argvar = NULL;       // variable as operand: $I_AM_A_SET_OF_SYMBOLS

	// Static null char for terminating char accumulators before comparing strings (var,state names)
	// > implicit cast of char accumulator stack's void *elem results in a valid C string
	char nterm = '\0';

	// Placeholder for transition integer symbols, endptr used for strtoll()/strtoull()
	char *endptr;
	CELL_TYPE intsym = 0;

	// For a another perspective of what this import function is doing/how it works,
	// take a look at the 'spec.tmf' file, which describes an NFA
	// written in tmfuck that recognizes a valid tmfuck config file
	// > spec.tmf currently does not work with command arguments... the
	//   quote/paren matching will require a PDA. :)
	//
	// MYSTATE RANGE:     PURPOSE:
	// 1 - 6              Core states: process state definitions and basic transition statements
	// > 1 - 2            > Ensures file starts with a directive or a state declaration, NOT a transition
	// > 3 - 6            > 3 is the 'hub' of the whole import machine
	// 7                  Single line comments # ...\n
	// 8                  Parse error state: frees import variables, prints any error messages
	// 9 - 10             Quoted transition syms in states 1-6
	// 11 - 16            Auxiliary states for 1-6 (mostly for whitespace handling)
	// > 12 - 13          Multiline comments #* ... *#
	// 17                 Prevents trailing comma in sym list
	// 20 - 22            State names listed after start:, final:, and reject: directives
	// 23 - 28            Parameters listed after blank: and bound: directives
	// 30 - 67            Extended arguments for PDA/TM (>a),(a>),(a>b),(L),(R),(z,L),(L,z,>a),...
	//                    > Supports any combination and order of (tape write, direction, or pop/push)
	// 100 - 103          Process command statement $(...)
	//                    > Contains its own switch(case) statement for parsing *args[] for execv() 
	// 300+               Variable references used in transition statements, ie. $VAR,a > q0
	// 400+               Variable declarations/reassignments, ie. VAR = a,b,c

	// Nearly every state handles comments (#, #*...*#	), which are processed at states 7 and 12-13
	// For (nearly) every case statement, unexpected characters go to mystate 8 to indicate an error
	// When EOF for machine file is reached, mystate must be 1 or 3

	struct Automaton machine = Automaton_init();
	struct Automaton *a0 = &machine;

	while ((read = getline(&line, &len, fp)) != -1) {
		                                      // These vars deprecated by Stack(CHAR) accumulators
		//	int i = 0;                         // char index per line
		//	int j = 0;                         // string start char (states, variables)
		//	int k = 0;                         // string len

			linechar = 1;

			for (int i = 0; line[i] != '\0'; i++) {

				//DEBUG:
				//printf("mystate: %3d | line: %4d | char: %3d | val: %c \n", mystate, linenum, i, line[i]);
				//printf("longest_sym: %d\n", longest_sym);
				//printf("mystate: %3d | line: %4d | char: %3d | j: %2d | k: %2d | val: %c \n", mystate, linenum, i, j, k, line[i]);

				CELL_TYPE charhead = line[i];

				switch (mystate) {
					// Ensure state name is at least one char
					case 1:
						if(isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							mystate = 11;
						} else if (isspace(charhead)) {
							mystate = 1;
						} else if (charhead == '#') {
							comment_state = 3;
							mystate = 7;
						} else mystate = 8;
						break;;
					// Finish acquiring state name string
					case 2:
						if (charhead == ':') {
							Stack_push(characc, &nterm);
							name = characc->elem;
							if (!strcmp(name, "start") || 
								!strcmp(name, "final") ||
								!strcmp(name, "reject")) {
								mystate = 20;
							} else if (!strcmp(name, "blank") ||
								!strcmp(name, "bound")) {
								mystate = 23;	
							} else {
								newstate = State_add(a0, name);
								mystate = 3;
							}
							Stack_clear(characc);
						} else if (charhead == '=') {
							Stack_push(characc, &nterm);
							name = characc->elem;
							newvar = Var_add(a0, name);
							Stack_clear(characc);

							mystate = 400;
						} else if (isspace(charhead)) {
							mystate = 2;
						} else if (charhead == '#') {
							comment_state = 2;
							mystate = 7;
						} else mystate = 8;
						break;;
					// Begin parsing state transitions
					case 3:
						// May be signed/unsigned integer sym, process at mystate 81
						if (isdigit(charhead) || charhead == '-') {
							Stack_push(characc, &charhead);
							Stack_push(symstack, &charhead);
							mystate = 80;
						} else if (isnamechar(charhead)) {
							Stack_push(characc, &charhead); // may be next state name
							symbol = charhead;
							Stack_push(symstack, &symbol); // may be a symbol
							
							mystate = 4;
						// For empty string transitions
						} else if (charhead == '>') {
							a0->epsilon = 1;
							symptr = NULL;
							Stack_clear(characc);
							
							mystate = 5;
						} else if (charhead == '\'') {
							mystate = 9;
						} else if (isspace(charhead)) {
							mystate = 3;
						} else if (charhead == '$') {
							//brackets = 1;
							mystate = 200;
						} else if (charhead == '#') {
							comment_state = 3;
							mystate = 7;
						} else mystate = 8;
						break;;
					// Set symbol for state transition
					case 4:
						if (charhead == '>') {
							Stack_clear(characc);
							mystate = 5;
						} else if (charhead == ',') {
							Stack_clear(characc);
							mystate = 17;
						} else if (isspace(charhead)) {
							mystate = 14;
						} else if (charhead == ':') {
							Stack_push(characc, &nterm);
							name = characc->elem;

							if (!strcmp(name, "start") || 
								!strcmp(name, "final") ||
								!strcmp(name, "reject")) {
								mystate = 20;
							} else if (!strcmp(name, "blank") ||
								!strcmp(name, "bound")) {
								mystate = 23;
							} else {
								newstate = State_add(a0, name);
								mystate = 3;
							}
							
							Stack_clear(characc);
							Stack_clear(symstack);
							Stack_clear(vsymstack);
						} else if (charhead == '=') {
							Stack_push(characc, &nterm);
							name = characc->elem;
							newvar = Var_add(a0, name);
							Stack_clear(characc);
							
							mystate = 400;
						} else if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							Stack_clear(symstack);
							Stack_clear(vsymstack);
							
							mystate=11;
						} else if (charhead == '#') {
							comment_state = 14;
							mystate = 7;
						} else mystate = 8;
						break;;
					// Ensure destination state in transition consists of at least one char
					case 5:
						if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							mystate = 6;
						} else if (isspace(charhead)) {
							mystate = 5;
						} else if (charhead == '#') {
							comment_state = 5;
							mystate = 7;
						} else mystate = 8;
						break;;
					// Finish acquiring state name string for the transition
					case 6:
						if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							mystate = 6;
						} else if (charhead == ';') {
							Stack_push(characc, &nterm);
							state = characc->elem;

							deststate = State_add(a0, state);
							
							// epsilon trans always by itself
							// > when symptr = NULL, Trans_add adds epsilon transition
							if (!symptr) {
								Trans_add(a0, newstate, deststate,
										symptr, popptr, pushptr, dirptr, writeptr);
							} else {
								for (unsigned int f = 0; f < vsymstack->size; f++) {
									symptr = Stack_get(vsymstack, f);
									newtrans = Trans_add(a0, newstate, deststate, 
											symptr, popptr, pushptr, dirptr, writeptr);
									Stack_push(alphabet, symptr);
								}

								for (unsigned int f = 0; f < symstack->size; f++) {
									symptr = Stack_get(symstack, f);
									newtrans = Trans_add(a0, newstate, deststate, 
											symptr, popptr, pushptr, dirptr, writeptr);
									Stack_push(alphabet, symptr);
								}
							}

							Stack_clear(symstack);
							Stack_clear(vsymstack);
							Stack_clear(characc);
							
							popptr = pushptr = dirptr = writeptr = NULL;
							symptr = &symbol;
							
							mystate = 3;
						// Allow multiple state transitions per symbol
						} else if (charhead == ',') {
							Stack_push(characc, &nterm);
							state = characc->elem;
							deststate = State_add(a0, state);

							if (!symptr) {
								Trans_add(a0, newstate, deststate,
									symptr, popptr, pushptr, dirptr, writeptr);
							} else {
								for (unsigned int f = 0; f < vsymstack->size; f++) {
									symptr = Stack_get(vsymstack, f);
									newtrans = Trans_add(a0, newstate, deststate, 
											symptr, popptr, pushptr, dirptr, writeptr);
									Stack_push(alphabet, symptr);
									//if (!newtrans->symgroup) newtrans->symgroup = 1;
								}

								for (unsigned int f = 0; f < symstack->size; f++) {
									symptr = Stack_get(symstack, f);
									newtrans = Trans_add(a0, newstate, deststate, 
											symptr, popptr, pushptr, dirptr, writeptr);
									Stack_push(alphabet, symptr);
									//if (!newtrans->symgroup) newtrans->symgroup = 1;
								}
							}
							Stack_clear(characc);

							mystate = 5;
						} else if (isspace(charhead)) {
							mystate = 16;
						} else if (charhead == '(') {
							mystate = 30;
						} else if (charhead == '#') {
							comment_state = 16;
							mystate = 7;
						} else mystate = 8;
						break;;
					// COMMENT HANDLING
					// > Comments can come after any line except for
					//   open-ended single-quoted characters
					//   ie. '# or 'a#
					// > Multi-line comments handled at states 12-13
					case 7:
						if (charhead == '*') {
							mlcmt_line = linenum;
							mlcmt_char = linechar;
							mystate = 12;
						} else {
							mystate = comment_state;
							goto line_end;
						}
						break;
					// PARSE ERROR (machine file reject)
					case 8:
						char highlight=line[linechar-2];
						if (highlight == '\n') highlight = ' ';
						fprintf(stderr,"Error on or near line %u, char [%u]:\n%.*s[%c]%s", 
							linenum, linechar -1, linechar-2, line, highlight, line+linechar-1);
						if (line[linechar-2] == '\n') putchar('\n');
						free(line);
						Stack_free(characc);
						Stack_free(specacc);
						Stack_free(symstack);
						Stack_free(vsymstack);
						Stack_free(minuend);
						Stack_free(subtrahend);
						fclose(fp);
						Automaton_free(a0);
						exit(EXIT_FAILURE);
						break;
					// transition symbol specified in single quotes, ie. 'a',' '
					case 9:
						symbol = charhead;
						Stack_push(symstack, &symbol);
						mystate = 10;
						break;
					// end single quote
					case 10:
						if (charhead == '\'') {
							mystate = 4;
						} else mystate = 8;
						break;
					// space case for state 1
					case 11:
						if (charhead == '=') {
							Stack_push(characc, &nterm);
							name = characc->elem;
							newvar = Var_add(a0, name);
							Stack_clear(characc);

							mystate = 400;
						} else if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							mystate = 11;
						} else if (isspace(charhead)) {
							mystate = 2;
						} else if (charhead == ':') {
							Stack_push(characc, &nterm);
							name = characc->elem;

							if (!strcmp(name, "start") || 
								!strcmp(name, "final") ||
								!strcmp(name, "reject")) {
								mystate = 20;
							} else if (!strcmp(name, "blank") ||
								!strcmp(name, "bound")) {
								mystate = 23;	
							} else {
								newstate = State_add(a0, name);
								mystate = 3;
							}
							Stack_clear(characc);
						} else if (charhead == '#') {
							comment_state = 2;
							mystate = 7;
						} else mystate = 8;
						break;
					// Multi-line comments (12-13)
					// > everything between #* and *#
					case 12:
						if (charhead == '*') {
							mystate = 13;
						} else mystate = 12;
						break;
					case 13:
						if (charhead == '#') {
							mystate = comment_state;
						} else if (charhead == '*') {
							mystate = 13;
						} else mystate = 12;
						break;
					// space case for state 4
					case 14:
						if (charhead == '=') {
							Stack_push(characc, &nterm);
							name = characc->elem;
							newvar = Var_add(a0, name);
							Stack_clear(characc);
							
							mystate = 400;
						} else if (isspace(charhead)) {
							mystate = 14;
						} else if (charhead == ',') {
							Stack_clear(characc);
							mystate = 17;
						} else if (charhead == '>') {
							Stack_clear(characc);
							mystate = 5;
						} else if (charhead == ':') {
							Stack_push(characc, &nterm);
							name = characc->elem;

							if (!strcmp(name, "start") || 
								!strcmp(name, "final") ||
								!strcmp(name, "reject")) {
								mystate = 20;
							} else if (!strcmp(name, "blank") ||
								!strcmp(name, "bound")) {
								mystate = 23;								
							} else {
								newstate = State_add(a0, name);
								mystate = 3;
							}

							Stack_clear(characc);
							Stack_clear(symstack);
							Stack_clear(vsymstack);
						} else if (charhead == '$') {
							//brackets = 1;
							mystate = 200;
						} else if (charhead == '#') {
							comment_state = 14;
							mystate = 7;
						} else
							mystate = 8;
						break;
					// space case for state 6
					case 16:
						if (isspace(charhead))
							mystate = 16;
						else if (charhead == ',') {
							Stack_push(characc, &nterm);
							state = characc->elem;
							deststate = State_add(a0, state);

							for (unsigned int f = 0; f < vsymstack->size; f++) {
								symptr = Stack_get(vsymstack, f);
								newtrans = Trans_add(a0, newstate, deststate, 
										symptr, popptr, pushptr, dirptr, writeptr);
								Stack_push(alphabet, symptr);
							}

							for (unsigned int f = 0; f < symstack->size; f++) {
								symptr = Stack_get(symstack, f);
								newtrans = Trans_add(a0, newstate, deststate, 
										symptr, popptr, pushptr, dirptr, writeptr);
								Stack_push(alphabet, symptr);
							}
							Stack_clear(characc);

							mystate = 5;
						} else if (charhead == ';') {
							Stack_push(characc, &nterm);
							state = characc->elem;
							deststate = State_add(a0, state);
							
							for (unsigned int f = 0; f < vsymstack->size; f++) {
								symptr = Stack_get(vsymstack, f);
								newtrans = Trans_add(a0, newstate, deststate, 
										symptr, popptr, pushptr, dirptr, writeptr);
								Stack_push(alphabet, symptr);
							}

							for (unsigned int f = 0; f < symstack->size; f++) {
								symptr = Stack_get(symstack, f);
								newtrans = Trans_add(a0, newstate, deststate, 
										symptr, popptr, pushptr, dirptr, writeptr);
								Stack_push(alphabet, symptr);
							}
							
							Stack_clear(symstack);
							Stack_clear(vsymstack);
							Stack_clear(characc);
							
							popptr = pushptr = dirptr = writeptr = NULL;
							symptr = &symbol;

							mystate = 3;
						} else if (charhead == '(') {
							mystate = 30;
						} else if (charhead == '#') {
							comment_state = 16;
							mystate = 7;
						} else 
							mystate = 8;
						break;
					// prevent trailing comma after listed chars for transitions
					case 17:
						if (isspace(charhead)) {
							mystate = 17;
						} else if (charhead == '\'') {
							mystate = 9;
						} else if (isdigit(charhead) || charhead == '-') {
							Stack_push(characc, &charhead);
							Stack_push(symstack, &charhead);
							mystate = 80;
						} else if (isnamechar(charhead)) {
							symbol = charhead;
							Stack_push(symstack, &symbol);
							mystate = 4;
						} else if (charhead == '$') {
							mystate = 300;
						} else if (charhead == '#') {
							comment_state = 17;
							mystate = 7;
						} else mystate = 8;
						break;
					// line represents the start state
					case 20:
						if (isspace(charhead)) {
							mystate = 20;
						} else if (isnamechar(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 21;
						} else if (charhead == '#') {
							comment_state = 20;
							mystate = 7;
						} else mystate = 8;
						break;
					case 21:
						if (isnamechar(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 21;
						} else if (isspace(charhead)) {
							mystate = 22;
						} else if (charhead == ',' || charhead == ';') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							newstate = State_add(a0, special);
	
							if (charhead == ',') {
								mystate = 20;
							} else mystate = 3;
							
							if (!strcmp(name, "start")) {
								set_start(a0, newstate);
							} else if (!strcmp(name, "final")) {
								if (newstate->reject) {
									fprintf(stderr, "%s cannot be both a final and reject state\n", special);
									mystate = 8;
								}
								set_final(a0, newstate);
							} else if (!strcmp(name, "reject")) {
								if (newstate->final) {
									fprintf(stderr, "%s cannot be both a final and reject state\n", special);
									mystate = 8;
								}
								set_reject(a0, newstate);
							}
							Stack_clear(specacc);
						} else if (charhead == '#') {
							comment_state = 22;
							mystate = 7;
						} else mystate = 8;
						break;
					case 22:
						if (isspace(charhead)) {
							mystate = 22;
						} else if (charhead == ',' || charhead == ';') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							newstate = State_add(a0, special);
							
							if (charhead == ',') {
								mystate = 20;
							} else mystate = 3;
							
							if (!strcmp(name, "start")) {
								set_start(a0, newstate);
							} else if (!strcmp(name, "final")) {
								if (newstate->reject) {
									fprintf(stderr, "%s cannot be both a final and reject state\n", special);
									mystate = 8;
								}
								set_final(a0, newstate);
							} else if (!strcmp(name, "reject")) {
								if (newstate->final) {
									fprintf(stderr, "%s cannot be both a final and reject state\n", special);
									mystate = 8;
								}
								set_reject(a0, newstate);
							}
							Stack_clear(specacc);
						} else if (charhead == '#') {
							comment_state = 22;
							mystate = 7;
						} else mystate = 8;
						break;
					// 23-27: line represents the custom blank character for tms
					case 23:
						if (isspace(charhead)) {
							mystate = 23;
						} else if (charhead == '\'') { 
							mystate = 24;
						} else if (charhead == ';') {
							if (!strcmp(name, "blank")) tm_blank = ' ';
							if (!strcmp(name, "bound")) tm_bound = '\0';
							mystate = 3;
						} else if (isdigit(charhead) || charhead == '-') {
							Stack_push(specacc, &charhead);
							mystate = 500;
						} else if (isnamechar(charhead)) { 
							if (!strcmp(name, "blank")) tm_blank = charhead;
							if (!strcmp(name, "bound")) {
								if (charhead == 'H') tm_bound_halt = 1;
								else tm_bound = charhead;
							}
							mystate = 27;
						} else if (charhead == '#') {
							comment_state = 23;
							mystate = 7;
						} else mystate = 8;
						break;
					case 24:
						if (!strcmp(name, "blank")) tm_blank = charhead;
						if (!strcmp(name, "bound")) {
								if (charhead == 'H') tm_bound_halt = 1;
								else tm_bound = charhead;
							}
						mystate = 25;
						break;
					case 25:
						if (charhead == '\'') {
							mystate = 26;
						} else mystate = 8;
						break;
					case 26:
						if (isspace(charhead)) {
							mystate = 26;
						} else if (charhead == ';') {
							mystate = 3;
						} else if (charhead == ',') {
							mystate = 28;
						} else if (charhead == '#') {
							comment_state = 26;
							mystate = 7;
						} else mystate = 8;
						break;
					case 27:
						if (isspace(charhead)) {
							mystate = 27;
						} else if (charhead == ';') {
							mystate = 3;
						} else if (charhead == ',') {
							mystate = 28;
						} else if (charhead == '#') {
							comment_state = 27;
							mystate = 7;
						} else mystate = 8;
						break;
					// prevent trailing comma
					case 28:
						if (isspace(charhead)) {
							mystate = 28;
						} else if (charhead == '\'') { 
							mystate = 24;
						} else if (charhead == ';') {
							mystate = 8;
						} else if (isnamechar(charhead)) { 
							if (!strcmp(name, "blank")) tm_blank = charhead;
							if (!strcmp(name, "bound")) {
								if (charhead == 'H') tm_bound_halt = 1;
								else tm_bound = charhead;
							}
							mystate = 27;
						} else if (charhead == '#') {
							comment_state = 28;
							mystate = 7;
						} else mystate = 8;
						break;
					case 500:
						if (isdigit(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 501;
						} else if (isspace(charhead)) {
							tm_blank = *(char *)Stack_pop(specacc);
							Stack_clear(specacc);
							mystate = 502;
						} else if (charhead == ';') {
							tm_blank = *(char *)Stack_pop(specacc);
							Stack_clear(specacc);
							mystate = 3;
						} else mystate = 8;
						break;
					case 501:
						if (isdigit(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 501;
						} else if (isspace(charhead)) {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							tm_blank = intsym;

							//if (specacc->size > longest_sym) longest_sym = specacc->size;
							Stack_clear(specacc);
							mystate = 502;
						} else if (charhead == ';') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							tm_blank = intsym;

							//if (specacc->size > longest_sym) longest_sym = specacc->size;
							Stack_clear(specacc);
							mystate = 3;
						} else mystate = 8;
						break;
					case 502:
						if (isspace(charhead)) {
							mystate = 502;
						} else if (charhead == ';') {
							mystate = 3;
						} else mystate = 8;
						break;
					// PUSH-POP/DIRECTION/WRITE PARSING
					case 30:
						if (isspace(charhead)) {
							mystate = 30;
						} else if (isdigit(charhead) || charhead == '-') {
							popsym = charhead;
							popptr = &popsym;
							writesym = charhead;
							writeptr = &writesym;

							Stack_push(specacc, &charhead);
							
							mystate = 90;
						} else if (charhead == 'L') {
							popsym = charhead; // may be popsym, set for now
							popptr = &popsym;
							direction = 1;
							dirptr = &direction;

							mystate = 43;
						} else if (charhead == 'R') {
							popsym = charhead; // may be popsym, set for now
							popptr = &popsym;
							direction = 2;
							dirptr = &direction;
							
							mystate = 43;
						} else if (isnamechar(charhead)) {
							popsym = charhead;     // could be tmwrite or pda pop, we'll set both for now
							popptr = &popsym;
							writesym = charhead;   // then confirm at state 31
							writeptr = &writesym;

							mystate = 31;
						} else if (charhead == '>') {
							mystate = 39;
						} else if (charhead == '\'') {
							quote_state = 31;
							
							mystate = 60;         // 60 handles quoted pda pops or tm writes
						} else if (charhead == '#') {
							comment_state = 30;
							
							mystate = 7;
						} else mystate = 8;
						break;
					case 31:
						if (isspace(charhead)) {
							mystate = 31;
						} else if (charhead == ')') {
							popptr = NULL;      // confirms prev charhead was not pop
							mystate = 54;              // 54 is end of ext ops
						} else if (charhead == ',') {
							popptr = NULL;      // confirms prev charhead was not pop
							mystate = 32;
						} else if (charhead == '>') {
							writeptr = NULL;   // confirms prev charhead was not tm write
							mystate = 37;
						} else if (charhead == '#') {
							comment_state = 31;
							mystate = 7;
						} else mystate = 8;
						break;
					case 32:
						if (isspace(charhead)) {
							mystate = 32;
						} else if (isdigit(charhead) || charhead == '-') {
							popsym = charhead;
							popptr = &popsym;
							//Stack_push(characc, &charhead);
							Stack_push(specacc, &charhead);
							mystate = 91;
						} else if (charhead == 'L') {
							popsym = charhead;  // potential popsym, set for now
							popptr = &popsym;
							direction = 1;
							dirptr = &direction;

							mystate = 35;
						} else if (charhead == 'R') {
							popsym = charhead;  // potential popsym, set for now
							popptr = &popsym;
							direction = 2;
							dirptr = &direction;

							mystate = 35;
						} else if (isnamechar(charhead)) {
							popsym = charhead;   // here, char is most definitely a stack pop
							popptr = &popsym;

							mystate = 33;
						} else if (charhead == '\'') {
							quote_state = 33;
							mystate = 60;      // 60 will handle quoted stack pop
						} else if (charhead == '>') {
							mystate = 57;
							//mystate = 41;
						} else if (charhead == '#') {
							comment_state = 32;
							mystate = 7;
						} else mystate = 8;
						break;
					case 33:
						if (isspace(charhead)) {
							mystate = 33;
						} else if (charhead == '>') {
							mystate = 34;
						} else if (charhead == '#') {
							comment_state = 33;
							mystate = 7;
						} else mystate = 8;
						break;
					case 34:
						if (isspace(charhead)) {
							mystate = 34;
						} else if (isdigit(charhead) || charhead == '-') {
							pushsym = charhead;
							pushptr = &pushsym;
							Stack_push(specacc, &charhead);

							mystate = 92;
						} else if (isnamechar(charhead)) {
							pushsym = charhead;  // anything right of a '>' is a stack push
							pushptr = &pushsym;
							
							mystate = 36;
						} else if (charhead == '\'') {
							quote_state = 36;
							mystate = 65;     // 65 will handle quoted stack push
						} else if (charhead == ')') {
							mystate = 54;
						} else if (charhead == ',') {
							mystate = 38;
						} else if (charhead == '#') {
							comment_state = 34;
							mystate = 7;
						} else mystate = 8;
						break;
					case 35:
						if (isspace(charhead)) {
							mystate = 35;
						} else if (charhead == ')') {
							popptr = NULL;
							mystate = 54;
						} else if (charhead == ',') {
							popptr = NULL;
							mystate = 46;
						} else if (charhead == '>') {
							dirptr = NULL;
							mystate = 34;   // allow middle L,R to be a popsym
						} else if (charhead == '#') {
							comment_state = 35;
							mystate = 7;
						} else mystate = 8;
						break;
					case 36:
						if (isspace(charhead)) {
							mystate = 36;
						} else if (charhead == ')') {
							mystate = 54;
						} else if (charhead == ',') {
							mystate = 38;
						} else if (charhead == '#') {
							comment_state = 36;
							mystate = 7;
						} else mystate = 8;
						break;
					case 37:
						if (isspace(charhead)) {
							mystate = 37;
						} else if (isdigit(charhead) || charhead == '-') {
							pushsym = charhead;
							pushptr = &pushsym;
							Stack_push(specacc, &charhead);

							mystate = 93;
						} else if (isnamechar(charhead)) {
							pushsym = charhead;
							pushptr = &pushsym;
							
							mystate = 40;
						} else if (charhead == '\'') {
							quote_state = 40;
							mystate = 67;   // quoted stack push
						} else if (charhead == ')') {
							mystate = 54;
						} else if (charhead == ',') {
							mystate = 41;
						} else if (charhead == '#') {
							comment_state = 37;
							mystate = 7;
						} else mystate = 8;
						break;
					case 38:
						if (isspace(charhead)) {
							mystate = 38;
						} else if (charhead == 'L') {
							direction = 1;
							dirptr = &direction;

							mystate = 47;
						} else if (charhead == 'R') {
							direction = 2;
							dirptr = &direction;

							mystate = 47;
						} else if (charhead == '#') {
							comment_state = 38;
							mystate = 7;
						} else mystate = 8;
						break;
					case 39:
						if (isspace(charhead)) {
							mystate = 39;
						} else if (isdigit(charhead) || charhead == '-') {
							pushsym = charhead;
							pushptr = &pushsym;
							Stack_push(specacc, &charhead);

							mystate = 93;
						} else if (isnamechar(charhead)) {
							pushsym = charhead;
							pushptr = &pushsym;

							mystate = 40;
						} else if (charhead == '\'') {
							quote_state = 40;
							mystate = 67;  // quoted stack push
						} else if (charhead == '#') {
							comment_state = 39;
							mystate = 7;
						} else mystate = 8;
						break;
					case 40:
						if (isspace(charhead)) {
							mystate = 40;
						} else if (charhead == ')') {
							mystate = 54;
						} else if (charhead == ',') {
							mystate = 41;
						} else if (charhead == '#') {
							comment_state = 40;
							mystate = 7;
						} else mystate = 8;
						break;
					case 41:
						if (isspace(charhead)) {
							mystate = 41;
						} else if (isdigit(charhead) || charhead == '-') {
							// Just push digit for now, verify if tmwrite or push at state 94
							Stack_push(specacc, &charhead);
							mystate = 94;
						} else if (charhead == 'L') {
							direction = 1;
							dirptr = &direction;

							mystate = 50;
						} else if (charhead == 'R') {
							direction = 2;
							dirptr = &direction;

							mystate = 50;
						} else if (isnamechar(charhead)) {
							writesym = charhead;
							writeptr = &writesym;

							mystate = 42;
						} else if (charhead == '\'') {
							quote_state = 42;
							mystate = 69;  
						} else if (charhead == '#') {
							comment_state = 41;
							mystate = 7;
						} else mystate = 8;
						break;
					case 42:
						if (isspace(charhead)) {
							mystate = 42;
						} else if (charhead == ')') {
							mystate = 54;
						} else if (charhead == ',') {
							mystate = 38;
						} else if (charhead == '#') {
							comment_state = 42;
							mystate = 7;
						} else mystate = 8;
						break;
					case 43:
						if (isspace(charhead)) {
							mystate = 43;
						} else if (charhead == ')') {
							popptr = NULL;
							mystate = 54;
						} else if (charhead == ',') {
							popptr = NULL;
							mystate = 44;
						} else if (charhead == '>') { // Allow initial L,R to be unquoted popsym
							dirptr = NULL;
							mystate = 37;
						} else if (charhead == '#') {
							comment_state = 43;
							mystate = 7;
						} else mystate = 8;
						break;
					case 44:
						if (isspace(charhead)) {
							mystate = 44;
						} else if (isdigit(charhead) || charhead == '-') {
							popsym = charhead;
							popptr = &popsym;
							writesym = charhead;
							writeptr = &writesym;
							Stack_push(specacc, &charhead);

							mystate = 95;
						} else if (isnamechar(charhead)) {
							popsym = charhead;
							popptr = &popsym;
							writesym = charhead;   // stack pop or tm write
							writeptr = &writesym;
							
							mystate = 45;
						} else if (charhead == '\'') {
							quote_state = 45;
							mystate = 60;
						} else if (charhead == '>') {
							mystate = 55;
						} else if (charhead == '#') {
							comment_state = 44;
							mystate = 7;
						} else mystate = 8;
						break;
					case 45:
						if (isspace(charhead)) {
							mystate = 45;
						} else if (charhead == ')') {
							popptr = NULL;
							mystate = 54;
						} else if (charhead == ',') {
							popptr = NULL;
							mystate = 46;
						} else if (charhead == '>') {
							writeptr = NULL;
							mystate = 51;
						} else if (charhead == '#') {
							comment_state = 45;
							mystate = 7;
						} else mystate = 8;
						break;
					case 46:
						if (isspace(charhead)) {
							mystate = 46;
						} else if (isdigit(charhead) || charhead == '-') {
							popsym = charhead;
							popptr = &popsym;
							Stack_push(specacc, &charhead);

							mystate = 96;
						} else if (isnamechar(charhead)) {
							popsym = charhead;  // definitely a stack pop
							popptr = &popsym;
							
							mystate = 48;
						} else if (charhead == '\'') {
							quote_state = 48;
							mystate = 60;
						} else if (charhead == '>') {
							mystate = 53;
						} else if (charhead == '#') {
							comment_state = 46;
							mystate = 7;
						} else mystate = 8;
						break;
					case 47:
						if (isspace(charhead)) {
							mystate = 47;
						} else if (charhead == ')') {
							mystate = 54;
						} else if (charhead == '#') {
							comment_state = 47;
							mystate = 7;
						} else mystate = 8;
						break;
					case 48:
						if (isspace(charhead)) {
							mystate = 48;
						} else if (charhead == '>') {
							mystate = 49;
						} else if (charhead == '#') {
							comment_state = 48;
							mystate = 7;
						} else mystate = 8;
						break;
					case 49:
						if (isspace(charhead)) {
							mystate = 49;
						} else if (isdigit(charhead) || charhead == '-') {
							Stack_push(specacc, &charhead);
							mystate = 97;
						} else if (isnamechar(charhead)) {
							pushsym = charhead;
							pushptr = &pushsym;

							mystate = 47;
						} else if (charhead == '\'') {
							quote_state = 47;
							mystate = 67;
						} else if (charhead == ')') {
							mystate = 54;
						} else if (charhead == '#') {
							comment_state = 49;
							mystate = 7;
						} else mystate = 8;
						break;
					case 50:
						if (isspace(charhead)) {
							mystate = 50;
						} else if (charhead == ')') {
							mystate = 54;
						} else if (charhead == ',') {
							mystate = 53;
						} else if (charhead == '#') {
							comment_state = 50;
							mystate = 7;
						} else mystate = 8;
						break;
					case 51:
						if (isspace(charhead)) {
							mystate = 51;
						} else if (isdigit(charhead) || charhead == '-') {
							pushsym = charhead;
							pushptr = &pushsym;

							Stack_push(specacc, &charhead);
							mystate = 98;
						} else if (isnamechar(charhead)) {
							pushsym = charhead;
							pushptr = &pushsym;

							mystate = 52;
						} else if (charhead == ')') {
							mystate = 54;
						} else if (charhead == '\'') {
							quote_state = 52;
							mystate = 67;
						} else if (charhead == ',') {
							mystate = 53;
						} else if (charhead == '#') {
							comment_state = 51;
							mystate = 7;
						} else mystate = 8;
						break;
					case 52:
						if (isspace(charhead)) {
							mystate = 52;
						} else if (charhead == ')') {
							mystate = 54;
						} else if (charhead == ',') {
							mystate = 53;
						} else if (charhead == '#') {
							comment_state = 52;
							mystate = 7;
						} else mystate = 8;
						break;
					case 53:
						if (isspace(charhead)) {
							mystate = 53;
						} else if (isdigit(charhead) || charhead == '-') {
							// Just push digit for now, verify if tmwrite or push at state 97
							Stack_push(specacc, &charhead);
							mystate = 97;
						} else if (isnamechar(charhead)) {
							if (pushptr || popptr) { 
								writesym = charhead;
								writeptr = &writesym;
							} else if (writesym) {
								pushsym = charhead;   // may be a stack push,
								pushptr = &pushsym;
							}
							mystate = 47;
						} else if (charhead == '\'') {
							quote_state = 47;
							mystate = 65;
						} else if (charhead == '#' ) {
							comment_state = 53;
							mystate = 7;
						} else mystate = 8;
						break;
					// End of extended operation parse
					case 54:
						if (isspace(charhead)) {
							mystate = 54;
						// Last ext op for transition statement
						// Add new transition
						} else if (charhead == ';') {
							Stack_push(characc, &nterm);
							state = characc->elem;
							deststate = State_add(a0, state);
						
							if (!symptr) {
								newtrans = Trans_add(a0, newstate, deststate, 
										symptr, popptr, pushptr, dirptr, writeptr);
							} else {
								for (unsigned int f = 0; f < vsymstack->size; f++) {
									symptr = Stack_get(vsymstack, f);
									newtrans = Trans_add(a0, newstate, deststate, 
											symptr, popptr, pushptr, dirptr, writeptr);
									Stack_push(alphabet, symptr);
								}

								for (unsigned int f = 0; f < symstack->size; f++) {
									symptr = Stack_get(symstack, f);
									newtrans = Trans_add(a0, newstate, deststate, 
											symptr, popptr, pushptr, dirptr, writeptr);
									Stack_push(alphabet, symptr);
								}
							}

							if (writeptr || dirptr) is_tm = 1;
							if (popptr || pushptr) is_pda = 1;

							// Clear transition symbol accumulators (no more dest states)
							Stack_clear(symstack);
							Stack_clear(vsymstack);
							Stack_clear(characc);
							
							// Reset ext symbols
							popptr = pushptr = dirptr = writeptr = NULL;
							symptr = &symbol;
							
							mystate = 3;
						// Another destination state ahead
						// > add new transition
						} else if (charhead == ',') {
							Stack_push(characc, &nterm);
							state = characc->elem;
							deststate = State_add(a0, state);

							if (!symptr) {
								newtrans = Trans_add(a0, newstate, deststate, 
										symptr, popptr, pushptr, dirptr, writeptr);
							} else {
								for (unsigned int f = 0; f < vsymstack->size; f++) {
									symptr = Stack_get(vsymstack, f);
									newtrans = Trans_add(a0, newstate, deststate, 
											symptr, popptr, pushptr, dirptr, writeptr);
									Stack_push(alphabet, symptr);
								}

								for (unsigned int f = 0; f < symstack->size; f++) {
									symptr = Stack_get(symstack, f);
									newtrans = Trans_add(a0, newstate, deststate,
											symptr, popptr, pushptr, dirptr, writeptr);
									Stack_push(alphabet, symptr);
								}
							}

							if (writeptr || dirptr) is_tm = 1;
							if (popptr || pushptr) is_pda = 1;

							// Only clear char accumulator (more dest states expected after ',')
							Stack_clear(characc);

							// Reset ext symbols (do not reset symptr yet)
							popptr = pushptr = dirptr = writeptr = NULL;
							
							mystate = 5;
						} else if (charhead == '#') {
							comment_state = 54;
							mystate = 7;
						} else mystate = 8;
						break;
					case 55:
						if (isspace(charhead)) {
							mystate = 55;
						} else if (isdigit(charhead) || charhead == '-') {
							pushsym = charhead;
							pushptr = &pushsym;
							Stack_push(specacc, &charhead);

							mystate = 99;
						} else if (isnamechar(charhead)) {
							pushsym = charhead;  // definitely a stack pop
							pushptr = &pushsym;
							
							mystate = 56;
						} else if (charhead == '\'') {
							quote_state = 56;
							mystate = 67;
						} else if (charhead == '#') {
							comment_state = 55;
							mystate = 7;
						} else mystate = 8;
						break;
					case 56:
						if (isspace(charhead)) {
							mystate = 56;
						} else if (charhead == ')') {
							mystate = 54;
						} else if (charhead == ',') {
							mystate = 53;
						} else if (charhead == '#') {
							comment_state = 56;
							mystate = 7;
						} else mystate = 8;
						break;
					case 57:
						if (isspace(charhead)) {
							mystate = 57;
						} else if (isdigit(charhead) || charhead == '-') {
							Stack_push(specacc, &charhead);
							mystate = 94;
						} else if (isnamechar(charhead)) {
							pushsym = charhead;
							pushptr = &pushsym;

							mystate = 42;
						} else if (charhead == '\'') {
							quote_state = 42;
							mystate = 67; 
						} else if (charhead == '#') {
							comment_state = 57;
							mystate = 7;
						} else mystate = 8;
						break;
					// Ambiguous quoted stack pop or tm write
					case 60:
						if (!writeptr) {
							writesym = charhead;
							writeptr = &writesym;
						}
						if (!popptr) {
							popsym = charhead;
							popptr = &popsym;
						}
						mystate = 61;
						break;
					case 61:
						if (charhead == '\'') {
							mystate = quote_state;
						} else mystate = 8;
						break;
					// Ambiguous quoted stack push or tm write
					case 65:
						if (pushsym) {
							writesym = charhead;
							writeptr = &writesym;
						} else if (writeptr) {
							pushsym = charhead;
							pushptr = &pushsym;
						}

						mystate = 66;
						break;
					case 66:
						if (charhead == '\'') {
							mystate = quote_state;
						} else mystate = 8;
						break;
					// Unambiguous stack push
					case 67:
						pushsym = charhead;
						pushptr = &pushsym;

						mystate = 68;
						break;
					case 68:
						if (charhead == '\'') {
							mystate = quote_state;
						} else mystate = 8;
						break;
					// Unambiguous tm write
					case 69:
						writesym = charhead;
						writeptr = &writesym;

						mystate = 70;
						break;
					case 70:
						if (charhead == '\'') {
							mystate = quote_state;
						} else mystate = 8;
						break;

					// INTEGER TRANSITION SYMBOL PROCESSING
					// > Transition symbols may be signed/unsigned integers!
					// > Single digits syms will be stored as their ASCII representations
					//   --> This can be overridden by preceding the transition sym digit with '0' :)
					//       ie.  7 > q0  is the same as    55 > q0;
					//           07 > q0  is the same as  '\a' > q0; (bell character)
					//            A > q0  is the same as    65 > q0;
					// > Only a single ascii char may be enclosed with single quotes. Don't try it with ints!
					//   --> Easiest way to specify a control character is to look up its ASCII decimal
					//       and use that as a transition sym.
					// > Multiple digits will be converted to integers of CELL_TYPE (defined in auto.h)
					case 80:
						// 2nd+ consecutive digit: This may be: 
						// > state name consisting of 1+ initial consecutive digits
						// > ascii (single-digit) transition sym
						// > int (multi-digit) transition sym
						if (isdigit(charhead)) {
							Stack_push(characc, &charhead);
							Stack_pop(symstack);
							mystate = 81;
						// Non-digit, finish accumulating state name elsewhere
						} else if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							Stack_clear(symstack);
							Stack_clear(vsymstack);  // not sure if vsymstack clear needed?
							mystate = 11;
						// No digits/chars left to count. Wait for (','|'>') or (':') to determine
						// if it's a transition sym or single-char(digit) state name
						// > single digit will be stored as it's ASCII value, not as it's 'numerical' value
						} else if (isspace(charhead)) {
							mystate = 4;
						// Single ascii digit as part of transition sym list
						} else if (charhead == ',') {
							Stack_clear(characc);
							mystate = 17;
						// Only ascii digit in transition
						} else if (charhead == '>') {
							Stack_clear(characc);
							mystate = 5;
						// Single-char(digit) state name
						} else if (charhead == ':') {
							Stack_push(characc, &nterm);
							name = characc->elem;
							newstate = State_add(a0, name);
							
							Stack_clear(characc);
							Stack_clear(symstack);
							Stack_clear(vsymstack);

							mystate = 3;
						} else if (charhead == '#') {
							comment_state = 4;
							mystate = 7;
						} else mystate = 8;
						break;
					case 81:
						// Keep counting digits, may be an int sym
						if (isdigit(charhead)) {
							Stack_push(characc, &charhead);
							mystate = 81;
						} else if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							mystate = 11;
						} else if (isspace(charhead)) {
							mystate = 82;
						} else if (charhead == ',') {
							// strtol
							Stack_push(characc, &nterm);
							special = characc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							Stack_push(symstack, &intsym);
							
							//if (characc->size > longest_sym) longest_sym = characc->size;
							Stack_clear(characc);
							mystate = 17;
						} else if (charhead == '>') {
							//strtol
							Stack_push(characc, &nterm);
							special = characc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							Stack_push(symstack, &intsym);

							//if (characc->size > longest_sym) longest_sym = characc->size;
							Stack_clear(characc);
							mystate = 5;
						} else if (charhead == ':') {
							Stack_push(characc, &nterm);
							name = characc->elem;
							newstate = State_add(a0, name);
							
							Stack_clear(characc);
							Stack_clear(symstack);
							Stack_clear(vsymstack);

							mystate = 3;
						} else if (charhead == '#') {
							comment_state = 82;
							mystate = 7;
						} else mystate = 8;
						break;
					case 82:
						if (isspace(charhead)) {
							mystate = 82;
						// int sym part of transition sym list
						} else if (charhead == ',') {
							// strtol
							Stack_push(characc, &nterm);
							special = characc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							Stack_push(symstack, &intsym);

							//if (characc->size > longest_sym) longest_sym = characc->size;
							Stack_clear(characc);
							mystate = 17;
						// int sym only or int sym ends trans sym list
						} else if (charhead == '>') {
							// strtol
							Stack_push(characc, &nterm);
							special = characc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							Stack_push(symstack, &intsym);

							//if (characc->size > longest_sym) longest_sym = characc->size;
							Stack_clear(characc);
							mystate = 5;
						// State name consisting solely of digits (ain't that fun?)
						} else if (charhead == ':') {
							Stack_push(characc, &nterm);
							name = characc->elem;
							newstate = State_add(a0, name);
							
							Stack_clear(characc);
							Stack_clear(symstack);
							Stack_clear(vsymstack);

							mystate = 3;
						} else if (charhead == '#') {
							comment_state = 82;
							mystate = 7;
						} else mystate = 8;
						break;

					// INTEGER SYMS as part of extended operations (pop-push/direction/write)
					// If more digits are accumulated, create an int from them and replace the
					// appropriate pop/push/write symbol
					//
					// digit counter for state 30
					case 90:
						if (isspace(charhead)) {
							if (specacc->size > 1) {
								//strtoll writesym & popsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								writesym = intsym;
								popsym = intsym;
							
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
							
							Stack_clear(specacc);
							mystate = 31;
						} else if (isdigit(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 90;
						} else if (charhead == ')') {
							if (specacc->size > 1) {
								//strtoll writesym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								writesym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
						
							Stack_clear(specacc);
							popptr = NULL;

							mystate = 54;              // 54 is end of ext ops
						} else if (charhead == ',') {
							if (specacc->size > 1) {
								//strtoll writesym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								writesym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
							
							Stack_clear(specacc);
							popptr = NULL;
							
							mystate = 32;
						} else if (charhead == '>') {
							if (specacc->size > 1) {
								//strtoll popsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								popsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
						
							Stack_clear(specacc);
							writeptr = NULL;
							
							mystate = 37;
						} else if (charhead == '#') {
							if (specacc->size > 1) {
								//strtoll writesym, popsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								writesym = intsym;
								popsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}

							Stack_clear(specacc);
							comment_state = 31;
							mystate = 7;
						} else mystate = 8;
						break;
					// digit counter for state 32
					case 91:
						if (isspace(charhead)) {
							if (specacc->size > 1) {
								//strtoll popsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								popsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}

							Stack_clear(specacc);
							mystate = 33;
						} else if (isdigit(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 91;
						} else if (charhead == '>') {
							if (specacc->size > 1) {
								//strtoll popsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								popsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
							
							Stack_clear(specacc);
							mystate = 34;
						} else if (charhead == '#') {
							if (specacc->size > 1) {
								//strtoll popsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								popsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
							
							Stack_clear(specacc);
							comment_state = 33;
							mystate = 7;
						} else mystate = 8;
						break;
					// digit counter for state 34
					case 92:
						if (isspace(charhead)) {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
							
							Stack_clear(specacc);
							mystate = 36;
						} else if (isdigit(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 92;
						} else if (charhead == ')') {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
							
							Stack_clear(specacc);
							mystate = 54;
						} else if (charhead == ',') {
							if (specacc->size > 1) {
								// strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
							
							Stack_clear(specacc);
							mystate = 38;
						} else if (charhead == '#') {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
							
							Stack_clear(specacc);
							comment_state = 36;
							mystate = 7;
						} else mystate = 8;
						break;
					// digit counter for states 37, 39
					case 93:
						if (isspace(charhead)) {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							mystate = 40;
						} else if (isdigit(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 93;
						} else if (charhead == ')') {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
						
							Stack_clear(specacc);
							mystate = 54;
						} else if (charhead == ',') {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							mystate = 41;
						} else if (charhead == '#') {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							comment_state = 40;
							mystate = 7;
						} else mystate = 8;
						break;
					// digit counter for state 41
					case 94:
						if (isspace(charhead)) {
							if (specacc->size > 1) {
								//strtoll pushsym or write
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							} else {
								intsym = *(char *)Stack_pop(specacc);
							}

							if (pushptr || popptr) {
								writesym = intsym;
								writeptr = &writesym;
							} else if (writeptr) {
								pushsym = intsym;
								pushptr = &pushsym;
							}
							Stack_clear(specacc);

							mystate = 42;
						} else if (isdigit(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 94;
						} else if (charhead == ')') {
							if (specacc->size > 1) {
								//strtoll pushsym or write
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							} else {
								intsym = *(char *)Stack_pop(specacc);
							}

							if (pushptr || popptr) {
								writesym = intsym;
								writeptr = &writesym;
							} else if (writeptr) {
								pushsym = intsym;
								pushptr = &pushsym;
							}
							Stack_clear(specacc);

							mystate = 54;
						} else if (charhead == ',') {
							if (specacc->size > 1) {
								//strtoll pushsym or write
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							} else {
								intsym = *(char *)Stack_pop(specacc);
							}

							if (pushptr || popptr) {
								writesym = intsym;
								writeptr = &writesym;
							} else if (writeptr) {
								pushsym = intsym;
								pushptr = &pushsym;
							}

							Stack_clear(specacc);
							mystate = 38;
						} else if (charhead == '#') {
							if (specacc->size > 1) {
								//strtoll pushsym or write
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							} else {
								intsym = *(char *)Stack_pop(specacc);
							}

							if (pushptr || popptr) {
								writesym = intsym;
								writeptr = &writesym;
							} else if (writeptr) {
								pushsym = intsym;
								pushptr = &pushsym;
							}
	
							Stack_clear(specacc);
							comment_state = 42;
							mystate = 7;
						} else mystate = 8;
						break;
					// digit counter for state 44
					case 95:
						if (isspace(charhead)) {
							if (specacc->size > 1) {
								//strtoll pop or write
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								popsym = intsym;
								writesym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							mystate = 45;
						} else if (isdigit(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 95;
						} else if (charhead == ')') {
							if (specacc->size > 1) {
								//strtoll write
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								writesym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							popptr = NULL;
							
							mystate = 54;
						} else if (charhead == ',') {
							if (specacc->size > 1) {
								//strtoll write
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								writesym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							popptr = NULL;

							mystate = 46;
						} else if (charhead == '>') {
							if (specacc->size > 1) {
								//strtoll pop
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								popsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							writeptr = NULL;

							mystate = 51;
						} else if (charhead == '#') {
							if (specacc->size > 1) {
								//strtoll pop or write
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								popsym = intsym;
								writesym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							comment_state = 45;
							mystate = 7;
						} else mystate = 8;
						break;
					// digit counter for state 46
					case 96:
						if (isspace(charhead)) {
							if (specacc->size > 1) {
								//strtoll pop
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								popsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							mystate = 48;
						} else if (isdigit(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 96;
						} else if (charhead == '>') {
							if (specacc->size > 1) {
								//strtoll pop
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								popsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							mystate = 49;
						} else if (charhead == '#') {
							if (specacc->size > 1) {
								//strtoll pop
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								popsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							comment_state = 48;
							mystate = 7;
						} else mystate = 8;
						break;
					// digit counter for states 49, 53
					case 97:
						if (isspace(charhead)) {
							if (specacc->size > 1) {
								//strtoll pushsym or write
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							} else {
								intsym = *(char *)Stack_pop(specacc);
							}

							if (pushptr) {
								writesym = intsym;
								writeptr = &writesym;
							} else if (writeptr) {
								pushsym = intsym;
								pushptr = &pushsym;
							}

							Stack_clear(specacc);
							mystate = 47;
						} else if (isdigit(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 97;
						} else if (charhead == ')') {
							if (specacc->size > 1) {
								//strtoll pushsym or write
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							} else {
								intsym = *(char *)Stack_pop(specacc);
							}

							if (pushptr) {
								writesym = intsym;
								writeptr = &writesym;
							} else if (writeptr) {
								pushsym = intsym;
								pushptr = &pushsym;
							}

							Stack_clear(specacc);
							mystate = 54;
						} else if (charhead == '#') {
							if (specacc->size > 1) {
								//strtoll pushsym or write
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							} else {
								intsym = *(char *)Stack_pop(specacc);
							}

							if (pushptr) {
								writesym = intsym;
								writeptr = &writesym;
							} else if (writeptr) {
								pushsym = intsym;
								pushptr = &pushsym;
							}

							Stack_clear(specacc);
							comment_state = 47;
							mystate = 7;
						} else mystate = 8;
						break;
					// digit counter for state 51
					case 98:
						if (isspace(charhead)) {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
								pushsym = intsym;
							}
								
							Stack_clear(specacc);
							mystate = 52;
						} else if (isdigit(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 98;
						} else if (charhead == ')') {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							mystate = 54;
						} else if (charhead == ',') {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							mystate = 53;
						} else if (charhead == '#') {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							comment_state = 52;
							mystate = 7;
						} else mystate = 8;
						break;
					// digit counter for state 55
					case 99:
						if (isspace(charhead)) {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							mystate = 56;
						} else if (isdigit(charhead)) {
							Stack_push(specacc, &charhead);
							mystate = 99;
						} else if (charhead == ')') {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
							}
								
							Stack_clear(specacc);
							mystate = 54;
						} else if (charhead == ',') {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							mystate = 53;
						} else if (charhead == '#') {
							if (specacc->size > 1) {
								//strtoll pushsym
								Stack_push(specacc, &nterm);
								special = specacc->elem;
								intsym = DECIMAL_IMPORT_FUNC;
								pushsym = intsym;
								
								//if (specacc->size > longest_sym) longest_sym = specacc->size;
							}
								
							Stack_clear(specacc);
							comment_state = 56;
							mystate = 7;
						} else mystate = 8;
						break;
					// COMMANDS
					case 200:
						/* TMP DISABLED 
						if (charhead == '(') {
							brackets = 1;
							j = i+1;
							k = 0;
							mystate = 101;
						} else*/ if (isnamechar(charhead)) { 
							Stack_push(characc, &charhead);
							mystate = 301;  // ingest variable
						} else mystate = 8;
						break;
						/* TMP DISABLED
					case 101:
						k++;
						if (charhead == '(') brackets++;
						else if (charhead == ')') {
							brackets--;
							if (brackets == 0) {
								struct State *cmdstate = &a0->states.block[State_add(a0, name)];
								if (cmdstate->cmd != NULL) {
									cmdstate->cmd = realloc(cmdstate->cmd, sizeof(char) * (strlen(cmdstate->cmd) + k));
									strncat(cmdstate->cmd, line+j, k-1);
								} else {
									cmdstate->cmd = malloc(sizeof(char) * k);
									cmdstate->cmd[0] = '\0';
									strncat(cmdstate->cmd, line+j, k-1);
								}
								mystate = 102;
								//for (int m = 0; cmdstate->cmd[m] != '\0'; m++) {
								//	putchar(cmdstate->cmd[m]);
								//}
								//putchar('\n');
								//printf("cmd: %s\n", cmdstate->cmd);
							} else { 
								//k++;
								mystate = 101;
							}
						} else if (charhead == '\n') {
							//struct State *cmdstate = State_add(a0, name);
							struct State *cmdstate = &a0->states.block[State_add(a0, name)];
							if (cmdstate->cmd != NULL) {
								cmdstate->cmd = realloc(cmdstate->cmd, sizeof(char) * (strlen(cmdstate->cmd)+k+1));
								strncat(cmdstate->cmd, line+j, k);
							} else {
								cmdstate->cmd = malloc(sizeof(char) * (k+1));
								cmdstate->cmd[0] = '\0';
								strncat(cmdstate->cmd, line+j, k);
							}
							j = i; j=0;
							k = 0;
							mystate = 101;
						} else {
							//k++;
							mystate = 101;
						}
						break;
					case 102:
						if (charhead == ';') {
							//mystate = 3;
							mystate = 103;
						} else if (isspace(charhead)) {
							//k = 0;
							mystate = 102;
						} else mystate = 8;
						break;
					case 103:
						int n = 0;
						k = 0;
						
						//struct State *cmdstate = State_add(a0, name);
						struct State *cmdstate = &a0->states.block[State_add(a0, name)];
						char *cmd = cmdstate->cmd;
						int brackets = 0;
						int arg_index = 0;
						
						int mycmdstate = 1;
						for (n = 0; cmd[n] != '\0'; n++) {
							//printf("\tmycmdstate: %d, cmd[%d]: %c\n", mycmdstate, n, cmd[n]);
							
							switch(mycmdstate) {
								case 1:
									//brackets = 0;
									if (isspace(cmd[n])) {
										mycmdstate = 1;
									} else if (cmd[n] == '"') {
										j = n + 1;
										k = 0;
										mycmdstate = 14;
									} else if (cmd[n] == '\'') {
										j = n + 1;
										k = 0;
										mycmdstate = 13;
									} else if (cmd[n] == '{') {
										brackets++;
										j = n + 1;
										k = 0;
										mycmdstate = 12;
									} else {
										j = n;
										k = 1;
										mycmdstate = 11;
									}
									break;
								// String begin: basic char
								case 11:
									if (isspace(cmd[n])) {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 1);
										arg_index++;
										mycmdstate = 1;
									} else if (cmd[n] == '{') {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 1);
										j = n + 1;
										k = 0;
										brackets++;
										mycmdstate = 22;
									} else if (cmd[n] == '\'') {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 1);
										j = n + 1;
										k = 0;
										mycmdstate = 23;
									} else if (cmd[n] == '"') {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 1);
										j = n + 1;
										k = 0;
										mycmdstate = 24;
									} else {
										k++;
										mycmdstate = 11;
									}
									break;
								// String begin: '}'
								case 12:
									if (cmd[n] == '{') brackets++;
									
									if (cmd[n] == '}' && --brackets == 0) {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 1);
										mycmdstate = 30;
									} else {
										k++;
										mycmdstate = 12;
									}
									break;
								// String begin: '\''
								case 13:
									if (cmd[n] == '\'') {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 1);
										mycmdstate = 30;
									} else {
										k++;
										mycmdstate = 13;
									}
									break;
								// String begin: '"'
								case 14:
									if (cmd[n] == '\"') {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 1);
										mycmdstate = 30;
									} else {
										k++;
										mycmdstate = 14;
									}
									break;
								// String middle: basic char
								case 21:
									if (isspace(cmd[n])) {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 0);
										arg_index++;
										mycmdstate = 1;
									} else if (cmd[n] == '{') {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 0);	
										j = n + 1;
										k = 0;
										brackets++;
										mycmdstate = 22;
									} else if (cmd[n] == '\'') {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 0);
										j = n + 1;
										k = 0;
										mycmdstate = 23;
									} else if (cmd[n] == '"') {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 0);
										j = n + 1;
										k = 0;
										mycmdstate = 24;
									} else {
										k++;
										mycmdstate = 21;
									}
									break;
								// String middle: '}'
								case 22:
									if (cmd[n] == '{') brackets++;
									
									if (cmd[n] == '}' && --brackets == 0) {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 0);
										mycmdstate = 30;
									} else {
										k++;
										mycmdstate = 22;
									}
									break;
								// String middle: '\''
								case 23:
									if (cmd[n] == '\'') {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 0);
										mycmdstate = 30;
									} else {
										k++;
										mycmdstate = 23;
									}
									break;
								// String middle: '"'
								case 24:
									if (cmd[n] == '"') {
										State_cmd_arg_add(cmdstate, arg_index, j, k, 0);
										mycmdstate = 30;
									} else {
										k++;
										mycmdstate = 24;
									}
									break;	
								// String end
								case 30:
									if (isspace(cmd[n])) {
										arg_index++;
										mycmdstate = 1;
									} else if (cmd[n] == '{') {										
										j = n + 1;
										k = 0;
										brackets++;
										mycmdstate = 22;
									} else if (cmd[n] == '\'') {
										j = n + 1;
										k = 0;
										mycmdstate = 23;
									} else if (cmd[n] == '"') {
										j = n + 1;
										k = 0;
										mycmdstate = 24;
									} else {
										j = n;
										k = 1;
										mycmdstate = 21;
									}
									break;
							}	
							//printf("\t brackets: %d\n", brackets);		
						}
						//printf("\tmycmdstate: %d, cmd[%d]: %c\n", mycmdstate, n, cmd[n]);
						
						if (mycmdstate == 11 ) {
							State_cmd_arg_add(cmdstate, arg_index, j, k, 1);
							arg_index++;
						} else if (mycmdstate == 21) {
							State_cmd_arg_add(cmdstate, arg_index, j, k, 0);
							arg_index++;
						} else if (mycmdstate == 30 || mycmdstate == 22 || mycmdstate == 23 || mycmdstate == 24 ) {
							cmdstate->cmd_args = realloc(cmdstate->cmd_args, sizeof(char *) * (arg_index+2));
							if (cmdstate->cmd_args == NULL) {
								fprintf(stderr, "Error allocating new pointer to command args\n");
								exit(EXIT_FAILURE);
							}
							arg_index++;
						} 
						
						cmdstate->cmd_args[arg_index] = (char *)NULL;
						mystate = 3;
						
						// test
						//for (int n = 0; cmdstate->cmd_args[n] != NULL; n++) 
						//	printf("\t\t%s argv[%d]: '%s'\n", cmdstate->name, n, cmdstate->cmd_args[n]);
						//break;
						break;
*/

					// VARIABLES (transition input)
					
					// PROCESS VARIABLE NAME
					// Ensure var is at least one legal char
					case 300:
						if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							
							mystate = 301;
						} else mystate = 8;
						break;
					// process var name length
					case 301:
						if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							mystate = 301;
						} else if (isspace(charhead)) {
							mystate = 302;
						} else if (charhead == '>') {
							Stack_push(characc, &nterm);
							special = characc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(vsymstack, symptr);
								}

								mystate = 5;
							} else {
								fprintf(stderr, "Undeclared variable $%s used in transition\n", special);
								mystate = 8;
							}
							Stack_clear(characc);
						} else if (charhead == ',') {
							Stack_push(characc, &nterm);
							special = characc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(vsymstack, symptr);
								}

								mystate = 17;
							} else {
								fprintf(stderr, "Undeclared variable $%s used in transition\n", special);
								mystate = 8;
							}
							Stack_clear(characc);
						} else if (charhead == '#') {
						  comment_state = 302;
						  mystate = 7;
						} else mystate = 8;
						break;
					// finish getting var name, add var syms to transition
					case 302:
						if (isspace(charhead)) {
							mystate = 302;
						} else if (charhead == '>') {
							Stack_push(characc, &nterm);
							special = characc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(vsymstack, symptr);
								}

								mystate = 5;
							} else {
								fprintf(stderr, "Undeclared variable $%s used in transition\n", special);
								mystate = 8;
							}
							Stack_clear(characc);
						} else if (charhead == ',') {
							Stack_push(characc, &nterm);
							special = characc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(vsymstack, symptr);
								}
								mystate = 17;
							} else {
								fprintf(stderr, "Undeclared variable $%s used in transition\n", special);
								mystate = 8;
							}
							Stack_clear(characc);
						} else if (charhead == '#') {
							comment_state = 302;
							mystate = 7;
						} else mystate = 8;
						break;

					// NEW VARIABLE DECLARATION
					// begin ingesting variable syms & vars
					// allow spaces before first sym ie VAR = a,...
					// > Here, the characc stack accumulates variable edits
					// > Here, the specacc stack accumulates reference $variable names
					case 400:
						if (isspace(charhead)) {
							mystate = 400;
						} else if (isdigit(charhead) || charhead == '-') {
							Stack_push(minuend, &charhead);
							Stack_push(characc, &charhead);
							Stack_push(specacc, &charhead);
							mystate = 420;
						} else if (isnamechar(charhead)) {
							Stack_push(minuend, &charhead);
							Stack_push(characc, &charhead);
							mystate = 401;
						} else if (charhead == '$' ) {
							Stack_push(characc, &charhead);
							mystate = 403;
						} else if (charhead == '\'') {
							Stack_push(characc, &charhead);
							mystate = 410;
						} else if (charhead == '#') {
							comment_state = 400;
							mystate = 7;
						} else mystate = 8;
						break;
					// minuend/newvar: allow spaces before comma, semicolon
					case 401:
						if (isspace(charhead)) {
							mystate = 401;
						} else if (charhead == ',') {
							Stack_push(characc, &charhead);
							mystate = 400;					// process next sym
						} else if (charhead == ';') {
							Stack_push(characc, &nterm);
							newedit = characc->elem;
							Var_edit_add(a0, newvar, newedit);
							Stack_clear(characc);

							Stack_clear(&newvar->syms);
							for (unsigned int f = 0; f < minuend->size; f++) {
								symptr = Stack_get(minuend, f);
								Stack_push(&newvar->syms, symptr);
							}

							Stack_clear(minuend);
							mystate = 3;
						// end minuend ingest, begin ingesting subtrahend
						} else if (charhead == '\\') {
							Stack_push(characc, &charhead);
							mystate = 405;
						} else if (charhead == '#') {
							comment_state = 401;
							mystate = 7;
						} else mystate = 8;
						break;
					// VAR = $OTHERVAR; |OR|
					// VAR = $OTHERVAR,$ANOTHERVAR; |OR|
					// VAR = $OTHERVAR - $ANOTHERVAR; |OR|
					// VAR = $OTHERVAR - A, B , $ANOTHERVAR, C, D; |OR|
					// VAR = $OTHERVAR,$EXTRA,B - $ANOTHERVAR,C,D;
					// gather minuend var chars
					// ensure minuend var is at least one char
					case 403:
						if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							Stack_push(specacc, &charhead);
							
							mystate = 404;
						} else mystate = 8;
						break;
					// gather remaining minuend variable name chars
					case 404:
						if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							Stack_push(specacc, &charhead);
							mystate = 404;
						} else if (isspace(charhead)) {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(minuend, symptr);
								}

								mystate = 401;
							} else {
								fprintf(stderr, "Undeclared variable $%s used as operand\n", special);
								mystate = 8;
							}
							Stack_clear(specacc);
						// other var is first in a list of vars or chars
						// assume for now it is part of a minuend
						} else if (charhead == ',') {
							Stack_push(characc, &charhead);
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(minuend, symptr);
								}

								mystate = 400;
							} else {
								fprintf(stderr, "Undeclared variable $%s used as operand\n", special);
								mystate = 8;
							}
							Stack_clear(specacc);
						// mineuend - subtrahend = difference
						// other var is here confirmed a minuend, indicating a subtrahend is coming up
						} else if (charhead == '\\') {
							Stack_push(characc, &charhead);
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(minuend, symptr);
								}
								
								mystate = 405;
							} else {
								fprintf(stderr, "Undeclared variable $%s used as operand\n", special);
								mystate = 8;
							}
							Stack_clear(specacc);
						// newvar is simply a copy of another var;
						} else if (charhead == ';') {
							Stack_push(characc, &nterm);
							newedit = characc->elem;
							Var_edit_add(a0, newvar, newedit);
							Stack_clear(characc);

							Stack_push(specacc, &nterm);
							special = specacc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(minuend, symptr);
								}

								Stack_clear(&newvar->syms);
								for (unsigned int f = 0; f < minuend->size; f++) {
									symptr = Stack_get(minuend, f);
									Stack_push(&newvar->syms, symptr);
								}
								Stack_clear(minuend);

								mystate = 3;
							} else {
								fprintf(stderr, "Undeclared variable $%s used as operand\n", special);
								mystate = 8;
							}
							Stack_clear(specacc);
						} else if (charhead == '#') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(minuend, symptr);
								}

								comment_state = 401;
								mystate = 7;
							} else {
								fprintf(stderr, "Undeclared variable $%s used as operand\n", special);
								mystate = 8;
							}
							Stack_clear(specacc);
						} else mystate = 8;
						break;
					// gather subtrahend
					// may consist of a var, char, or comma-separated list of chars and vars
					case 405:
						if (isspace(charhead)) {
							mystate = 405;
						} else if (charhead == '$') {
							Stack_push(characc, &charhead);
							mystate = 406;
						} else if (charhead == '\'') {
							Stack_push(characc, &charhead);
							mystate = 412;
						} else if (isdigit(charhead) || charhead == '-') {
							Stack_push(characc, &charhead);
							Stack_push(specacc, &charhead);
							Stack_push(subtrahend, &charhead);
							mystate = 425;
						} else if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							Stack_push(subtrahend, &charhead);
							mystate = 414;
						} else if (charhead == '#') {
							comment_state = 405;
							mystate = 7;
						} else mystate = 8;
						break;
					// subtrahend var: first char of var name
					case 406:
						if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							Stack_push(specacc, &charhead);
							
							mystate = 407;
						} else mystate = 8;
						break;
					// subtrahend var: rest of chars of var name
					case 407:
						if (isnamechar(charhead)) {
							Stack_push(characc, &charhead);
							Stack_push(specacc, &charhead);
							
							mystate = 407;
						} else if (isspace(charhead)) {
							mystate = 408;
						// var is an element of a comma-separated list of subtrahends
						} else if (charhead == ',') {
							Stack_push(characc, &charhead);
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(subtrahend, symptr);
								}

								mystate = 405;
							} else {
								fprintf(stderr, "Undeclared variable $%s used as operand\n", special);
								mystate = 8;
							}
							Stack_clear(specacc);
						// var marks end of subtrahend and variable declaration
						} else if (charhead == ';') {
							Stack_push(characc, &nterm);
							newedit = characc->elem;
							Var_edit_add(a0, newvar, newedit);
							Stack_clear(characc);

							Stack_push(specacc, &nterm);
							special = specacc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(subtrahend, symptr);
								}

								Stack_clear(&newvar->syms);
								// minuend - subtrahend here
								for (unsigned int f = 0; f < minuend->size; f++) {
									symptr = Stack_get(minuend, f);
									int exists = 0;
									for (unsigned int g = 0; g < subtrahend->size; g++) {
										extraptr = Stack_get(subtrahend, g);
										if (*symptr == *extraptr) {
											exists = 1;
											break;
										}
									}
									if (!exists) Stack_push(&newvar->syms, symptr);
								}
								Stack_clear(minuend);
								Stack_clear(subtrahend);
								mystate = 3;
							} else {
								fprintf(stderr, "Undeclared variable $%s used as operand\n", special);
								mystate = 8;
							}
							Stack_clear(specacc);
						} else if (charhead == '#') {
							comment_state = 408;
							mystate = 7;
						} else mystate = 8;
						break;
					// subtrahend var: trailing spaces
					case 408:
						if (isspace(charhead)) {
							mystate = 408;
						} else if (charhead == ',') {
							Stack_push(characc, &charhead);
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(subtrahend, symptr);
								}
								mystate = 405;
							} else {
								fprintf(stderr, "Undeclared variable $%s used as operand\n", special);
								mystate = 8;
							}
							Stack_clear(specacc);
						// end of subtrahend and variable declaration
						} else if (charhead == ';') {
							Stack_push(characc, &nterm);
							newedit = characc->elem;
							Var_edit_add(a0, newvar, newedit);
							Stack_clear(characc);
							
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							argvar = Var_get(a0, special);

							if (argvar) {
								for (unsigned int f = 0; f < argvar->syms.size; f++) {
									symptr = Stack_get(&argvar->syms, f);
									Stack_push(subtrahend, symptr);
								}
							
								Stack_clear(&newvar->syms);
								// minuend - subtrahend here
								for (unsigned int f = 0; f < minuend->size; f++) {
									symptr = Stack_get(minuend, f);
									int exists = 0;
									for (unsigned int g = 0; g < subtrahend->size; g++) {
										extraptr = Stack_get(subtrahend, g);
										if (*symptr == *extraptr) {
											exists = 1;
											break;
										}
									}
									if (!exists) Stack_push(&newvar->syms, symptr);
								}
								Stack_clear(minuend);
								Stack_clear(subtrahend);
								mystate = 3;
							} else {
								fprintf(stderr, "Undeclared variable $%s used as operand\n", special);
								mystate = 8;
							}
							Stack_clear(specacc);
						} else if (charhead == '#') {
							comment_state = 408;
							mystate = 7;
						} else mystate = 8;
						break;
					// minuend: explict quoted char 'x'
					case 410:
						Stack_push(characc, &charhead);
						Stack_push(minuend, &charhead);
						mystate = 411;
						break;
					case 411:
						if (charhead == '\'') {
							Stack_push(characc, &charhead);
							mystate = 401;
						} else mystate = 8;
						break;
					// subtrahend: explict quoted char 'x'
					case 412:
						Stack_push(characc, &charhead);
						Stack_push(subtrahend, &charhead);
						mystate = 413;
						break;
					case 413:
						if (charhead == '\'') {
							Stack_push(characc, &charhead);
							mystate = 414;
						} else mystate = 8;
						break;
					case 414:
						if (isspace(charhead)) {
							mystate = 414;
						} else if (charhead == ',') {
							Stack_push(characc, &charhead);
							mystate = 405;
						} else if (charhead == ';') {
							Stack_push(characc, &nterm);
							newedit = characc->elem;
							Var_edit_add(a0, newvar, newedit);
							Stack_clear(characc);
							Stack_clear(&newvar->syms);

							// minuend - subtrahend here
							for (unsigned int f = 0; f < minuend->size; f++) {
								symptr = Stack_get(minuend, f);
								int exists = 0;
								for (unsigned int g = 0; g < subtrahend->size; g++) {
									extraptr = Stack_get(subtrahend, g);
									if (*symptr == *extraptr) {
										exists = 1;
										break;
									}
								}
								if (!exists) Stack_push(&newvar->syms, symptr);
							}
							Stack_clear(minuend);
							Stack_clear(subtrahend);
							
							mystate = 3;
						} else if (charhead == '#') {
							comment_state = 414;
							mystate = 7;
						} else mystate = 8;
						break;
					// int sym in minuend
					// 420-422: digit sym accumulator for state 400
					// single digit, treat as ascii
					case 420:
						if (isdigit(charhead)) {
							Stack_pop(minuend);
							Stack_push(characc, &charhead);
							Stack_push(specacc, &charhead);
							mystate = 421;
						} else if (isspace(charhead)) {
							Stack_clear(specacc);

							mystate = 401;
						} else if (charhead == ',') {
							Stack_clear(specacc);

							Stack_push(characc, &charhead);
							mystate = 400;					// process next sym
						} else if (charhead == ';') {
							Stack_clear(specacc);
						
							Stack_push(characc, &nterm);
							newedit = characc->elem;
							Var_edit_add(a0, newvar, newedit);
							Stack_clear(characc);

							Stack_clear(&newvar->syms);
							for (unsigned int f = 0; f < minuend->size; f++) {
								symptr = Stack_get(minuend, f);
								Stack_push(&newvar->syms, symptr);
							}

							Stack_clear(minuend);
							mystate = 3;
						// end minuend ingest, begin ingesting subtrahend
						} else if (charhead == '\\') {
							Stack_clear(specacc);
						
							Stack_push(characc, &charhead);
							mystate = 405;
						} else if (charhead == '#') {
							comment_state = 401;
							mystate = 7;
						} else mystate = 8;
						break;
					// count all digits, convert to int type
					case 421:
						if (isdigit(charhead)) {
							Stack_push(characc, &charhead);
							Stack_push(specacc, &charhead);
							mystate = 421;
						} else if (isspace(charhead)) {
							mystate = 422;
						} else if (charhead == ',') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							//if (specacc->size > longest_sym) longest_sym = specacc->size;
							
							Stack_push(minuend, &intsym);
							Stack_clear(specacc);

							Stack_push(characc, &charhead);
							mystate = 400;					// process next sym
						} else if (charhead == ';') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							intsym = DECIMAL_IMPORT_FUNC;	
							//if (specacc->size > longest_sym) longest_sym = specacc->size;

							Stack_push(minuend, &intsym);
							Stack_clear(specacc);
						
							Stack_push(characc, &nterm);
							newedit = characc->elem;
							Var_edit_add(a0, newvar, newedit);
							Stack_clear(characc);

							Stack_clear(&newvar->syms);
							for (unsigned int f = 0; f < minuend->size; f++) {
								symptr = Stack_get(minuend, f);
								Stack_push(&newvar->syms, symptr);
							}

							Stack_clear(minuend);
							mystate = 3;
						// end minuend ingest, begin ingesting subtrahend
						} else if (charhead == '\\') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							//if (specacc->size > longest_sym) longest_sym = specacc->size;
							
							Stack_push(minuend, &intsym);
							Stack_clear(specacc);
						
							Stack_push(characc, &charhead);
							mystate = 405;
						} else if (charhead == '#') {
							comment_state = 422;
							mystate = 7;
						} else mystate = 8;

						break;
					// Spaces following int sym digits
					case 422:
						if (isspace(charhead)) {
							mystate = 422;
						} else if (charhead == ',') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							//if (specacc->size > longest_sym) longest_sym = specacc->size;
							
							Stack_push(minuend, &intsym);
							Stack_clear(specacc);

							Stack_push(characc, &charhead);
							mystate = 400;					// process next sym
						} else if (charhead == ';') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							//if (specacc->size > longest_sym) longest_sym = specacc->size;
							
							Stack_push(minuend, &intsym);
							Stack_clear(specacc);
						
							Stack_push(characc, &nterm);
							newedit = characc->elem;
							Var_edit_add(a0, newvar, newedit);
							Stack_clear(characc);

							Stack_clear(&newvar->syms);
							for (unsigned int f = 0; f < minuend->size; f++) {
								symptr = Stack_get(minuend, f);
								Stack_push(&newvar->syms, symptr);
							}

							Stack_clear(minuend);
							mystate = 3;
						// end minuend ingest, begin ingesting subtrahend
						} else if (charhead == '\\') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							//if (specacc->size > longest_sym) longest_sym = specacc->size;
							
							Stack_push(minuend, &intsym);
							Stack_clear(specacc);
						
							Stack_push(characc, &charhead);
							mystate = 405;
						} else if (charhead == '#') {
							comment_state = 422;
							mystate = 7;
						} else mystate = 8;
						break;
					// int sym in subtrahend: 425-427
					// single-digit ascii sym
					case 425:
						if (isdigit(charhead)) {
							Stack_pop(subtrahend);
							Stack_push(characc, &charhead);
							Stack_push(specacc, &charhead);
							mystate = 426;
						} else if (isspace(charhead)) {
							mystate = 414;
						} else if (charhead == ',') {
							Stack_push(characc, &charhead);
							mystate = 405;
						} else if (charhead == ';') {
							Stack_push(characc, &nterm);
							newedit = characc->elem;
							Var_edit_add(a0, newvar, newedit);
							Stack_clear(characc);
							Stack_clear(&newvar->syms);

							// minuend - subtrahend here
							for (unsigned int f = 0; f < minuend->size; f++) {
								symptr = Stack_get(minuend, f);
								int exists = 0;
								for (unsigned int g = 0; g < subtrahend->size; g++) {
									extraptr = Stack_get(subtrahend, g);
									if (*symptr == *extraptr) {
										exists = 1;
										break;
									}
								}
								if (!exists) Stack_push(&newvar->syms, symptr);
							}
							Stack_clear(minuend);
							Stack_clear(subtrahend);
							
							mystate = 3;
						} else if (charhead == '#') {
							comment_state = 414;
							mystate = 7;
						} else mystate = 8;
						break;
					// count digits in int sym
					case 426:
						if (isdigit(charhead)) {
							Stack_push(characc, &charhead);
							Stack_push(specacc, &charhead);
							mystate = 426;
						} else if (isspace(charhead)) {
							mystate = 427;
						} else if (charhead == ',') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							//if (specacc->size > longest_sym) longest_sym = specacc->size;
							
							Stack_push(subtrahend, &intsym);
							Stack_clear(specacc);

							Stack_push(characc, &charhead);
							mystate = 405;
						} else if (charhead == ';') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							//if (specacc->size > longest_sym) longest_sym = specacc->size;
							
							Stack_push(subtrahend, &intsym);
							Stack_clear(specacc);

							Stack_push(characc, &nterm);
							newedit = characc->elem;
							Var_edit_add(a0, newvar, newedit);
							Stack_clear(characc);
							Stack_clear(&newvar->syms);

							// minuend - subtrahend here
							for (unsigned int f = 0; f < minuend->size; f++) {
								symptr = Stack_get(minuend, f);
								int exists = 0;
								for (unsigned int g = 0; g < subtrahend->size; g++) {
									extraptr = Stack_get(subtrahend, g);
									if (*symptr == *extraptr) {
										exists = 1;
										break;
									}
								}
								if (!exists) Stack_push(&newvar->syms, symptr);
							}
							Stack_clear(minuend);
							Stack_clear(subtrahend);
							
							mystate = 3;
						} else if (charhead == '#') {
							comment_state = 427;
							mystate = 7;
						} else mystate = 8;
						break;
					case 427:
						if (isspace(charhead)) {
							mystate = 427;
						} else if (charhead == ',') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							//if (specacc->size > longest_sym) longest_sym = specacc->size;
							
							Stack_push(subtrahend, &intsym);
							Stack_clear(specacc);

							Stack_push(characc, &charhead);
							mystate = 405;
						} else if (charhead == ';') {
							Stack_push(specacc, &nterm);
							special = specacc->elem;
							intsym = DECIMAL_IMPORT_FUNC;
							//if (specacc->size > longest_sym) longest_sym = specacc->size;
							
							Stack_push(subtrahend, &intsym);
							Stack_clear(specacc);

							Stack_push(characc, &nterm);
							newedit = characc->elem;
							Var_edit_add(a0, newvar, newedit);
							Stack_clear(characc);
							Stack_clear(&newvar->syms);

							// minuend - subtrahend here
							for (unsigned int f = 0; f < minuend->size; f++) {
								symptr = Stack_get(minuend, f);
								int exists = 0;
								for (unsigned int g = 0; g < subtrahend->size; g++) {
									extraptr = Stack_get(subtrahend, g);
									if (*symptr == *extraptr) {
										exists = 1;
										break;
									}
								}
								if (!exists) Stack_push(&newvar->syms, symptr);
							}
							Stack_clear(minuend);
							Stack_clear(subtrahend);
							
							mystate = 3;
						} else if (charhead == '#') {
							comment_state = 427;
							mystate = 7;
						} else mystate = 8;
						break;
				}
				linechar++;
				// DEBUG:
				//printf("inputsym: %zd, popsym: %zd, pushsym: %zd, writesym: %zd\n", symbol, popsym, pushsym, writesym);
			}
	line_end:
			linenum++;
			// DEBUG:
			//printf("mystate: %3d | line: %4d | char: %3d | j: %2d | k: %2d | val: %c \n", mystate, linenum, i, j, k, line[i]);
	}

	Stack_free(characc);
	Stack_free(specacc);
	Stack_free(symstack);
	Stack_free(vsymstack);
	Stack_free(minuend);
	Stack_free(subtrahend);

	fclose(fp);

	if (mystate != 3 && mystate != 1) {
		// Offer info to find unmatched multiline comment
		if (mystate == 12 || mystate == 13) {
			fprintf(stderr, "Unmatched multiline comment started on line %u, char %u\n",
					mlcmt_line, mlcmt_char);
		} else {
			// Show approximate line/char that caused import to fail
			char highlight=line[linechar-2];
			if (highlight == '\n') highlight = ' ';
			fprintf(stderr,"Error on or near line %u, char [%u]:\n%.*s[%c]%s", 
					linenum, linechar -1, linechar-2, line, highlight, line+linechar-1);
			if (line[linechar-2] == '\n') putchar('\n');
		}

		free(line);
		Automaton_free(a0);
		exit(EXIT_FAILURE);
	}
			
	free(line);
	
	if (a0->start == NULL) {
		// Add a state if none exist (empty language!)
		if (a0->states.size == 0) State_add(a0, "q0");
		
		// Try first to set start state to parent state of first transition in block
		if (a0->trans.size > 0) {
			//newtrans = Block_addr(&a0->trans, 0);
			struct Stack *datastacks = a0->trans.data.elem;
			struct Stack *strip = &datastacks[0];
			struct Trans *strip_elem = strip->elem;
			newtrans = &strip_elem[0];
			
			set_start(a0, newtrans->pstate);
			fprintf(stderr, "Warning: No start state specified, assuming '%s'\n", 
					newtrans->pstate->name);
		
		// If no transitions, set start state to first state in block
		} else {
			//newstate = Block_addr(&a0->states, 0);
			struct Stack *datastacks = a0->states.data.elem;
			struct Stack *strip = &datastacks[0];
			struct State *strip_elem = strip->elem;
			newstate = &strip_elem[0];

			set_start(a0, newstate);
			fprintf(stderr, "Warning: No start state specified, assuming '%s'\n",
					newstate->name);	
		}
	}
	
	if (a0->finals.elem == NULL) {
		fprintf(stderr, "Warning: No final states specified\n");
	}
	
	// Sort states in alpha order
	//qsort(a0->states.mem+1, a0->states.len-1, sizeof(struct State), namecmp);// State_compare);

	// Determine alphabet
	qsort(alphabet->elem, alphabet->size, sizeof(CELL_TYPE), symcmp);
	extra = 0;
	for (unsigned int i = 0; i < alphabet->size; i++) {
		symptr = Stack_get(alphabet, i);
		if (!i || *symptr != extra) {
			Stack_push(&machine.alphabet, symptr);
		}
		extra = *symptr;
	}
	Stack_free(alphabet);

	// Account for null term
	longest_name--;
	//longest_sym--;

	return machine;
}

// Order strings by alpha order, then digit order
// > yes:  q1, q2,   q100, q101
//    no:  q1, q100, q101, q2
int alnumcmp(const char *s1, const char *s2)
{
	for (;;) {
		if (*s2 == '\0')
			return *s1 != '\0';
		else if (*s1 == '\0')
			return 1;
		else if (!(isdigit(*s1) && isdigit(*s2))) {
			if (*s1 != *s2)
				return (int)*s1 - (int)*s2;
			else
				(++s1, ++s2);
		} else {
			char *lim1, *lim2;
			unsigned long n1 = strtoul(s1, &lim1, 10);
			unsigned long n2 = strtoul(s2, &lim2, 10);
			if (n1 > n2)
				return 1;
			else if (n1 < n2)
				return -1;
			s1 = lim1;
			s2 = lim2;
		}
	}
}

// Order transitions according to:
// 1. Parent state name alnumcp
// 2. Destination state name alnumcmp
// 3. Symbol cmp
//
int transcmp(const void *a, const void *b)
{
	const struct Trans *t1 = *(struct Trans **)a;
	const struct Trans *t2 = *(struct Trans **)b;

	if (!t1 && !t2) return 0;
	if (!t1) return 1;
	if (!t2) return -1;

	int namecmp = alnumcmp(t1->pstate->name, t2->pstate->name);
	if (!namecmp) {
		int destcmp = alnumcmp(t1->dstate->name, t2->dstate->name);
		if (!destcmp) {
			int symcmp = t1->inputsym - t2->inputsym;
			return symcmp;
		}
		return destcmp;
	}
	return namecmp;
}

// Prints a single symbol within a transition
// The symtype flag specifies which symbol to print:
// > all chars printed unquoted
//
// Returns number of chars printed (printf return value)
//int Trans_sym_print(ptrdiff_t sym)
int Trans_sym_print(CELL_TYPE sym)
{
	if (sym > 31 && sym < 127) {
		return printf("%c", (char)sym);
	} else if (sign) {
		//return printf("%02td", sym);
		return printf("%02" SIGNED_PRINT_FMT, (SIGNED_PRINT_TYPE)sym);
	} else {
		return printf("%02" UNSIGNED_PRINT_FMT, (UNSIGNED_PRINT_TYPE)sym);
	}
}

// Conf sym print
// > quotes printable syms that are reserved tmf chars
//
// Returns number of chars printed (printf return value)
//int Trans_sym_conf_print(ptrdiff_t sym)
int Trans_sym_conf_print(CELL_TYPE sym)
{
	// 'Printable' ASCII
	// > Symbols in this range will always be printed as ASCII chars
	// > Maybe in the future I'll add trans flags to ensure syms
	//   are printed in the manner which they were defined in the 
	//   machine file	(ie. 65 printing back as 65 and not as A)
	// > Octal/Hex representations in the future?
	if (sym > 31 && sym < 127) {
		
		if (isnamechar(sym)) {
			return printf("%c", (char)sym);

		// Put single quotes around tmf control/reserved characters (';',':','$','>',... etc.)
		// See TOKEN variable in spec.tmf for full list of control chars
		} else {
			return printf("'%c'", (char)sym);
		}

	// Treat unprintable syms as integers (CELL_TYPE/ptrdiff_t)
	// > Single digit ints in tmf must be preceded by a '0' to
	//   be imported as their implied integer values, otherwise 
	//   they are stored as their ASCII decimal codes (ie. A -> 65)
	} else if (sign) {
		return printf("%02" SIGNED_PRINT_FMT, (SIGNED_PRINT_TYPE)sym);
		//return printf("%02td", sym);
	} else {
		return printf("%02" UNSIGNED_PRINT_FMT, (UNSIGNED_PRINT_TYPE)sym);
	}
}

// Runtime sym print
// > maintains consistent spacing w/ quoted syms and
//   multi-character decimal syms
//
// Returns number of chars printed (printf return value)
//int Trans_sym_run_print(ptrdiff_t sym)
int Trans_sym_run_print(CELL_TYPE sym)
{
	if (sym > 31 && sym < 127) {
		char tmp[4];
		if (isnamechar(sym)) {
			tmp[0] = ' ';
			tmp[1] = (char)sym;
			tmp[2] = ' ';
			tmp[3] = '\0';
			//return printf("%*c", longest_sym, (char)sym);
		} else {
			tmp[0] = '\'';
			tmp[1] = (char)sym;
			tmp[2] = '\'';
			tmp[3] = '\0';
			//return printf("'%*c'", longest_sym, (char)sym);
		}
		return printf("%*s", longest_sym, tmp);
	//} else if (sym >= 0 && sym < 10) {
	} else if (!sym || (sym > 0 && sym < 10)) {  // avoiding compiler warning :/
		char tmp[3];
		tmp[0] = '0';
		tmp[1] = (char)(sym+48);
		tmp[2] = '\0';
		return printf("%*s", longest_sym, tmp);
	} else if (sign) {
		return printf("%*" SIGNED_PRINT_FMT, longest_sym, (SIGNED_PRINT_TYPE)sym);
	} else {
		return printf("%*" UNSIGNED_PRINT_FMT, longest_sym, (UNSIGNED_PRINT_TYPE)sym);
	}

}

void Trans_print(struct Trans *trans)
{
	printf("%*s: ", longest_name, trans->pstate->name);
	if (!trans->epsilon) {
		//Trans_sym_conf_print(trans->inputsym);
		Trans_sym_run_print(trans->inputsym);
		printf(" > %-*s ", longest_name, trans->dstate->name);
	}
	else { 
		printf("    > %-*s ", longest_name, trans->dstate->name);
	}

	if (trans->write) {
		putchar('(');
		Trans_sym_conf_print(trans->writesym);
		if (trans->dir == 1) {
			printf(",L");
		} else if (trans->dir == 2) {
			printf(",R");
		}
		if (trans->pop) {
			putchar(',');
			Trans_sym_conf_print(trans->popsym);
			putchar('>');

			if (trans->push)
				Trans_sym_conf_print(trans->pushsym);
		} else if (trans->push) {
			printf(",>");
			Trans_sym_conf_print(trans->pushsym);
		}
		putchar(')');

	} else if (trans->dir == 1) {
		printf("(L");
		if (trans->pop) {
			putchar(',');
			Trans_sym_conf_print(trans->popsym);
			putchar('>');

			if (trans->push)
				Trans_sym_conf_print(trans->pushsym);
		} else if (trans->push) {
			printf(",>");
			Trans_sym_conf_print(trans->pushsym);
		}
		putchar(')');

	} else if (trans->dir == 2) {
		printf("(R");
		if (trans->pop) {
			putchar(',');
			Trans_sym_conf_print(trans->popsym);
			putchar('>');

			if (trans->push)
				Trans_sym_conf_print(trans->pushsym);
		} else if (trans->push) {
			printf(",>");
			Trans_sym_conf_print(trans->pushsym);
		}
		putchar(')');

	} else if (trans->pop) {
		putchar('(');
		Trans_sym_conf_print(trans->popsym);
		putchar('>');

		if (trans->push)
			Trans_sym_conf_print(trans->pushsym);
		putchar(')');

	} else if (trans->push) {
		printf("(>");
		Trans_sym_conf_print(trans->pushsym);
		putchar(')');
	}
	if (trans->dstate->final) printf(" |F|");
	else if (trans->dstate->reject) printf( " |R|");
}

// Print extended operations of a transition
// > Anything within parens following destination state
// > Pop, push, direction, write
//
//
int Trans_ext_print(struct Trans *trans) {

	int len = 0;
	if (trans->write) {
		putchar('('); len++;
		len += Trans_sym_conf_print(trans->writesym);
		if (trans->dir == 1) {
			len += printf(",L");
		} else if (trans->dir == 2) {
			len += printf(",R");
		}
		if (trans->pop) {
			putchar(','); len++;
			len += Trans_sym_conf_print(trans->popsym);
			putchar('>'); len++;

			if (trans->push)
				len += Trans_sym_conf_print(trans->pushsym);
		} else if (trans->push) {
			len += printf(",>");
			len += Trans_sym_conf_print(trans->pushsym);
		}
		putchar(')'); len++;

	} else if (trans->dir == 1) {
		len += printf("(L");
		if (trans->pop) {
			putchar(','); len++;
			len += Trans_sym_conf_print(trans->popsym);
			putchar('>'); len++;

			if (trans->push)
				len += Trans_sym_conf_print(trans->pushsym);
		} else if (trans->push) {
			len += printf(",>");
			len += Trans_sym_conf_print(trans->pushsym);
		}
		putchar(')'); len++;

	} else if (trans->dir == 2) {
		len += printf("(R");
		if (trans->pop) {
			putchar(','); len++;
			len += Trans_sym_conf_print(trans->popsym);
			putchar('>'); len++;

			if (trans->push)
				len += Trans_sym_conf_print(trans->pushsym);
		} else if (trans->push) {
			len += printf(",>");
			len += Trans_sym_conf_print(trans->pushsym);
		}
		putchar(')'); len++;

	} else if (trans->pop) {
		putchar('('); len++;
		len += Trans_sym_conf_print(trans->popsym);
		putchar('>'); len++;

		if (trans->push)
			len += Trans_sym_conf_print(trans->pushsym);
		putchar(')'); len++;

	} else if (trans->push) {
		len += printf("(>");
		len += Trans_sym_conf_print(trans->pushsym);
		putchar(')'); len++;
	}

	return len;
}

// Print automaton to console
// > States with no transitions (that are not parent
//   states of any transitions) are ommitted
//   --> Unreachable states with transitions will still
//       be printed
// > Automatons generated via regex are sorted first, as
//   the Trans Block order generated by nfa_to_regex() is
//   somewhat incomprehensible.
// > Print order closely follows the original conf file's order
//   --> Adjacent similar transitions will be 'collapsed' into lists
//       of either similar symbols or similar destinations, with
//       the former taking priority over the latter.
//   --> Similar transitions that are not adjacent to one another
//       in the original conf file will not be added to any 
//       'relevant' existing lists.
//
void Automaton_print(struct Automaton *a0, int format)
{

	// Print start state.
	printf("start: %s;\n", a0->start->name);

	// Print final states.
	if (a0->finals.elem) {
		struct State **finals = a0->finals.elem;
		printf("final: %s", finals[0]->name);
		for (size_t i = 1; i < a0->finals.size; i++)
			if (finals[i]->final) printf(",%s", finals[i]->name);
		printf(";\n");
	}

	// Print reject states.
	if (a0->rejects.elem) {
		struct State **rejects = a0->rejects.elem;
		printf("reject: %s", rejects[0]->name);
		for (size_t i = 1; i < a0->rejects.size; i++)
			if (rejects[i]->reject) printf(",%s", rejects[i]->name);
		printf(";\n");
	}

	// Print custom tm blank char if not default.
	// Default: '_' (underscore)
	if (tm_blank != '_') {
		printf("blank: ");
		//(isnamechar(tm_blank)) ? Trans_sym_print(tm_blank) : printf("'%c'", tm_blank);
		Trans_sym_conf_print(tm_blank);
		printf(";\n");
	}

	// Print tm tape bounds params if not default.
	// > Default: infinite tape in both directions, no boundary reject conditions
	if (tm_bound != '\0') {
		printf("bound: %c", tm_bound);
		(tm_bound_halt) ? printf(",H;\n") : printf(";\n");
	}

	// End printing if Trans Block size is zero.
	if (!a0->trans.size) return;

	// Sort copy of **hash array in Trans Block
	// > For Automaton generated by regex
	// > More logical order of printed states than
	//   what is generated by regex_to_nfa()
	struct Trans **thash = NULL;
	if (a0->regex) {
		thash = malloc(sizeof(struct Trans *) * a0->trans.hashmax);
		if (thash == NULL) {
			fprintf(stderr, "Error initializing sorted copy of Trans Block before print\n");
			exit(EXIT_FAILURE);
		}
		memcpy(thash, a0->trans.hash.elem, sizeof(struct Trans *) * a0->trans.hashmax);
		qsort(thash, a0->trans.hashmax, sizeof(struct Trans *), transcmp);
	}

	struct Trans *trans_prev = NULL;
	size_t size = 0;
	int symlen = 0;

	for (size_t i = 0; i < a0->trans.data.size; i++) {
		struct Stack *strip = Stack_get(&a0->trans.data, i);
		for (size_t j = 0; j < strip->size; j++) {
			struct Trans *trans = NULL;
			struct Trans *trans_next = NULL;

			// Get next transition			
			if (thash) {
				trans = thash[size++];
				if (size < a0->trans.size) trans_next = thash[size];
			} else {
				trans = Stack_get(strip, j);
				if (j+1 < strip->size) {
					trans_next = Stack_get(strip, j+1);
				} else if (i+1 < a0->trans.data.size) {
					struct Stack *strip_next = Stack_get(&a0->trans.data, i+1);
					trans_next = Stack_get(strip_next, 0);
				}
			}

			// Print parent state, if this is the start of new transition print sequence
			if (trans_prev) {
				if (trans->pstate != trans_prev->pstate) {
					printf("\n%s:%s", trans->pstate->name, (format) ? "\n   " : " ");
				}
			} else {
				printf("%s:%s", trans->pstate->name, (format) ? "\n   " : " ");
			}
	
			// Current and next trans share a parent state
			if (trans_next && trans_next->pstate == trans->pstate) {
	
				// Determine if next transition's destination is the same and is not an e-transition
				// > Epsilon transitions may not be an element in a symbol list
				int next_dest_same = (!trans_next->epsilon &&
						trans_next && trans_next->dstate == trans->dstate &&
						trans_next->popsym == trans->popsym &&
						trans_next->pushsym == trans->pushsym &&
						trans_next->dir == trans->dir &&
						trans_next->writesym == trans->writesym);

				// Trans symbol list (input symbols share a destination)
				// > a,b,c > q0;
				if (next_dest_same) {

					// If destination list was in progress, terminate it
					// > Symbol list has 'print priority' over dest list
					if (trans_prev && trans_prev->pstate == trans->pstate && 
							trans_prev->inputsym == trans->inputsym) {
						printf(";%s", (format) ? "\n   " : " ");
						symlen = 0;
					}

					if (trans->epsilon) {
						printf("> %s", trans->dstate->name);
						Trans_ext_print(trans);
						printf(";%s", (format) ? "\n   " : " ");
						symlen = 0;
					} else {
						// Don't allow a symbol list to get too long
						if (format && symlen >= 30) {
							printf("\n   ");
							symlen = 0;
						}
						symlen += Trans_sym_conf_print(trans->inputsym) + 1;
						putchar(',');	
					}
				
				// Trans destination list (single symbol goes to many destinations):
				// > ie. a > q0,q1(L),q2(b,R);
				} else if (trans_next->inputsym == trans->inputsym) {
					
					// Beginning of a dest list
					if (!trans_prev || trans_prev->pstate != trans->pstate ||
							trans_prev->inputsym != trans->inputsym) {	
						if (trans->epsilon) {
							printf("> %s", trans->dstate->name);
						} else {
							Trans_sym_conf_print(trans->inputsym);
							printf(" > %s", trans->dstate->name);
						}

					// Dest list in progress
					} else {
						printf(",%s", trans->dstate->name);
					}
					Trans_ext_print(trans);
					symlen = 0;
				
				// Normal (single symbol to single destination)
				} else {
					if (trans->epsilon) {
						printf("> %s", trans->dstate->name);
					} else {
						
						// Dest list end
						if (trans_prev && trans_prev->pstate == trans->pstate &&
								trans_prev->inputsym == trans->inputsym) {
							printf(",%s", trans->dstate->name);
						
						// Normal 
						} else {
							Trans_sym_conf_print(trans->inputsym);
							printf(" > %s", trans->dstate->name);
						}
					}
					Trans_ext_print(trans);
					printf(";%s", (format) ? "\n   " : " ");
					symlen = 0;
				}

			// New next parent state, end of this transition print sequence
			} else {
				
				// Properly append to active destination list
				// > Don't print symbol & prepend a comma to dest
				if (trans_prev && trans_prev->pstate == trans->pstate && 
						trans_prev->inputsym == trans->inputsym) {
					printf(",%s", trans->dstate->name);
				
				// Normal (single symbol to single destination)
				} else if (trans->epsilon) {
					printf("> %s", trans->dstate->name);
				} else {
					Trans_sym_conf_print(trans->inputsym);
					printf(" > %s", trans->dstate->name);
				}
				Trans_ext_print(trans);
				putchar(';');
				symlen = 0;
			}

			trans_prev = trans;
		}
	}

	putchar('\n');

	if (thash) free(thash);

	return;
}
