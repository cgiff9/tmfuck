#ifndef REGEX_H_
#define REGEX_H_

void remove_spaces(char* s);
int isregexsym(char c);
//int isregexsym(unsigned int c);
int is_valid_regex(char *regex);
char *infix(char *regex);
struct Automaton regex_to_nfa(char *regex);

#endif // REGEX_H_

