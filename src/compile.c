#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "compile.h"
#include "file.h"

#define GROWTH_FACTOR 2.0

char cell_type_str[100];

char *header_string = 
	/*"#include \"src/tape.h\"\n"*/
	"#include <math.h>\n"
	"#include <time.h>\n"
	"#include <getopt.h>\n"
	"#include <inttypes.h>\n"
	"#include <ctype.h>\n"
	"#include <stddef.h>\n"
	"#include <string.h>\n"
	"#include <stdio.h>\n"
	"#include <stdlib.h>\n\n"
	"#define GROWTH_FACTOR %lf\n\n"
	;

char *stackcode =
	"struct Stack {\n"
	"\t%s *elem;\n"
	"\tuintmax_t size;\n"
	"\tuintmax_t max;\n"
	"};\n\n"

	"struct Stack Stack_init()\n"
	"{\n"
	"\tstruct Stack stk0;\n"
	"\tstk0.size = 0;\n"
	"\tstk0.max = 100;\n"
	"\tstk0.elem = malloc(sizeof(%s) * stk0.max);\n"
	"\treturn stk0;\n"
	"}\n\n"

	"void Stack_push(struct Stack *stk0, %s elem)\n"
	"{\n"
	"\tif (stk0->size >= stk0->max) {\n"
		"\t\tstk0->max *= GROWTH_FACTOR;\n"
		"\t\t%s *newelem = realloc(stk0->elem, sizeof(%s) * stk0->max);\n"
		"\t\tif (!newelem) {\n"
			"\t\t\tfprintf(stderr, \"Error increasing stack memory.\\n\");\n"
			"\t\t\texit(EXIT_FAILURE);\n"
		"\t\t}\n"
		"\t\tstk0->elem = newelem;\n"
	"\t}\n"
	"\tstk0->elem[stk0->size++] = elem;\n"
	"}\n\n"

	"void Stack_free(struct Stack *stk0)\n"
	"{\n"
	"\tif (stk0->elem) {\n"
	"\t\tfree(stk0->elem);\n"
	"\t}\n"
	"}\n\n"
	;

char *stackcode_generic =
	"struct %s {\n"
	"\t%s *elem;\n"
	"\tuintmax_t size;\n"
	"\tuintmax_t max;\n"
	"};\n\n"

	"struct %s %s_init()\n"
	"{\n"
	"\tstruct %s stk0;\n"
	"\tstk0.size = 0;\n"
	"\tstk0.max = 100;\n"
	"\tstk0.elem = malloc(sizeof(%s) * stk0.max);\n"
	"\treturn stk0;\n"
	"}\n\n"

	"void %s_push(struct %s *stk0, %s elem)\n"
	"{\n"
	"\tif (stk0->size >= stk0->max) {\n"
		"\t\tstk0->max *= GROWTH_FACTOR;\n"
		"\t\t%s *newelem = realloc(stk0->elem, sizeof(%s) * stk0->max);\n"
		"\t\tif (!newelem) {\n"
			"\t\t\tfprintf(stderr, \"Error increasing stack memory.\\n\");\n"
			"\t\t\texit(EXIT_FAILURE);\n"
		"\t\t}\n"
		"\t\tstk0->elem = newelem;\n"
	"\t}\n"
	"\tstk0->elem[stk0->size++] = elem;\n"
	"}\n\n"

	"%s *%s_peek(struct %s *stk0)\n"
	"{\n"
	"\tif (!stk0->size) return NULL;\n\n"
	"\treturn &stk0->elem[stk0->size-1];\n"
	"}\n\n"

	"void %s_free(struct %s *stk0)\n"
	"{\n"
	"\tif (stk0->elem) {\n"
	"\t\tfree(stk0->elem);\n"
	"\t}\n"
	"}\n\n"
	;

