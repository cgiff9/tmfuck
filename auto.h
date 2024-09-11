#ifndef AUTO_H_
#define AUTO_H_

#include <stddef.h>
#include <stdint.h>

#include <xxhash.h>

#include "block.h"
#include "stack.h"

#define STR(x)   #x
#define SHOW_DEFINE_FULL(x) fprintf(stderr, "%s = %s", #x, STR(x))
#define SHOW_DEFINE_VALUE(x) fprintf(stderr, "%s", STR(x))

/*===========================================*/
//
// CELL_TYPE MACRO (BE CAREFUL!)
//
// ------------------------------------------
//
// Below, you can define the type of each 'cell'
// or 'input symbol' in the Automaton. Here, 'cell'
// refers to both the units of a Turing machine
// tape/input string AND the units of the stack. 
//
// If your alphabet is limited to ASCII, keeping cell 
// types as 'char' or 'unsigned char' will 
// ultimately save you some memory, especially
// for machines with very many states and transitions.
//
// Hopefully this goes without saying-- this value 
// must be set before compilation. If that
// is news to you, I would exit this file 
// immediately and do some research on 'C macros'
// and 'C variable types'.
//
// The value for CELL_TYPE should be nothing other 
// than an int-based type (ie. char, short, long, etc). 
// It may be signed or unsigned. Using floats,
// doubles, or other datatypes not based on int
// is undefined and likely won't even compile.
//
// Types tested: 
// > signed/unsigned char
// > signed/unsigned short 
// > signed/unsigned int
// > signed/unsigned long
// > uint8_t, int8_t, int16_t, uint16_t,...
// > ptrdiff_t
// > size_t
// > intmax_t
// > uintmax_t
//
// Default type: short
// > On my system: 
//   SHRT_MIN = -32768 
//   SHRT_MAX = 32767


   #define CELL_TYPE short

/*===========================================*/

// Determines the default maximum number of
// characters shown in a tape or stack during
// verbose modes. This can be overridden with
// the -w command line argument, so only
// change this if you're tired of doing that.
//
// Values less than 9 can get messy in the
// general case, and values less than the
// 'longest symbol' defined in your conf
// file is likely to get messy.


   #define TAPE_PRINT_MAX 60

/*===========================================*/

// Determines which datatype symbols of 
// CELL_TYPE will be cast to before being 
// printed in verbose mode.
//
// > Somewhat simplifies format specifiers
//   used in printf() function
//
// PRINT_TYPE: 
// > Actual data type the cell symbol is
//   cast to before printout.
//
// PRINT_FMT:  
// > Partial format specifier used by
//   printf() for the above datatype. This
//   format specifier is concatenated with
//   varying printf() format strings 
//   throughout the code.
//
// The _FMT macro values must be enclosed in
// double quotes, or they will not be 
// properly integrated into the printf()
// format strings across the code.
//
// The differentiation between signed and 
// unsigned mostly exists to ensure decimals
// are printed accurately when CELL_TYPE is
// set to the largest signed or unsigned 
// datatype on your system. In my case,
// these are ptrdiff_t (signed long long)
// and size_t (unsigned long long);
//
// Best to leave these alone, really.


   #define SIGNED_PRINT_TYPE intmax_t
   #define UNSIGNED_PRINT_TYPE uintmax_t

	#define SIGNED_PRINT_FMT "jd"
	#define UNSIGNED_PRINT_FMT "ju"

/*===========================================*/

// The pragma directive below can lessen 
// program's runtime memory usage when 
// compiling with gcc, potentially at the 
// expense of degraded performance due to 
// memory misalignment. 
//
// Disabled by default


/* #pragma pack(1) */

/*===========================================*/

extern CELL_TYPE CELL_MAX;
extern CELL_TYPE CELL_MIN;

// Macros below modified from Glyph's answer
// on StackOverflow: 
//
//   https://stackoverflow.com/a/7266266
//
// Determines min and max values for the
// CELL_TYPE datatype.


/* #define issigned(t) (((t)(-1)) < ((t) 0)) */

#define issigned(t) (!((t)0 - (t)1 > 0))

#define umaxof(t) (((0x1ULL << ((sizeof(t) * 8ULL) - 1ULL)) - 1ULL) | \
                    (0xFULL << ((sizeof(t) * 8ULL) - 4ULL)))

