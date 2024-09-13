#ifndef FILE_H_
#define FILE_H_

#include "auto.h"
#include "inttypes.h"

/* #define SIGNED_IMPORT_FUNC  strtoll(special, &endptr, 10)
#define UNSIGNED_IMPORT_FUNC  strtoull(special, &endptr, 10)
#define DECIMAL_IMPORT_FUNC (issigned(CELL_TYPE)) ? (CELL_TYPE) SIGNED_IMPORT_FUNC : (CELL_TYPE) UNSIGNED_IMPORT_FUNC */

#define SIGNED_IMPORT_FUNC  strtoimax(special, &endptr, 10)
#define UNSIGNED_IMPORT_FUNC  strtoumax(special, &endptr, 10)
#define DECIMAL_IMPORT_FUNC (sign) ? (CELL_TYPE) SIGNED_IMPORT_FUNC : (CELL_TYPE) UNSIGNED_IMPORT_FUNC

extern int flag_verbose;
extern int verbose_inline;
extern double delay;
extern int execute;
extern CELL_TYPE tm_blank;
extern size_t initial_tape_size;
extern char tm_bound;
extern int tm_bound_halt;
extern int is_tm;
extern int is_pda;
extern int sign;
extern int print_max;
extern int debug;
extern unsigned int longest_name;
extern unsigned int longest_sym;
extern unsigned int tape_print_width;
extern size_t run_memory;

struct Automaton Automaton_import(char *filename);
//int Trans_sym_print(struct Trans *trans, int symtype);
int Trans_sym_print(CELL_TYPE sym);
int Trans_sym_conf_print(CELL_TYPE sym);
int Trans_sym_run_print(CELL_TYPE sym);
void Trans_print(struct Trans *trans);
void Automaton_print(struct Automaton *a0, int format);

int isnamechar(CELL_TYPE c);
unsigned int num_places (CELL_TYPE n);
int symcmp(const void *a, const void *b);

#endif // FILE_H_
