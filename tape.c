#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "tape.h"
#include "auto.h"
#include "file.h"

//CELL_TYPE blank = '_';


struct Tape Tape_init()
{
	return Tape_init_max(0);
}

struct Tape Tape_init_max(size_t max)
{
	struct Tape tp0;
	tp0.tape = Stack_init_max(CELL, max);
	tp0.head = 0;
	tp0.ghost = 0;

	return tp0;
}

void Tape_clear(struct Tape *tp0)
{
	if (tp0->ghost) tp0->tape.elem = NULL;
	tp0->ghost = 0;
	Stack_clear(&tp0->tape);
	tp0->head = 0;
}

size_t Tape_free(struct Tape *tp0)
{
	if (tp0->ghost) return 0;
	return Stack_free(&tp0->tape);
}

// Moves tape head left or right
// 0: stay
// 1: left
// 2: right
//
// Returns updated head position
HEAD_TYPE Tape_pos(struct Tape *tp0, int dir)
{
	if (!dir) return tp0->head;

	if (dir == 1)
		return --tp0->head;
	
	return ++tp0->head;
}

// Sets element at current 'tape' head position
// to symbol
// > takes pointer to symbol
// > allocates more tape memory if the head is
//   out of bounds of the current allocation
//   --> memory only allocated during tape write,
//       not during tape position changes
//
//
int Tape_write(struct Tape *tp0, CELL_TYPE *sym)
{

	if (!tp0->tape.elem) { 
		Stack_push(&tp0->tape, sym);
		return 0;
	}

	CELL_TYPE tmpsym = *(CELL_TYPE *)sym;
	CELL_TYPE *tmpelem = tp0->tape.elem;
	
	if (tp0->head < 0) {
		if (tmpsym == tm_blank) return 0;

		int resize = 0;
		while(tp0->tape.size - tp0->head >= tp0->tape.max) {
			// See stack.h for more details
			tp0->tape.max STACK_GROWTH_FUNC;
			resize = 1;
		}

		if (resize) {
			CELL_TYPE *newelem = realloc(tp0->tape.elem, tp0->tape.max * sizeof(CELL_TYPE));
			if (!newelem) {
				fprintf(stderr, "Error rellocating tape elements\n");
				exit(EXIT_FAILURE);
			}
			tp0->tape.elem = newelem;
			tmpelem = tp0->tape.elem;
		}

		int offset = tp0->head * -1;
		memmove(tp0->tape.elem + offset * sizeof(CELL_TYPE), 
				tp0->tape.elem, 
				(tp0->tape.size) * sizeof(CELL_TYPE));
		memset(tp0->tape.elem, tm_blank, offset * sizeof(CELL_TYPE));
		
		tp0->tape.size += offset;
		tp0->head = 0;
			
	} else if (tp0->head >= (long int)tp0->tape.size) {
		if (tmpsym == tm_blank) return 0;

		int resize = 0;
		while(tp0->tape.size + tp0->head - tp0->tape.size >= tp0->tape.max) {
			//tp0->tape.max *= STACK_MULTIPLIER;
			tp0->tape.max STACK_GROWTH_FUNC;
			resize = 1;
		}

		if (resize) {
			CELL_TYPE *newelem = realloc(tp0->tape.elem, tp0->tape.max * sizeof(CELL_TYPE));
			if (!newelem) {
				fprintf(stderr, "Error rellocating tape elements\n");
				exit(EXIT_FAILURE);
			}
			tmpelem = tp0->tape.elem = newelem;
		}

		memset(tp0->tape.elem + tp0->tape.size, 
				tm_blank, 
				(tp0->head - tp0->tape.size) * sizeof(CELL_TYPE));	
		tp0->tape.size = tp0->head + 1;
	} 

	tmpelem[tp0->head] = tmpsym;

	return 0;
}	

// Returns the value in the cell at the
// current tape head position
//
// Returns CELL_TYPE value
CELL_TYPE Tape_head(struct Tape *tp0)
{
	if (tp0->head < 0 || tp0->head >= (long int) tp0->tape.size) return tm_blank;

	CELL_TYPE *tmpelem = tp0->tape.elem;
	return tmpelem[tp0->head];
}