#define smaxof(t) (((0x1ULL << ((sizeof(t) * 8ULL) - 1ULL)) - 1ULL) | \
                    (0x7ULL << ((sizeof(t) * 8ULL) - 4ULL)))

#define maxof(t) ((uintmax_t) (issigned(t) ? smaxof(t) : umaxof(t)))

/* #define maxof(t) ((size_t) (issigned(t) ? smaxof(t) : umaxof(t))) */
/* #define maxof(t) ((unsigned long long) (issigned(t) ? smaxof(t) : umaxof(t))) */


struct State {
	char *name;
	//struct Stack *current;      // current stacks }
	//struct Stack *next;         // next stacks    } used by Automaton_run()
	//struct Stack *current_tapes;
	//struct Stack *next_tapes;
	//struct Stack *e_closure;

	// FLAGS:
	unsigned char start:1;
	unsigned char final:1;
	unsigned char reject:1;
	unsigned char epsilon:1;      // this state contains epsilon transitions
	unsigned char epsilon_mark:1; // used for detecting epsilon loops
	unsigned char syms:1;         // this state has symbol transitions
	unsigned char exec:1;         // this state has a command associated
};

struct Trans {
	struct State *pstate;
	struct State *dstate;
	
	CELL_TYPE inputsym;         // current symbol being read on input string/tape
	CELL_TYPE popsym;           // symbol to match and pop from top of stack
	CELL_TYPE pushsym;          // symbol to push to top of stack
	CELL_TYPE writesym;         // symbol to write to TM tape

   // FLAGS:	
	unsigned char dir:2;        // tape head direction	
                               // > 0 = stay, 1 = left, 2 = right
	unsigned char reject:1;
	unsigned char epsilon:1;       // this trans is an epsilon transition
	unsigned char epsilon_loop:1;  // following this trans will complete an 'epsilon loop'
	unsigned char epsilon_mark:2;
	unsigned char pop:1;           // this trans has a stack pop operation
	unsigned char push:1;          // this trans has a stack push operation
	unsigned char write:1;         // this trans has a tape write operation

	unsigned char strans:1;    // this trans shares an inputsym and pstate with another
	//unsigned char vargroup:2;  // this trans was added as part of a variable in the machine file
										// 0 = no, 1=yes, 2=yes(alt)
	//unsigned char symgroup:1;  // this trans was part of a list of symbols in the machine file
	unsigned char exec:1;      // this trans has a command associated 
};


struct Automaton {
	struct Block states;
	struct Block trans;
	struct Block vars;
	struct Block names;
	struct Block stacks;
	struct Block tapes;

	struct State *start;
	struct Stack finals;
	struct Stack rejects;

	struct Stack alphabet;
	struct Stack delims;

//	struct Stack symgroups;
//	struct Stack vargroups;
	unsigned char tm:1;
	unsigned char pda:1;
	
	unsigned char regex:1;
	unsigned char epsilon:1;
	unsigned char strans:1;
	unsigned char epsilon_loops:1;
};


size_t Automaton_free(struct Automaton *a0);
struct Automaton Automaton_init();
char *Name_add(struct Automaton *a0, char *name, int state);
struct State *State_add(struct Automaton *a0, char *name);
struct Trans *Trans_get(struct Automaton *a0, struct State *pstate, CELL_TYPE inputsym, 
		XXH64_hash_t *offset_prev, int epsilon);
//struct Trans *Trans_add(struct Automaton *a0, struct State *pstate, struct State *dstate, 
//		ptrdiff_t sym, ptrdiff_t pop, ptrdiff_t push, ptrdiff_t dir, ptrdiff_t write, int epsilon);
struct Trans *Trans_add(struct Automaton *a0, struct State *pstate, struct State *dstate, 
		CELL_TYPE *inputsym, CELL_TYPE *popsym, CELL_TYPE *pushsym, CELL_TYPE *dir, CELL_TYPE *writesym);
void set_start(struct Automaton *a0, struct State *s0);
void set_final(struct Automaton *a0, struct State *s0);
void set_reject(struct Automaton *a0, struct State *s0);

#endif // AUTO_H_