char *tapecode =
	"struct Tape {\n"
	"\tstruct Stack tape;\n"
	"\tuintmax_t head;\n"
	"};\n\n"

	"struct Tape Tape_init()\n"
	"{\n"
	"\tstruct Tape tp0;\n"
	"\ttp0.head = 0;\n"
	"\ttp0.tape = Stack_init();\n"
	"\tfor (size_t i = 0; i < tp0.tape.max; i++) tp0.tape.elem[i] = tm_blank;\n"
	"\treturn tp0;\n"
	"}\n\n"

	"void Tape_move_left(struct Tape *tp0)\n"
	"{\n"
	"\tif (!tp0->head) {\n"
		"\t\tunsigned int oldmax = tp0->tape.max;\n"
		"\t\ttp0->tape.max *= GROWTH_FACTOR;\n"
		"\t\t%s *newelem = realloc(tp0->tape.elem, tp0->tape.max * sizeof(%s));\n"
		"\t\tif (!newelem) {\n"
			"\t\t\tfprintf(stderr, \"Error expanding tape.\\n\");\n"
			"\t\t\texit(EXIT_FAILURE);\n"
		"\t\t}\n"
		"\t\tsize_t offset = tp0->tape.max - oldmax;\n"
		"\t\tmemmove(newelem + offset, newelem, oldmax * sizeof(%s));\n"
		"\t\tfor (size_t i = 0; i < offset; i++) newelem[i] = tm_blank;\n"

		"\t\ttp0->tape.elem = newelem;\n"
		"\t\ttp0->head = offset;\n"
		"\t\ttp0->tape.size += offset;\n"
	"\t}\n"
	"\ttp0->head--;\n"
	"}\n\n"

	"void Tape_move_right(struct Tape *tp0)\n"
	"{\n"
	"\tif (tp0->head >= tp0->tape.max - 1) {\n"
		"\t\tStack_push(&tp0->tape, tm_blank);\n"
		"\t\tfor (size_t i = tp0->tape.size - 1; i < tp0->tape.max; i++) tp0->tape.elem[i] = tm_blank;\n"
	"\t}\n"
	"\ttp0->head++;\n"
	"}\n\n"


	"void Tape_free(struct Tape *tp0)\n"
	"{\n"
	"\tStack_free(&tp0->tape);\n"
	"}\n\n"
	;

char *symcmpfunc =
	"int symcmp(const void *a, const void *b)\n"
	"{\n"
	"\treturn *(%s *)a - *(%s *)b;\n"
	"}\n\n"
	;

char *isdelimfunc =
	"int isdelim(%s *delims, %s sym)\n"
	"{\n"
	"\t%s *exists = bsearch(&sym, delims, %u, sizeof(%s), symcmp);\n"
	"\treturn (exists && 1);\n"
	"}\n\n"
	;

