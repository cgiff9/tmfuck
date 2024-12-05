#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "regex.h"
#include "auto.h"
#include "stack.h"

#define REGSYMS_LEN 6
#define EMPTY PTRDIFF_MAX

char regsyms[REGSYMS_LEN] = { '(', ')', '*', '+', '_', '|' };

void remove_spaces(char* s) {
	char* d = s;
	do {
		while (*d == ' ') {
			++d;
		}
	} while ((*s++ = *d++));
}

// Determines if character in a regex string
// is an operator or operand
// > operators stored in regsyms[] above
//
// Returns 1 on yes, 0 on no
int isregexsym(char c)
{
	if (isalnum(c)) return 1;
	//unsigned int *exists = bsearch(&c, syms, SYMS_LEN, sizeof(unsigned int), symcmp);
	//if (exists) return 1;
	
	//for (unsigned int i = 0; syms[i] != '\0'; i++)
	for (unsigned int i = 0; i < REGSYMS_LEN; i++)
		if (c == regsyms[i])
			return 0;

	return 1;
}

// Ensures the regex is not 'incorrect'
// > balanced parentheses
// > operators/operands properly place
//
// Returns 1 if valid, 0 if invalid
int is_valid_regex(char *regex)
{
	int state = 1;
	int num_parens = 0;
	for (int i=0; regex[i] != '\0'; i++) {
		char c = regex[i];
		switch (state) {
			case 1:
				if (isregexsym(c)) state = 2;
				//else if (c == '*' || c == '|') state = 0;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else if (c == ')') state = 0;
				else state = 0;
				break;
			case 2:
				if (isregexsym(c)) state = 2;
				else if (c == '*' || c == '+') state = 3;
				else if (c == '|') state = 4;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 3:
				if (isregexsym(c)) state = 2;
				//else if (c == '*') state = 0;
				else if (c == '|') state = 4;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 4:
				if (isregexsym(c)) state = 2;
				//else if (c == '*' || c == '|') state = 0;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 5:
				if (isregexsym(c)) state = 6;
				//else if (c == '*' || c == '|') state = 0;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 6:
				if (isregexsym(c)) state = 6;
				else if (c  == '*' || c == '+') state = 7;
				else if (c == '|') state = 8;
				else if (c == ')') {
					state = 9;
					num_parens--;
				}
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 7:
				if (isregexsym(c)) state = 6;
				//else if (c == '*') state = 0;
				else if (c == '|') state = 8;
				else if (c == ')') {
					state = 9;
					num_parens--;
				}
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 8:
				if (isregexsym(c)) state = 6;
				//else if (c == '|' || c == '*' || c == ')') state = 0;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
			case 9:
				if (isregexsym(c)) state = 6;
				else if (c == '*' || c == '+') state = 7;
				else if (c == ')') {
					state = 9;
					num_parens--;
				} else if (c == '|') state = 8;
				else if (c == '(') {
					state = 5;
					num_parens++;
				} else state = 0;
				break;
		}
		if (state == 0) {
			return 0;
		}
	}
	if ((state == 1 || state == 2 || state == 3 || 
				state == 6 || state == 7 || state == 9) && num_parens == 0) return 1;
	else if (state != 0) {
		return 0;
	}
	return 1;
}