// Reserves space in automaton's Tape block
// for a new tape
//
// Returns pointer to new Tape
struct Tape *Tape_add(struct Automaton *a0)
{
	struct Block *block = &a0->tapes;

	if (block->size >= block->max) {
		Block_grow(block);
	}

	/*struct Stack *datastacks = block->data.elem;
	struct Stack *strip = &datastacks[block->data.size-1];
	struct Tape *tapes = strip->elem;
	struct Tape *newtape = &tapes[strip->size++];*/

	struct Stack *strip = Stack_get(&block->data, block->data.size-1);
	struct Tape *newtape = Stack_get(strip, strip->size++);

	*newtape = Tape_init();

	block->size++;
	return newtape;
}

// Makes a complete copy of the source tape
// with a new memory allocation
// 
void Tape_copy(struct Tape *dest, struct Tape *src)
{
	if (!src->tape.size) { 
		Stack_clear(&dest->tape);
		dest->head = 0;
		return;
	}

	Stack_copy(&dest->tape, &src->tape);
	dest->head = src->head;
	dest->ghost = 0;

	return;
}

// Dest tape and src tape object share
// the same underlying memory allocation,
// but have independent heads
// > Dest tape should be freshly initialized 
//   (empty), but if noti, it's own memory 
//   allocation will be free'd first
// > To be used with machines that
//   do not alter the tape contents (DFA,
//   NFA, PDA), or TM transitions that do
//   not write to tape
// > Dest tape inherits src tape head position
//
void Tape_ghost(struct Tape *dest, struct Tape *src)
{
	dest->tape = src->tape;
	dest->head = src->head;
	dest->ghost = 1;
	
	return;
}