char *tapeimportfunc_delim =
	"struct Tape Tape_import(char *input_str, int is_file)\n"
	
	"{\n"	
	"\tstruct Tape input_tape = Tape_init();\n\n"

	"\tFILE *tape_file = NULL;\n"
	"\tif (is_file) {\n"
	"\t\ttape_file = fopen(input_str, \"r\");\n"
	"\t\tif (!tape_file) {\n"
	"\t\t\tfprintf(stderr, \"Error opening input string/tape file %%s\\n\", input_str);\n"
	"\t\t\texit(EXIT_FAILURE);\n"
	"\t\t}\n"
	"\t} else if (input_str[0] == \'\\0\') {\n"
	"\t\treturn input_tape;\n"
	"\t}\n\n"

	"\tsize_t input_ctr = 0;\n"
	"\tchar c = 0;\n"
	"\t%s cell = 0;\n\n"

	"\tstruct %s digitacc = %s_init();\n"
		
	"\t%s tmpc;\n"
		"\tchar *endptr = NULL;\n"
		"\tchar *special = NULL;\n"
		"\tint delim_flag = 0;\n"
		"\tint mystate = 1;\n\n"

		"\twhile ((is_file && ((c = fgetc(tape_file)) != EOF)) || \n"
				"\t  (!is_file && (c = input_str[input_ctr++]) != \'\\0\')) {\n\n"

			"\t\tcell = c;\n\n"

			"\t\tswitch(mystate) {\n"
				"\t\t\tcase 1:\n"
					"\t\t\t\tif (isdigit(c) || c == \'-\') {\n"
						"\t\t\t\t\t%s_push(&digitacc, c);\n"
						"\t\t\t\t\tmystate = 4;\n"
					"\t\t\t\t} else if (isdelim(delims, cell)) {\n"
						"\t\t\t\t\tdelim_flag = 1;\n"
						"\t\t\t\t\tmystate = 2;\n"
					"\t\t\t\t} else {\n"
						"\t\t\t\t\tStack_push(&input_tape.tape, cell);\n"
						"\t\t\t\t\tmystate = 3;\n"
					"\t\t\t\t}\n"
					"\t\t\t\tbreak;\n"
				"\t\t\tcase 2:\n"
					"\t\t\t\tif (isdigit(c) || c == \'-\') {\n"
						"\t\t\t\t\t%s_push(&digitacc, c);\n"
						"\t\t\t\t\tmystate = 4;\n"
					"\t\t\t\t} else if (isdelim(delims, cell)) {\n"
						"\t\t\t\t\tdelim_flag = 1;\n"
						"\t\t\t\t\tmystate = 2;\n"
					"\t\t\t\t} else {\n"
						"\t\t\t\t\tStack_push(&input_tape.tape, cell);\n"
						"\t\t\t\t\tmystate = 3;\n"
					"\t\t\t\t}\n"
					"\t\t\t\tbreak;\n"
				"\t\t\tcase 3:\n"
					"\t\t\t\tif (isdelim(delims, cell)) {\n"
						"\t\t\t\t\tdelim_flag = 1;\n"
						"\t\t\t\t\tmystate = 2;\n"
					"\t\t\t\t} else {\n"
						"\t\t\t\t\tStack_push(&input_tape.tape, cell);\n"
						"\t\t\t\t\tmystate = 3;\n"
					"\t\t\t\t}\n"
					"\t\t\t\tbreak;\n"
				"\t\t\tcase 4:\n"
					"\t\t\t\tif (isdelim(delims, cell)) {\n"
						"\t\t\t\t\tdelim_flag = 1;\n"
						"\t\t\t\t\ttmpc = digitacc.elem[--digitacc.size];\n"
						"\t\t\t\t\tStack_push(&input_tape.tape, tmpc);\n"
						"\t\t\t\t\tmystate = 2;\n"
					"\t\t\t\t} else if (isdigit(c)) {\n"
						"\t\t\t\t\t%s_push(&digitacc, c);\n"
						"\t\t\t\t\tmystate = 5;\n"
					"\t\t\t\t} else {\n"
						"\t\t\t\t\tfor (unsigned int i = 0; i < digitacc.size; i++) {\n"
							"\t\t\t\t\t\ttmpc = digitacc.elem[i];\n"
							"\t\t\t\t\t\tStack_push(&input_tape.tape, tmpc);\n"
						"\t\t\t\t\t}\n"
						"\t\t\t\t\tStack_push(&input_tape.tape, cell);\n"
						"\t\t\t\t\tdigitacc.size = 0;\n"
						"\t\t\t\t\tmystate = 3;\n"
					"\t\t\t\t}\n"
					"\t\t\t\tbreak;\n"
				"\t\t\tcase 5:\n"
					"\t\t\t\tif (isdigit(c)) {\n"
						"\t\t\t\t\t%s_push(&digitacc, c);\n"
						"\t\t\t\t\tmystate = 5;\n"
					"\t\t\t\t} else if (isdelim(delims, cell)) {\n"
						"\t\t\t\t\tdelim_flag = 1;\n"
						"\t\t\t\t\t%s_push(&digitacc, \'\\0\');\n"
						"\t\t\t\t\tspecial = digitacc.elem;\n"
						"\t\t\t\t\ttmpc = %s;\n"
						"\t\t\t\t\tStack_push(&input_tape.tape, tmpc);\n"
						"\t\t\t\t\tdigitacc.size = 0;\n"
						"\t\t\t\t\tmystate = 2;\n"
					"\t\t\t\t} else {\n"
						"\t\t\t\t\tfor (unsigned int i = 0; i < digitacc.size; i++) {\n"
							"\t\t\t\t\t\ttmpc = digitacc.elem[i];\n"
							"\t\t\t\t\t\tStack_push(&input_tape.tape, tmpc);\n"
						"\t\t\t\t\t}\n"
						"\t\t\t\t\tStack_push(&input_tape.tape, cell);\n"
						"\t\t\t\t\tdigitacc.size = 0;\n"
						"\t\t\t\t\tmystate = 3;\n"
					"\t\t\t\t}\n"
					"\t\t\t\tbreak;\n"
			"\t\t}\n"
		"\t}\n\n"

		"\tif (digitacc.size == 1) {\n"
			"\t\ttmpc = digitacc.elem[--digitacc.size];\n"
			"\t\tStack_push(&input_tape.tape, tmpc);\n"
		"\t} else if (digitacc.size) {\n"
			"\t\tif (delim_flag) {\n"
			"\t\t\t%s_push(&digitacc, \'\\0\');\n"
			"\t\t\tspecial = digitacc.elem;\n"
			"\t\t\ttmpc = %s;\n"
			"\t\t\tStack_push(&input_tape.tape, tmpc);\n"
			"\t\t} else {\n"
			"\t\t\tfor (unsigned int i = 0; i < digitacc.size; i++) {\n"
			"\t\t\t\ttmpc = digitacc.elem[i];\n"
			"\t\t\t\tStack_push(&input_tape.tape, tmpc);\n"
			"\t\t\t}\n"
			"\t\t}\n"
		"\t}\n\n"

		"\t%s_free(&digitacc);\n"

		"\tfor (size_t i = input_tape.tape.size; i < input_tape.tape.max; i++)\n"
		"\t\tinput_tape.tape.elem[i] = tm_blank;\n\n"
	
		"\tif (is_file) fclose(tape_file);\n\n"

		"\treturn input_tape;\n"
	"}\n\n"
	;

