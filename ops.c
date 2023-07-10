#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "auto.h"
#include "ops.h"

//int nsleep(long milliseconds)
int nsleep(double seconds)
{
	double fraction = seconds - ((long)seconds);
	long milliseconds = (seconds+(long)fraction*1000) * 1000;
	struct timespec req, rem;
	if(milliseconds > 999) {   
		req.tv_sec = (int)(milliseconds / 1000); 
		req.tv_nsec = (milliseconds - ((long)req.tv_sec * 1000)) * 1000000; 
	} else {   
		req.tv_sec = 0;
		req.tv_nsec = milliseconds * 1000000;
	}   
	return nanosleep(&req , &rem);
}

struct Automaton *e_closure(struct State *state)
{
	struct Automaton *a0 = Automaton_create();
	State_add(a0, state);
	for (int i = 0; i < a0->len; i++) {
		for (int j = 0; j < a0->states[i]->num_trans; j++) {
			if (a0->states[i]->trans[j]->symbol == '\0') {
				State_add(a0, a0->states[i]->trans[j]->state);
			}
		}
	}
	return a0;
}

struct AutomatonList *AutomatonList_create()
{
	struct AutomatonList *al0 = malloc(sizeof(struct AutomatonList));
	if (al0 == NULL) {
		fprintf(stderr, "Error allocating memory for AutomatonList\n");
		exit(EXIT_FAILURE);
	}
	al0->len = 0;
	al0->max_len = 2;
	al0->automatons = malloc(sizeof(struct Automaton *) * al0->max_len);
	if (al0->automatons == NULL) {
		fprintf(stderr, "Error allocating memory for list in AutomatonList\n");
		exit(EXIT_FAILURE);
	}
	return al0;
}

int Automaton_equiv(struct Automaton *a0, struct Automaton *a1)
{
	if (a0->len != a1->len) return 0;
	if (a0->len == 0 && a1->len == 0) return 1;
	for (int i = 0; i < a0->len; i++) {
		int c = 0;
		for (int j = 0; j < a1->len; j++) {
			if (a0->states[i] == a1->states[j]) c++;
		}
		if (c != 1) { 
			return 0;
		}
	}
	return 1;
}

int Automaton_get(struct AutomatonList *al0, struct Automaton *a0)
{
	for (int i = 0; i < al0->len; i++) {
		if (Automaton_equiv(al0->automatons[i], a0)) return i;
	}
	return -1;
}

int Automaton_add(struct AutomatonList *al0, struct Automaton *a0) 
{
	if (Automaton_get(al0, a0) > -1) return 0;
	al0->len++;
	if (al0->len > al0->max_len) {
		al0->max_len *= 2;
		al0->automatons = realloc(al0->automatons, sizeof(struct Automaton *) * al0->max_len);
		if (al0->automatons == NULL) {
			fprintf(stderr, "Error reallocating memory for list in AutomatonList\n");
			exit(EXIT_FAILURE);
		}
	}
	al0->automatons[al0->len-1] = a0;
	return 1;
}

int AutomatonList_equiv(struct AutomatonList *al0, struct AutomatonList *al1)
{
	if (al0->len != al1->len) return 0;
	if (al0->len == 0 && al1->len == 0) return 1;
	for (int i = 0; i < al0->len; i++) {
		struct Automaton *a0 = al0->automatons[i];
		if (Automaton_get(al1, a0) == - 1) return 0; 
	}
	return 1;
}

int State_group_index(struct AutomatonList *al0, struct State *s0)
{
		for (int i = 0; i < al0->len; i++) {
			struct State *stmp0 = State_get(al0->automatons[i], s0->name);
			if (s0 == stmp0) return i;
		}
		return -1;
}

// assumes s0 and s1 exist somewhere in al0
int States_grouped(struct AutomatonList *al0, struct State *s0, struct State *s1)
{
	for (int i = 0; i < s0->num_trans; i++) {
		for (int j = 0; j < s1->num_trans; j++) {
			if (s0->trans[i]->symbol == s1->trans[j]->symbol) {
				int s0_index = State_group_index(al0, s0->trans[i]->state);
				int s1_index = State_group_index(al0, s1->trans[j]->state);
				if (s0_index != s1_index) { //&& s0->trans[i]->state != s1->trans[i]->state)
					//printf("%s[%d] and %s[%d] are NOT grouped\n", s0->name, s0_index, s1->name, s1_index);
					return 0;
				}
			}
		}
	}
	//printf("%s and %s are grouped\n", s0->name, s1->name);
	return 1;
}