// Prints a tape to console
// > max number of characters determined by '-w' command arg, default 60
// > head moves freely when close to either end of the tape
// > head stays in middle when not close to either end of the tape
// > ellipses (...) indicate whether tape ends extend beyond what is printed
// > printable ASCII characters appear as-is
// > decimal numbers and unprintable ASCII appear as decimals
//   -> remember: decimal 0 prints as "00", decimal 1 as "01", ... up to "09"
// > 'infinite' blanks on the edges of the tape are not printed
//
void Tape_print(struct Tape *tp0)
{
	// set trim for trailing blanks on right edge of tape
	int right_blank_trim = tp0->tape.size - 1;
	CELL_TYPE *cell = Stack_get(&tp0->tape, right_blank_trim);
	//ptrdiff_t sym = (cell) ? *cell : tm_blank;
	CELL_TYPE sym = (cell) ? *cell : tm_blank;
	while (sym == tm_blank && right_blank_trim > tp0->head) {
		cell = Stack_get(&tp0->tape, --right_blank_trim);
		sym = (cell) ? *cell : tm_blank;
	}

	// set trim for preceding blanks on left edge of tape
	int left_blank_trim = 0;
	cell = Stack_get(&tp0->tape, left_blank_trim);
	sym = (cell) ? *cell : tm_blank;
	while (sym == tm_blank && left_blank_trim < tp0->head) {
		cell = Stack_get(&tp0->tape, ++left_blank_trim);
		sym = (cell) ? *cell : tm_blank;
	}

	int buff_len = tape_print_width * print_max + 1;

	int left_begin = tp0->head - tape_print_width;
	int left_end = tp0->head;
	int right_begin = tp0->head + 1;
	int right_end = tp0->head + tape_print_width;

	// adjust begin and end indexes to account for leading/trailing blanks	
	if (left_blank_trim > left_begin) left_begin = left_blank_trim;
	if (right_blank_trim < right_end) right_end = right_blank_trim;

	// fill left buffer with printed symbols
	char buff_left[buff_len];
	buff_left[0] = '\0';
	char *fmt = NULL;
	unsigned int num_chars_left = 0;
	int prev_dec = 0;

	for (int i = left_begin; i < left_end || i < tp0->head; i++) {
		cell = Stack_get(&tp0->tape, i);
		sym = (cell) ? *cell : tm_blank;
		
		if (sym > 31 && sym < 127) {
			if (i != left_begin && prev_dec) fmt = "|%c";
			else fmt = "%c";
			prev_dec = 0;
		} else {
			if (i != left_begin) 
				fmt = (sign) ? "|%02" SIGNED_PRINT_FMT : "|%02" UNSIGNED_PRINT_FMT;
			else 
				fmt = (sign) ? "%02" SIGNED_PRINT_FMT : "%02" UNSIGNED_PRINT_FMT;

			prev_dec = 1;
		}
		num_chars_left += (sign) ? 
			snprintf(buff_left + num_chars_left, buff_len, fmt, (SIGNED_PRINT_TYPE)sym) :
			snprintf(buff_left + num_chars_left, buff_len, fmt, (UNSIGNED_PRINT_TYPE)sym);
	}

	// fill head buffer with head symbol and indicator brackets
	cell = Stack_get(&tp0->tape, tp0->head);
	sym = (cell) ? *cell : tm_blank;

	if (sym > 31 && sym < 127) {
		fmt = "[%c]";
		prev_dec = 0;
	} else if (tp0->head <= 0) {                  // decimal spacers:
		fmt = (sign) ? 
			"[%02" SIGNED_PRINT_FMT "]|" : 
			"[%02" UNSIGNED_PRINT_FMT "]|";    // left end of tape
		prev_dec = 1;
	} else if (tp0->head >= right_end) {
		fmt = (sign) ? 
			"|[%02" SIGNED_PRINT_FMT "]" : 
			"|[%02" UNSIGNED_PRINT_FMT "]";    // right end of tape
		prev_dec = 1;
	} else {
		fmt = (sign) ? 
			"|[%02" SIGNED_PRINT_FMT  "]|" : 
			"|[%02" UNSIGNED_PRINT_FMT "]|";  // within tape
		prev_dec = 1;
	}

	char buff_head[print_max + 5]; // +1 for null term, +2 for brackets, +2 for decimal spacers
	unsigned int num_chars_head = (sign) ? 
		snprintf(buff_head, print_max + 5, fmt, (SIGNED_PRINT_TYPE)sym) :
		snprintf(buff_head, print_max + 5, fmt, (UNSIGNED_PRINT_TYPE)sym);

	// fill right buffer with printed symbols
	char buff_right[buff_len];
	buff_right[0] = '\0';
	unsigned int num_chars_right = 0;
	for (int i = right_begin; i <= right_end; i++) {
		cell = Stack_get(&tp0->tape, i);
		sym = (cell) ? *cell : tm_blank;

		if (sym > 31 && sym < 127) {
			if (prev_dec && i != right_begin) fmt = "|%c";
			else fmt = "%c";
			prev_dec = 0;
		} else {
			if (i != right_begin || !prev_dec) 
				fmt = (sign) ? "|%02" SIGNED_PRINT_FMT : "|%02" UNSIGNED_PRINT_FMT;
			else 
				fmt = (sign) ? "%02" SIGNED_PRINT_FMT : "%02" UNSIGNED_PRINT_FMT;

			prev_dec = 1;
		}
		
		num_chars_right += (sign) ? 
			snprintf(buff_right + num_chars_right, buff_len, fmt, (SIGNED_PRINT_TYPE)sym) :
			snprintf(buff_right + num_chars_right, buff_len, fmt, (UNSIGNED_PRINT_TYPE)sym);
	}

	// If printed chars in all buffers are within the print limit, print them as-is
	if (num_chars_left + num_chars_head + num_chars_right <= tape_print_width) {
		printf("   %s%s%s", buff_left, buff_head, buff_right);
		return;
	}

	int tape_len_half = tape_print_width / 2;
	int trim_left = 0;
	int trim_right = tape_len_half;
	int num_chars_head_half = num_chars_head / 2;
	int num_chars_head_rem = num_chars_head % 2;

	unsigned int limit_left = tape_len_half - num_chars_head_half;
	unsigned int limit_right = tape_len_half - num_chars_head_half - num_chars_head_rem;

	// Left and right buffers each exceed ~half the print limit
	if (num_chars_left > limit_left && num_chars_right > limit_right) {
		trim_left = num_chars_left - tape_len_half + num_chars_head_half;
		trim_right = tape_print_width - (num_chars_left + num_chars_head - trim_left);
		printf("...%s%s%.*s...", buff_left + trim_left, buff_head, trim_right, buff_right);
		return;
	}
	
	// Right end of the tape
	if (num_chars_left > limit_left) {
		trim_left = num_chars_left - (tape_print_width - num_chars_right - num_chars_head);
		trim_right = num_chars_right;
		printf("...%s%s%.*s", buff_left + trim_left, buff_head, trim_right, buff_right);
	
	// Left end of the tape
	} else if (num_chars_right > limit_right) {
		trim_right = tape_print_width - num_chars_left - num_chars_head;
		printf("   %s%s%.*s...", buff_left + trim_left, buff_head, trim_right, buff_right);
	
	// Middle catch-all (size of head buffer barely puts length over print limit)
	} else {
		trim_left = num_chars_head_half;
		trim_right -= num_chars_head_half - num_chars_head_rem;
		printf("...%s%s%.*s...", buff_left + trim_left, buff_head, trim_right, buff_right);
	}

	// DEBUG:
	//	printf("trim_L:  %d,  trim_R: %d\n", trim_left, trim_right);
	//	printf("nchar_L: %d, nchar_R: %d\n", num_chars_left, num_chars_right);

	return;

}