char *tapeimportfunc =
	"struct Tape Tape_import(char *input_str, int is_file)\n"

	"{\n"	
	"\tstruct Tape input_tape = Tape_init();\n\n"
	
	"\tFILE *tape_file = NULL;\n"
	"\tif (is_file) {\n"
	"\t\ttape_file = fopen(input_str, \"r\");\n"
	"\t\tif (tape_file == NULL) {\n"
	"\t\t\tfprintf(stderr, \"Error opening input string/tape file %%s\\n\", input_str);\n"
	"\t\t\texit(EXIT_FAILURE);\n"
	"\t\t}\n"
	"\t} else if (input_str[0] == \'\\0\') {\n"
	"\t\treturn input_tape;\n"
	"\t}\n\n"

	"\tsize_t input_ctr = 0;\n"
	"\tchar c = 0;\n"
	"\t%s cell = 0;\n\n"
	
	"\twhile ((is_file && (c = fgetc(tape_file)) != EOF) || \n"
	"\t  (!is_file && (c = input_str[input_ctr++]) != \'\\0\')) {\n"
	
			"\t\tcell = c;\n"
			"\t\tStack_push(&input_tape.tape, cell);\n"
		"\t}\n\n"

	"\tfor (size_t i = input_tape.tape.size; i < input_tape.tape.max; i++)\n"
	"\t\tinput_tape.tape.elem[i] = tm_blank;\n\n"

	"\tif (is_file) fclose(tape_file);\n\n"

	"\treturn input_tape;\n"
	"}\n\n"
	;

