#ifndef AUTO_H_
#define AUTO_H_

#define STATE_NAME_MAX 100

extern int flag_verbose;

struct Automaton {
	int max_len;
	int len;
	struct State *start;
	//struct Alphabet *alphabet;
	struct State **states;
};

struct Transition {
	char symbol;
	struct State *state;
};

struct State {
	char *name;
	int start;
	int final;
	struct Transition **trans;
	int num_trans;
	int max_trans;
};

struct State *State_create(char *name);
struct Automaton *Automaton_create();
struct State* get_state(struct Automaton *automaton, char *name);
void State_name_add(struct Automaton *automaton, char *name);
void State_add(struct Automaton *automaton, struct State *state);
struct Transition *Transition_create(char symbol, struct State *state);
void Transition_add(struct State *state, struct Transition *trans);
void State_destroy(struct State *state);
void Automaton_destroy(struct Automaton *automaton);
void Automaton_clear(struct Automaton *automaton);
void State_print(struct State *state);
void Automaton_print(struct Automaton *automaton);
int isnamechar(char c);
struct Automaton *Automaton_import(char *filename);
int isDFA(struct Automaton *automaton);
int DFA_run(struct Automaton *automaton, char *input);
int Automaton_run(struct Automaton *automaton, char *input);
void Automaton_run_file(struct Automaton *automaton, char *input_string_file);

#endif // AUTO_H_