struct AutomatonList *partition(struct AutomatonList *al0, struct Automaton *a0)
{
	struct AutomatonList *al1 = AutomatonList_create();
	if (a0->len == 1) {
		struct Automaton *new0 = Automaton_create();
		State_add(new0, a0->states[0]);
		Automaton_add(al1, new0);
		return al1;
	}
	
	if (States_grouped(al0, a0->states[0], a0->states[1])) {
		struct Automaton *new0 = Automaton_create();
		State_add(new0, a0->states[0]);
		State_add(new0, a0->states[1]);
		Automaton_add(al1, new0);
	} else {
		struct Automaton *new0 = Automaton_create();
		struct Automaton *new1 = Automaton_create();
		State_add(new0, a0->states[0]);
		State_add(new1, a0->states[1]);
		Automaton_add(al1, new0);
		Automaton_add(al1, new1);
	}
	
	for (int i = 2; i < a0->len; i++) {
		struct State *stmp = a0->states[i];
		int matched = 0;
		for (int j = 0; j < al1->len; j++) {
			struct Automaton *atmp = al1->automatons[j];
			if (States_grouped(al0, stmp, atmp->states[0])) {
				State_add(atmp, stmp);
				matched = 1;
				break;
			} 
		}
		if (!matched) {
			struct Automaton *new0 = Automaton_create();
			State_add(new0, stmp);
			Automaton_add(al1, new0);
		}
	}
	
	return al1;
}

struct Automaton *purge_unreachable(struct Automaton *a0)
{
	struct Automaton *visited = Automaton_create();
	
	State_add(visited, a0->start);
	struct State *state = a0->start;
	for (int i = 0; i < visited->len; i++) {
		struct State *stmp = visited->states[i];
		for (int j = 0; j < stmp->num_trans; j++) {
			State_add(visited, stmp->trans[j]->state);
		}
	}
	visited->start = a0->start;
	
	return visited;
}

struct Automaton *DFA_minimize(struct Automaton *a0)
{
	// Remove unreachable states
	struct Automaton *a1 = purge_unreachable(a0);
	a0 = a1;
	
	// Set initial partition (final and non-final states)
	struct Automaton *other = Automaton_create();
	struct Automaton *final = Automaton_create();
	for (int i = 0; i < a0->len; i++) {
		if (a0->states[i]->final) State_add(final, a0->states[i]);
		else State_add(other, a0->states[i]);
	}
	
	
	struct AutomatonList *al0 = AutomatonList_create();
	struct AutomatonList *al1 = AutomatonList_create();
	
	if (other->len > 0) Automaton_add(al0, other);
	else Automaton_clear(other);
	if (final->len > 0) Automaton_add(al0, final);
	else Automaton_clear(final);
	
	struct AutomatonList *al2;
	for (int i = 0; i < al0->len; i++) {
		al2 = partition(al0, al0->automatons[i]);
		for (int j = 0; j < al2->len; j++)
			Automaton_add(al1, al2->automatons[j]);
		free(al2->automatons);
		free(al2);
	}
	
	// Partition each set of states
	while (!AutomatonList_equiv(al0, al1)) {
		for (int i = 0; i < al0->len; i++) Automaton_clear(al0->automatons[i]);
		free(al0->automatons);
		free(al0);
		al0 = al1;
		al1 = AutomatonList_create();
		
		for (int i = 0; i < al0->len; i++) {
			al2 = partition(al0, al0->automatons[i]);
			for (int j = 0; j < al2->len; j++)
				Automaton_add(al1, al2->automatons[j]);
			free(al2->automatons);
			free(al2);
		}
	}
	
	// sort al1 by ascending distance from start state
	int t[al1->len];
	memset(t, -1, sizeof(t));
	int t_len = 0;
	for (int i = 0; i < al1->len && t_len < al1->len; i++) {
		int state_index;
		if (i == 0) {
			state_index = State_group_index(al1, a0->start);
			t[0] = state_index;
			t_len++;
		} else
			state_index = t[i];
		
		struct Automaton *atmp = al1->automatons[state_index];
		struct State *stmp = atmp->states[0];
		for (int j = 0; j < stmp->num_trans && t_len < al1->len; j++) {
			int trans_index = State_group_index(al1, stmp->trans[j]->state);
			int contained = 0;
			for (int k = 0; k < al1->len; k++) {
				if (t[k] == trans_index) { 
					contained = 1;
					//printf("t already contains %d\n", trans_index);
					break;
				}
			}
			
			if (trans_index != state_index && !contained) {
				t[t_len] = trans_index;
				t_len++;
			}
		}
	}
	