char *main_string =
	"int main(int argc, char **argv)\n"
	"{\n"

	"\tint opt;\n"
	"\tchar *input_string = NULL;\n"
	"\tchar *input_string_file = NULL;\n"
	"\twhile ((opt = getopt (argc, argv, \"-:i:\")) != -1) {\n"
		"\t\tswitch (opt) {\n"
			"\t\t\tcase \'i\':\n"
				"\t\t\t\tinput_string_file = optarg;\n"
				"\t\t\t\tbreak;\n"
			"\t\t\tcase \'?\':\n"
				"\t\t\t\tfprintf(stderr, \"Unknown option \'-%c\'\\n\", optopt);\n"
				"\t\t\t\texit(EXIT_FAILURE);\n"
			"\t\t\tcase \':\':\n"
				"\t\t\t\tfprintf(stderr, \"Option -%c requires an argument\\n\", optopt);\n"
				"\t\t\t\texit(EXIT_FAILURE);\n"
			"\t\t\tcase 1:\n"
					"\t\t\t\tinput_string = optarg;\n"
					"\t\t\t\tbreak;\n"
		"\t\t}\n"
	"\t}\n\n"

	"\tstruct Tape tape;\n"
	"\tif (input_string_file) {\n"
	"\t\ttape = Tape_import(input_string_file, 1);\n"
	"\t} else if (!input_string) {\n"
	"\t\tfprintf(stderr, \"%s <tape/string>\\n%s -i <tape/string file>\\n\", argv[0], argv[0]);\n"
	"\t\texit(EXIT_FAILURE);\n"
	"\t} else {\n"
	"\t\ttape = Tape_import(input_string, 0);\n"
	"\t}\n\n"
	;