// Takes a stack (list) of delimiters and a symbol
// and determines if the symbol is a delimiter
//
// Returns 1 if delim, 0 if not delim
int isdelim(struct Stack *delims, CELL_TYPE sym)
{
	CELL_TYPE *exists = bsearch(&sym, delims->elem, delims->size, sizeof(CELL_TYPE), symcmp);
	return (exists && 1);
}

// Imports an ASCII string from the command line or
// a text file into a Tape struct containing an
// array of symbols cast to CELL_TYPE (defined in auto.h)
// 
// Processes single ASCII characters, or interprets sequences
// of ASCII digits as decimals, as indicated by an input
// delimiter. This delimiter is supplied as a single 
// printable ASCII char following the '-d' command-line
// argument. Multiple delimiters may be specified, ie:
//
//   ./tmf -f stuff.tmf -d %          (1 delimiter)
//   ./tmf -f stuff.tmf -d % -d ,     (2 delimiters)
//   ./tmf -f stuff.tmf -d ws         (6 delimiters)
//
// The final example above shows use of the special 'ws'
// value, which instructs every character considered
// to be whitespace to be added as a delimiter. Look up
// 'C isspace()' online to see what is considered
// whitespace.
//
// The delimiter must completely surround a sequence of 
// digits for that sequence to be interpreted as a
// decimal. The only exceptions to this rule are the
// very first (leftmost) and very last (rightmost) cells
// of the initial input tape.
//
// The delimiter may exist any number of times between
// any cell.
//
// For the delimiter to be interpreted literally in
// the input tape, its decimal equivalent must be used.
// The examples below use a '%' as the delimiter.
//
// To specify ASCII decimals >= 0 and <= 9, prepend
// a '0' before them while using the delimiters.
//
// > 00 = ASCII decimal zero
// > 09 = ASCII decimal nine
// >  0 = ASCII decimal forty-eight
// >  9 = ASCII decimal fifty-seven
//
// > Individual cells in the 'imported' column are
//   separated by pipe characters '|'.
//
// RAW TEXT:              IMPORTED TAPE:      # CELLS:  EXPLANATION:
// -123%                  |-123|              1         
// %-123                  |-123|              1
// -123                   |-|1|2|3|           4         No delim, all chars are printable ASCII
// 26%a%b%c%127           |26|a|b|c|127|      5         Optional delims between printable ASCII
// %26%a%b%c%127%         |26|a|b|c|127|      5         Optional delims on tape ends
// 26%abc%127             |26|a|b|c|127|      5         Delims between printable ASCII are optional
// 26%abc127              |26|a|b|c|1|2|7|    7         No delim before 127, so each digit is a cell
// 26%abc127%             |26|a|b|c|1|2|7|    7         No delim between 'c' and '1', each digit a cell
// 26%%%A%127             |26|A|127|          3         Multiple delims have same effect as one.
// %37%ABC%37%            |%|A|B|C|%|         5         Delimiter decimal ('%' = 37) specified on tape
// ABC%0%1%2%             |A|B|C|0|1|2|       6         All chars are printable ASCII
// ABC%48%49%50%          |A|B|C|0|1|2|       6         Printable ASCII supplied by their decimal values
// ABC%00%01%02           |A|B|C|00|01|02|    6         Decimals 0, 1, and 2 supplied
//
//
// Returns struct Tape
struct Tape Tape_import(struct Automaton *a0, char *input_str, int is_file)
{	
	struct Tape input_tape = Tape_init_max(initial_tape_size);
	