	al2 = AutomatonList_create();
	for (int i = 0; i < al1->len; i++)
		Automaton_add(al2, al1->automatons[t[i]]);
	free(al1->automatons);
	free(al1);
	al1 = al2;
	
	// Fill min automaton with states
	struct Automaton *min = Automaton_create();
	for (int i = 0; i < al1->len; i++) {
		char name[STATE_NAME_MAX];
		snprintf(name, STATE_NAME_MAX, "q%d", i);
		State_name_add(min, name);
	}
	
	// Populate states with transitions
	for (int i = 0; i < al1->len; i++) {	
		
		struct Automaton *atmp = al1->automatons[i];
		struct State *stmp = atmp->states[0];
		for (int j = 0; j < stmp->num_trans; j++) {
			int trans_index = State_group_index(al1, stmp->trans[j]->state);
			struct Transition *new_trans = Transition_create(stmp->trans[j]->symbol, min->states[trans_index], '\0', '\0', '\0');
			Transition_add(min->states[i], new_trans);
		}
		
		for (int j = 0; j < atmp->len; j++) {
			if (atmp->states[j]->start) {
				min->states[i]->start = 1;
				min->start = min->states[i];
			}
			if (atmp->states[j]->final) {
				min->states[i]->final = 1;
				break;
			}
		}
	}
	
	Automaton_clear(a1);
	
	for (int i = 0; i < al0->len; i++) Automaton_clear(al0->automatons[i]);
	free(al0->automatons);
	free(al0);
	
	for (int i = 0; i < al1->len; i++) Automaton_clear(al1->automatons[i]);
	free(al1->automatons);
	free(al1);
	
	return min;
}

struct Automaton *nfa_to_dfa(struct Automaton *automaton)
{
	char alphabet[256];
	*alphabet = '\0';
	for (int i = 0; i < automaton->len; i++) {
		struct State *state = automaton->states[i];
		for (int j = 0; j < state->num_trans; j++) {
			if (strchr(alphabet, state->trans[j]->symbol) == NULL)
				strncat(alphabet, &state->trans[j]->symbol, 1);
		}
	}

	struct AutomatonList *al0 = AutomatonList_create();
	struct Automaton *start = e_closure(automaton->start);
	struct Automaton *a0 = Automaton_create();
	Automaton_add(al0, start);
	
	for (int i = 0; i < al0->len; i++) {
		char name[STATE_NAME_MAX];
		snprintf(name, STATE_NAME_MAX, "q%d", i);
		State_name_add(a0, name);

		// Build AutomatonList (sets of sets of states)
		for (int j = 0; alphabet[j] != '\0'; j++) {
			struct Automaton *atmp = al0->automatons[i];
			char symbol = alphabet[j];
			struct Automaton *new_auto = Automaton_create();
			for (int k = 0; k < atmp->len; k++) {
				struct State *stmp = atmp->states[k];
				for (int l = 0; l < stmp->num_trans; l++) {
					if (symbol == stmp->trans[l]->symbol) {
						struct Automaton *ectmp = e_closure(stmp->trans[l]->state);
						for (int h = 0; h < ectmp->len; h++) {
							State_add(new_auto, ectmp->states[h]);
						}
						Automaton_clear(ectmp);
					}
				}
			}
			// Add transitions
			int added = Automaton_add(al0, new_auto);
			int trans_index = Automaton_get(al0, new_auto);
			if (!added) Automaton_clear(new_auto);
			if (trans_index == a0->len) {
				snprintf(name,STATE_NAME_MAX, "q%d", a0->len);
				State_name_add(a0, name);
			}
			struct Transition *new_trans = Transition_create(symbol, a0->states[trans_index], '\0', '\0', '\0');
			Transition_add(a0->states[i], new_trans);
		}
		// Set final states
		for (int j = 0; j < al0->automatons[i]->len; j++) {
			struct State *s0 = al0->automatons[i]->states[j];
			if (s0->final) { 
				a0->states[i]->final = 1;
				break;
			}
		}
	}
	a0->start = a0->states[0];
	a0->start->start = 1;

	for (int i = 0; i < al0->len; i++) {
		Automaton_clear(al0->automatons[i]);
	}
	free(al0->automatons);
	free(al0);

	return a0;
}