void machine_to_c(struct Automaton *a0, char *output_filename)
{
	FILE *output_file = fopen(output_filename, "w+");
	CELL_TO_STRING(cell_type_str, CELL_TYPE);

	char print_type[100];
	if (sign) {
		CELL_TO_STRING(print_type, SIGNED_PRINT_TYPE);
	} else {
		CELL_TO_STRING(print_type, UNSIGNED_PRINT_TYPE);	
	}

	// Print includes
	fprintf(output_file, header_string, STACK_MULTIPLIER);

	// Print blank symbol
	if (sign) {
		fprintf(output_file, "%s tm_blank = %" SIGNED_PRINT_FMT ";\n\n",
				cell_type_str, (SIGNED_PRINT_TYPE)tm_blank);
	} else {
		fprintf(output_file, "%s tm_blank = %" UNSIGNED_PRINT_FMT ";\n\n",
				cell_type_str, (UNSIGNED_PRINT_TYPE)tm_blank);
	}

	// Print Stack functions (for machine stack, tape)
	char *stack_type = "Stack";
	char *cell_type = cell_type_str;
	fprintf(output_file, stackcode_generic,
			stack_type, cell_type, stack_type, stack_type, stack_type, cell_type,
			stack_type, stack_type, cell_type, /*GROWTH_FACTOR,*/ cell_type, cell_type,
			cell_type, stack_type, stack_type,
			stack_type, stack_type);

	// Print Tape functions (import function handled afterwards)
	fprintf(output_file, tapecode, 
			/*GROWTH_FACTOR,*/ cell_type_str, cell_type_str, cell_type_str);

	if (a0->delims.size) {
		// Support functions for Tape_import w/ delimiter
		fprintf(output_file, symcmpfunc, cell_type_str, cell_type_str);
		fprintf(output_file, isdelimfunc, cell_type_str, cell_type_str, cell_type_str, a0->delims.size, cell_type_str);
		
		// Delimiters
		fprintf(output_file, "%s delims[%u] = {", cell_type_str, a0->delims.size);
		for (unsigned int i = 0; i < a0->delims.size; i++) {
			if (sign) {
				fprintf(output_file, "%" SIGNED_PRINT_FMT ",", 
						(SIGNED_PRINT_TYPE) *(CELL_TYPE *) Stack_get(&a0->delims, i));
			} else {
				fprintf(output_file, "%" UNSIGNED_PRINT_FMT ",", 
						(UNSIGNED_PRINT_TYPE) *(CELL_TYPE *) Stack_get(&a0->delims, i));
			}
		}
		fprintf(output_file, "};\n\n");

		// Character buffer for integer processing
		// > If CELL_TYPE already char, do not generate CharBuff
		if (strcmp(cell_type_str, "char")) {
			stack_type = "CharBuff";
			cell_type = "char";
			fprintf(output_file, stackcode_generic,
					stack_type, cell_type, stack_type, stack_type, stack_type, cell_type,
					stack_type, stack_type, cell_type, GROWTH_FACTOR, cell_type, cell_type,
					cell_type, stack_type, stack_type,
					stack_type, stack_type);
		} else {
			stack_type = "Stack";
		}

		cell_type = cell_type_str;
		fprintf(output_file, tapeimportfunc_delim, 
				cell_type,
				stack_type,
				stack_type,
				cell_type,
				stack_type,
				stack_type,
				stack_type,
				stack_type,
				stack_type,
				(sign) ? "strtoimax(special, &endptr, 10)" : "strtoumax(special, &endptr, 10)",
				stack_type,
				(sign) ? "strtoimax(special, &endptr, 10)" : "strtoumax(special, &endptr, 10)",
				stack_type);

	} else {

		// Tape_import, no delimiter
		fprintf(output_file, tapeimportfunc, cell_type_str, cell_type_str);
	}

	// Symbol print function
	fprintf(output_file,
		"int Trans_sym_print(%s sym)\n"
		"{\n"
		"\tif (sym > 31 && sym < 127) {\n"
		"\t\treturn printf(\"%%c\", (char)sym);\n"
		"\t} else {\n",
		cell_type_str);

	if (sign) {
		fprintf(output_file,
			"\t\treturn printf(\"%%02" SIGNED_PRINT_FMT "\", (%s)sym);\n"
			"\t}\n"
			"}\n\n",
			print_type);
	} else {
		fprintf(output_file,
			"\t\treturn printf(\"%%02" UNSIGNED_PRINT_FMT "\", (%s)sym);\n"
			"\t}\n"
			"}\n\n",
			print_type);
	}
			
	// main() begin
	fprintf(output_file, "%s", main_string);

	if (a0->pda) {
		fprintf(output_file,
				"\tstruct Stack stack = Stack_init();\n\n");
	}

	fprintf(output_file, 
			"\tint halt_status = 0;\n"
			"\tsize_t steps = 0;\n"
			"%s" // origsize var
			"\tptrdiff_t state = %td;\n\n"

			"\tclock_t begin = clock();\n\n"

			"\twhile(%s) {\n\n"
			"\t\t%s sym = tape.tape.elem[tape.head%s];\n",
			
			(a0->tm) ? "" : "\tsize_t origsize = tape.tape.size;\n",
			(ptrdiff_t)a0->start,
			(a0->tm) ? "1" : "origsize--",
			cell_type_str,
			(a0->tm) ? "" : "++");

	if (a0->pda) {
		fprintf(output_file,
				"\t\t%s *top = Stack_peek(&stack);\n\n"
				
				"\t\tswitch(state) {\n\n",
				cell_type_str);
	} else {
		fprintf(output_file,
				"\n"
				"\t\tswitch(state) {\n\n");
	}

	// Generate transitions in switch(case) statement
	
	struct Stack states = Stack_init(STATEPTR);
	Stack_push(&states, &a0->start);

	struct State *state = NULL;
	while((state = Stack_pop(&states))) {
		if (state->output_mark) continue;
		state->output_mark = 1;

		char *flags = (state->start) ? (state->final) ? " (start,final)" : 
				(state->reject) ? " (start,reject)" : " (start)" : 
				(state->final) ? " (final)" : (state->reject) ? " (reject)" : "";

		if (state->syms || state->epsilon) {
			
			fprintf(output_file,
					"\t\t\t// state %s%s\n"
					"\t\t\tcase %td:\n"
					"\t\t\t\tswitch(sym) {\n",
					state->name,
					flags,
					(ptrdiff_t)state);

			for (unsigned int i = 0; i < a0->alphabet.size; i++) {
				CELL_TYPE *symbol = Stack_get(&a0->alphabet, i);
				XXH64_hash_t offset = a0->trans.hashmax;
				struct Trans *trans = Trans_get(a0, state, *symbol, &offset, 0);

				if (trans) {

					if (sign) {
						fprintf(output_file, "\t\t\t\t\tcase %" SIGNED_PRINT_FMT ":\n", 
								(SIGNED_PRINT_TYPE)*symbol);
					} else {
						fprintf(output_file, "\t\t\t\t\tcase %" UNSIGNED_PRINT_FMT ":\n", 
								(UNSIGNED_PRINT_TYPE)*symbol);
					}

					while (trans) {

						if (trans->pop) {
							if (sign) {
								fprintf(output_file,
										"\t\t\t\t\tif (top && *top == %" SIGNED_PRINT_FMT ") {\n"
										"\t\t\t\t\t\tstack.size--;\n",
										(SIGNED_PRINT_TYPE)trans->popsym);
							} else {
								fprintf(output_file,
										"\t\t\t\t\tif (top && *top == %" UNSIGNED_PRINT_FMT ") {\n"
										"\t\t\t\t\t\tstack.size--;\n",
										(UNSIGNED_PRINT_TYPE)trans->popsym);
							}
						}

						char *dir = NULL;
						if (trans->dir == 1) dir = "Tape_move_left(&tape)";
						else if (trans->dir == 2) dir = "Tape_move_right(&tape)";

						if (trans->write) { 
							if (sign) {
								fprintf(output_file, 
										"\t\t\t\t\t\ttape.tape.elem[tape.head] = %" SIGNED_PRINT_FMT ";\n", 
										(SIGNED_PRINT_TYPE)trans->writesym);
							} else {
								fprintf(output_file, 
										"\t\t\t\t\t\ttape.tape.elem[tape.head] = %" UNSIGNED_PRINT_FMT ";\n", 
										(UNSIGNED_PRINT_TYPE)trans->writesym);
							}
						}
						if (trans->dir) fprintf(output_file, 
								"\t\t\t\t\t\t%s;\n", 
								dir);
						if (trans->dstate) fprintf(output_file, 
								"\t\t\t\t\t\tstate = %td;\n", 
								(ptrdiff_t)trans->dstate);

						if (trans->push) {
							if (sign) {
								fprintf(output_file,
										"\t\t\t\t\t\tStack_push(&stack, %" SIGNED_PRINT_FMT ");\n",
										(SIGNED_PRINT_TYPE)trans->pushsym);
							} else {
								fprintf(output_file,
										"\t\t\t\t\t\tStack_push(&stack, %" UNSIGNED_PRINT_FMT ");\n",
										(UNSIGNED_PRINT_TYPE)trans->pushsym);
							}
						}

						if (trans->pop) {
							fprintf(output_file,
									"\t\t\t\t\t} else {\n"
									"\t\t\t\t\t\tgoto %s;\n"
									"\t\t\t\t\t}\n",
									(a0->tm) ? "halt" : "reject");
						}

						fprintf(output_file, 
								"\t\t\t\t\t\tbreak;\n");

						Stack_push(&states, &trans->dstate);

						if (trans->strans) {
							trans = Trans_get(a0, state, *symbol, &offset, 0);
						} else {
							break;
						}
					}
				}
			}

			char *halt_status = (a0->tm) ?
				(state->final) ? "\t\t\t\thalt_status = 1;\n\t\t\t\tgoto halt;\n" :
				(state->reject) ? "\t\t\t\tgoto halt;\n" :
				"\t\t\t\tbreak;\n" :
				(state->reject) ? "\t\t\t\tgoto halt;\n" : "\t\t\t\tbreak;\n";

			fprintf(output_file, 
					"\t\t\t\t\tdefault:\n"
					"\t\t\t\t\t\tgoto %s;\n"
					"\t\t\t\t}\n"
					"%s",
					(a0->tm) ? "halt" : "reject",
					halt_status);

		} else {
			
			char *halt_status = (a0->tm) ?
				(state->final) ? "\t\t\t\thalt_status = 1;\n" : "" : "";

			fprintf(output_file,
					"\t\t\t// state %s%s\n"
					"\t\t\tcase %td:\n"
					"%s"
					"%s"
					"\t\t\t\tgoto halt;\n",
					
					state->name,
					flags,
					(ptrdiff_t)state,
					(a0->tm) ? "" : 
					"\t\t\t\tswitch(sym) {\n"
					"\t\t\t\t\tdefault:\n"
					"\t\t\t\t\t\tgoto reject;\n"
					"\t\t\t\t}\n",
					halt_status);
		}
	}

	// main() end
	fprintf(output_file,
			"\t\t\tdefault:\n"
			"\t\t\t\tgoto halt;\n"	
			"\t\t}\n"
			"\t\tsteps++;\n"
			"\t}\n\n"
			"halt:;\n\n");
			
	if (!a0->tm) {
		fprintf(output_file,
				"\tswitch(state) {\n");

		for (unsigned int i = 0; i < a0->finals.size; i++) {
			struct State *final = Stack_get(&a0->finals, i);
			fprintf(output_file,
					"\t\tcase %td:\n",
					(ptrdiff_t)final);
		}

		fprintf(output_file,
				"\t\t\thalt_status = 1;\n"
				"\t\t\tbreak;\n"
				"\t\tdefault:\n"
				"reject:\n"
				"\t\t\thalt_status = 0;\n"
				"\t}\n\n");
	}
			
	fprintf(output_file,
			"\tclock_t end = clock();\n"
			"\tdouble run_time = (double)(end - begin) / CLOCKS_PER_SEC;\n"
			"\tdouble sec;\n"
			"\tdouble msec = modf(run_time, &sec);\n\n");

	CELL_TYPE delim = 0;
	if (a0->delims.size) {
		int has_newline = 0;
		int has_space = 0;
		int has_tab = 0;
		for (unsigned int i = 0; i < a0->delims.size; i++) {
			CELL_TYPE *tmpc = Stack_get(&a0->delims, i);
			if (!isspace(*tmpc)) {
				if (sign) {
					delim = *tmpc;
				} else {
					delim = *tmpc;
				}
				goto end_delim;
			}
			has_newline = (*tmpc == 10);
			has_space = (*tmpc == 32);
			has_tab = (*tmpc == 9);
		}

		// If no non-whitespace delims, choose a whitespace
		// delim based on this priority
		if (a0->delims.size == 6) {
				delim = 0;
			if (has_space) {
				delim = 32;
			} else if (has_tab) {
				delim = 9;
			} else if (has_newline) {
				delim = 10;
			} else {
				delim = 32; // if other whitespace, just set delim to normal space
			}
		}

		// If custom whitespace delim specified, use the first one
		delim = *(CELL_TYPE *) Stack_get(&a0->delims, 0);

	} else {
		delim = '|'; // use vertical bar if no delims present
	}

end_delim:

	if (sign) {
		fprintf(output_file,
				"\t%s delim = %" SIGNED_PRINT_FMT ";\n",
				cell_type_str,
				(SIGNED_PRINT_TYPE)delim);
	} else {
		fprintf(output_file,
				"\t%s delim = %" UNSIGNED_PRINT_FMT ";\n",
				cell_type_str,
				(UNSIGNED_PRINT_TYPE)delim);
	}

	fprintf(output_file,
			"\tsize_t i,j;\n"
			"\tfor (i = 0; tape.tape.elem[i] == tm_blank && i < tape.tape.max; i++);\n"
			"\tfor (j = tape.tape.max - 1; tape.tape.elem[j] == tm_blank && j > 0; j--);\n"
			"\tfor (size_t k = i; k < j; k++) {\n"
			"\t\tif (Trans_sym_print(tape.tape.elem[k]) > 1) {\n"
			"\t\t\tputchar(delim);\n"
			"\t\t}\n"
			"\t}\n"
			"\tTrans_sym_print(tape.tape.elem[j]);\n"
			"\tputchar(\'\\n\');\n\n"
			
			"\tfprintf(stderr, \"%%s\\n\", (halt_status) ? \"ACCEPTED\" : \"REJECTED\");\n"
			"\tfprintf(stderr, \"STEPS:   %%zu\\n\", steps);\n\n"
			"\tfprintf(stderr, \"Runtime: %%ldm%%ld.%%06lds\\n\",\n"
			"\t\t(long int)sec / 60, (long int)sec %% 60, (long int)(msec * 1000000));\n\n"

			"\tTape_free(&tape);\n"
			"%s"
			"\treturn halt_status;\n\n}",
			(a0->pda) ? "\tStack_free(&stack);\n\n" : "\n");
		
	
	fclose(output_file);
}