	FILE *tape_file = NULL;
	if (is_file) {
		tape_file = fopen(input_str, "r");
		if (tape_file == NULL) {
			fprintf(stderr, "Error opening input string/tape file %s\n", input_str);
			exit(EXIT_FAILURE);
		}
	} else if (input_str[0] == '\0') {
		return input_tape;
	}
	
	size_t input_ctr = 0;
	char c = 0;
	CELL_TYPE cell = 0;

	if (a0->delims.size) {
		
		struct Stack digitacc = Stack_init(CHAR);
		struct Stack *delims = &a0->delims;
		CELL_TYPE tmpc;
		char *endptr = NULL;
		char *special = NULL;
		char nterm = '\0';
		int mystate = 1;

		while ((is_file && ((c = fgetc(tape_file)) != EOF)) || 
				 (!is_file && (c = input_str[input_ctr++]) != '\0')) {
			
			cell = c;

			switch(mystate) {
				case 1:
					if (isdigit(cell) || cell == '-') {
						Stack_push(&digitacc, &c);

						mystate = 4;
					} else if (isdelim(delims, cell)) {
						mystate = 2;
					} else {
						Stack_push(&input_tape.tape, &cell);

						mystate = 3;
					}
					break;
				case 2:
					if (isdigit(cell) || cell == '-') {
						Stack_push(&digitacc, &c);

						mystate = 4;
					} else if (isdelim(delims, cell)) {
						mystate = 2;
					} else {
						Stack_push(&input_tape.tape, &cell);

						mystate = 3;
					}
					break;
				case 3:
					if (isdelim(delims, cell)) {
						mystate = 2;
					} else {
						Stack_push(&input_tape.tape, &cell);

						mystate = 3;
					}
					break;
				case 4:
					if (isdelim(delims, cell)) {
						tmpc = *(char *)Stack_pop(&digitacc);
						Stack_push(&input_tape.tape, &tmpc);

						mystate = 2;
					} else if (isdigit(cell)) {
						Stack_push(&digitacc, &c);

						mystate = 5;
					} else {
						for (unsigned int i = 0; i < digitacc.size; i++) {
							tmpc = *(char *)Stack_get(&digitacc, i);
							Stack_push(&input_tape.tape, &tmpc);
						}
						Stack_push(&input_tape.tape, &cell);
						Stack_clear(&digitacc);

						mystate = 3;
					}
					break;
				case 5:
					if (isdigit(cell)) {
						Stack_push(&digitacc, &c);

						mystate = 5;
					} else if (isdelim(delims, cell)) {
						Stack_push(&digitacc, &nterm);
						special = digitacc.elem;
						tmpc = DECIMAL_IMPORT_FUNC;
						Stack_push(&input_tape.tape, &tmpc);
						Stack_clear(&digitacc);

						mystate = 2;
					} else {
						for (unsigned int i = 0; i < digitacc.size; i++) {
							tmpc = *(char *)Stack_get(&digitacc, i);
							Stack_push(&input_tape.tape, &tmpc);
						}
						Stack_push(&input_tape.tape, &cell);
						Stack_clear(&digitacc);
	
						mystate = 3;
					}
					break;
			}
		}

		if (digitacc.size == 1) {
			tmpc = *(char *)Stack_pop(&digitacc);
			Stack_push(&input_tape.tape, &tmpc);
		} else if (digitacc.size) {
			Stack_push(&digitacc, &nterm);
			special = digitacc.elem;
			tmpc = DECIMAL_IMPORT_FUNC;
			Stack_push(&input_tape.tape, &tmpc);
		}

		Stack_free(&digitacc);

	} else {
		
		while ((is_file && (c = fgetc(tape_file)) != EOF) || 
				 (!is_file && (c = input_str[input_ctr++]) != '\0')) {
			cell = c;
			Stack_push(&input_tape.tape, &cell);
		}
		//CELL_TYPE *tmpelem = input_tape.tape.elem;
		//for (size_t i = input_tape.tape.size; i < input_tape.tape.max; i++) tmpelem[i] = tm_blank;
	}

	if (is_file) fclose(tape_file);

	return input_tape;
}