// Validates regex, then converts to infix or
// Reverse Polish Notation (RPN)
// > ie. (ab|cd|ef)* --> ab_|cd_|ef_*
//
// Implicit concatenations are given an explict
// symbol for concatenation, arbitrarily chosen
// here to be '_'
// > ie. abc --> ab_c_
// > ie. (abc)*def --> ab_c_*d_e_f_
char *infix(char *regex)
{
	remove_spaces(regex);
	if (!is_valid_regex(regex)) {
		fprintf(stderr, "Invalid regex\n");
		exit(EXIT_FAILURE);
	}

	struct Stack stack = Stack_init(CHAR);
	char *tmp = malloc(sizeof(char) * strlen(regex)*2);
	if (!tmp) {
		fprintf(stderr, "Error allocating memory for infix string during regex processing.\n");
		exit(EXIT_FAILURE);
	}
	
	//*tmp = '\0';
	tmp[0] = '\0';
	char pushchar = 0;
	int concat_detect = 0;

	char *peek = NULL;
	char *c = NULL;
	for (int i = 0; regex[i] != '\0'; i++) {
		
		// If an implicit concatenation was detected
		// on the previous iteration, add the explicit
		// concat symbol '_' to the infix string
		if (concat_detect) {
			peek = Stack_peek(&stack);
			while (peek && *peek != '\0' &&
					((*peek == '*') || 
					 (*peek == '_') || 
					 //(*peek == '+' && *peek != '\0')
					 (*peek == '+'))) {
			
				c = Stack_pop(&stack);
				strncat(tmp, c, 1);
				peek = Stack_peek(&stack);
			}
			
			pushchar = '_';
			Stack_push(&stack, &pushchar);
			concat_detect = 0;
		}

		// Determines whether arrangement of current symbol and
		// next symbol constitutes an implicit concatenation
		if ( 
				(isregexsym(regex[i]) && isregexsym(regex[i+1])) || 
				(regex[i] == '*' && isregexsym(regex[i+1]))      ||
				(regex[i] == '+' && isregexsym(regex[i+1]))      ||
				(regex[i] == '*' && regex[i+1] == '(')           ||
				(regex[i] == '+' && regex[i+1] == '(')           ||
				(regex[i] == ')' && regex[i+1] == '(')           ||
				(isregexsym(regex[i]) && regex[i+1] == '(')      ||
				(regex[i] == ')' && isregexsym(regex[i+1]))
				) {
			concat_detect = 1;
		}

		if (isregexsym(regex[i])) {
			strncat(tmp, &regex[i], 1);
		
		} else if (regex[i] == '*' || regex[i] == '+') {
			peek = Stack_peek(&stack);
			while (peek && *peek != '\0' && 
					((*peek == '*') || 
					//(peek == '+' && peek != '\0')
					(*peek == '+'))) {
			
				c = Stack_pop(&stack);
				strncat(tmp, c, 1);
				peek = Stack_peek(&stack);
			}
	
			if (regex[i] == '*') { 
				pushchar = '*';
				Stack_push(&stack, &pushchar);
			} else { 
				pushchar = '+';
				Stack_push(&stack, &pushchar);
			}

		} else if (regex[i] == '|') {
			peek = Stack_peek(&stack);
			while (peek && *peek != '\0' &&
					((*peek == '*') || 
					 (*peek == '+') || 
					 (*peek == '_') || 
					 //(peek == '|' && peek != '\0')
					 (*peek == '|' ))) {
				
				c = Stack_pop(&stack);
				strncat(tmp, c, 1);
				peek = Stack_peek(&stack);
			}

			pushchar = '|';
			Stack_push(&stack, &pushchar);
		
		} else if (regex[i] == '(') { 
			pushchar = '(';
			Stack_push(&stack, &pushchar);
		
		} else if (regex[i] == ')') {
			peek = Stack_peek(&stack);
			while (*peek != '(' && *peek != '\0') {
				c = Stack_pop(&stack);
				strncat(tmp, c, 1);
				peek = Stack_peek(&stack);
			}
			
			if (*peek == '(') Stack_pop(&stack);
			else if (*peek == '\0') {
				fprintf(stderr, "Parentheses mismatch\n");
				Stack_free(&stack);
				free(tmp);
				exit(EXIT_FAILURE);
			}
		}
	}

	peek = Stack_peek(&stack);
	//while (peek != '\0') {
	while (stack.size > 0) {
		
		// If open paren exists on stack, then parens
		// are not balanced... though is_valid_regex()
		// probably already verified this
		if (*peek == '(') {
			fprintf(stderr, "Parentheses mismatch\n");
			exit(EXIT_FAILURE);
		}

		c = Stack_pop(&stack);
		strncat(tmp, c, 1);
		peek = Stack_peek(&stack);
	}

	Stack_free(&stack);
	return tmp;
}

// Takes a regex string in infix (RPN) format
// and constructs an NFA that accepts the language
// of that regex
// > Uses Thompson NFA construction
// > Start/Final state pointers are always
//   pushed/popped in pairs, as those are the
//   only relevant states to the construction
//   algorithm.
//
// Returns an automaton struct
struct Automaton regex_to_nfa(char *regex)
{
	char *regex_infix = infix(regex);

	// DEBUG:
	//printf("infix: %s\n", regex_infix);
	
	struct Automaton a0 = Automaton_init();
	
	// Flag the automaton as generated via regex
	// > informs Automaton_print() to sort transitions
	a0.regex = 1;
	a0.epsilon = 1;

	struct State *newstart, *newfinal,
					 *start_a, *final_a, 
					 *start_b, *final_b;

