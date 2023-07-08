#ifndef OPS_H_
#define OPS_H_

struct AutomatonList {
	int len;
	int max_len;
	struct Automaton **automatons;
};

int nsleep(double miliseconds);
struct Automaton *e_closure(struct State *state);
struct AutomatonList *AutomatonList_create();
int Automaton_equiv(struct Automaton *a0, struct Automaton *a1);
int Automaton_get(struct AutomatonList *al0, struct Automaton *a0);
int Automaton_add(struct AutomatonList *al0, struct Automaton *a0);
int AutomatonList_equiv(struct AutomatonList *al0, struct AutomatonList *al1);
int State_group_index(struct AutomatonList *al0, struct State *s0);
int States_grouped(struct AutomatonList *al0, struct State *s0, struct State *s1);
struct AutomatonList *partition(struct AutomatonList *al0, struct Automaton *a0);
struct Automaton *purge_unreachable(struct Automaton *a0);
struct Automaton *nfa_to_dfa(struct Automaton *automaton);
struct Automaton *DFA_minimize(struct Automaton *a0);
#endif // OPS_H_