	// Stack of 'automatons', which are really
	// just start/final pairs of state pointers
	struct Stack astack = Stack_init(STATEPTR);

	const char *name_prefix = "q";
	char sname[30+strlen(name_prefix)+1];

	unsigned int name_ctr = 0;
	//ptrdiff_t infix_top;
	CELL_TYPE infix_top;
	for (size_t i = 0; regex_infix[i] != '\0'; i++) {
		infix_top = regex_infix[i];

		switch(infix_top) {
			// Concatenate top two automatons
			// > Adds e-transition from final_a to start_b
			//   rather than merging the two states
			case '_':
				final_b = Stack_pop(&astack);
				start_b = Stack_pop(&astack);
				final_a = Stack_pop(&astack);
				start_a = Stack_pop(&astack);

				Trans_add(&a0, final_a, start_b, NULL, NULL, NULL, NULL, NULL);
				
				Stack_clear(&a0.finals);
				final_a->final = 0;
				set_start(&a0, start_a);
				set_final(&a0, final_b);

				Stack_push(&astack, &start_a);
				Stack_push(&astack, &final_b);

				break;

			// Apply + operation to top automaton
			// > 'one or more'
			case '+':
				final_a = Stack_pop(&astack);
				start_a = Stack_pop(&astack);

				Trans_add(&a0, final_a, start_a, NULL, NULL, NULL, NULL, NULL);

				Stack_push(&astack, &start_a);
				Stack_push(&astack, &final_a);

				break;

			// Apply Kleene star * to top automaton
			// > 'zero or more'
			case '*':
				final_a = Stack_pop(&astack);
				start_a = Stack_pop(&astack);

				snprintf(sname, 30+2, "%s%u", name_prefix, name_ctr++);
				newstart = State_add(&a0, sname);
				snprintf(sname, 30+2, "%s%u", name_prefix, name_ctr++);
				newfinal = State_add(&a0, sname);

				Trans_add(&a0, final_a, start_a, NULL, NULL, NULL, NULL, NULL);
				Trans_add(&a0, final_a, newfinal, NULL, NULL, NULL, NULL, NULL);
				Trans_add(&a0, newstart, start_a, NULL, NULL, NULL, NULL, NULL);
				Trans_add(&a0, newstart, newfinal, NULL, NULL, NULL, NULL, NULL);

				Stack_clear(&a0.finals);
				final_a->final = 0;
				set_start(&a0, newstart);
				set_final(&a0, newfinal);

				Stack_push(&astack, &newstart);
				Stack_push(&astack, &newfinal);

				break;

			// Apply union | operation to top two automatons
			case '|':
				final_b = Stack_pop(&astack);
				start_b = Stack_pop(&astack);
				final_a = Stack_pop(&astack);
				start_a = Stack_pop(&astack);

				snprintf(sname, 30+2, "%s%u", name_prefix, name_ctr++);
				newstart = State_add(&a0, sname);
				snprintf(sname, 30+2, "%s%u", name_prefix, name_ctr++);
				newfinal = State_add(&a0, sname);

				Trans_add(&a0, newstart, start_a, NULL, NULL, NULL, NULL, NULL);
				Trans_add(&a0, newstart, start_b, NULL, NULL, NULL, NULL, NULL);
				Trans_add(&a0, final_a, newfinal, NULL, NULL, NULL, NULL, NULL);
				Trans_add(&a0, final_b, newfinal, NULL, NULL, NULL, NULL, NULL);

				Stack_clear(&a0.finals);
				final_b->final = 0;
				final_a->final = 0;
				set_start(&a0, newstart);
				set_final(&a0, newfinal);

				Stack_push(&astack, &newstart);
				Stack_push(&astack, &newfinal);

				break;

			// Character is a non-regex-control symbol
			// Construct an automaton from the symbol and push it
			default:
				snprintf(sname, 30+2, "%s%u", name_prefix, name_ctr++);
				newstart = State_add(&a0, sname);
				snprintf(sname, 30+2, "%s%u", name_prefix, name_ctr++);
				newfinal = State_add(&a0, sname);

				Trans_add(&a0, newstart, newfinal, &infix_top, NULL, NULL, NULL, NULL);
		
				Stack_clear(&a0.finals);
				set_start(&a0, newstart);
				set_final(&a0, newfinal);

				Stack_push(&astack, &newstart);
				Stack_push(&astack, &newfinal);
				break;
		}
	}

	Stack_free(&astack);
	free(regex_infix);

	return a0;
}
