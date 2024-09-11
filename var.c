#include <stdio.h>
#include <string.h>
#include <xxhash.h>

#include "var.h"
#include "auto.h"
#include "stack.h"
#include "block.h"

// Creates variable from provided name
// > syms contains the actual elements of the var
// > edits contains modifications/reassignments
//   of the variable
//
// Returns static var
/*
struct Var Var_init(char *name)
{
	struct Var var;
	var.syms = Stack_init(CELL);
	//var.edits = Stack_init(CHAR);
	var.edits = Stack_init(CHARPTR);

	//snprintf(var.name, sizeof(var.name), "%s", name);
	var.name = Name_add(a0, name);
	
	return var;
}
*/
// Get the variable
//
// Returns pointer to variable or NULL if one with given name doesn't exist
struct Var *Var_get(struct Automaton *a0, char *name)
{
	struct Block *block = &a0->vars;
	struct Var **hash_elem = block->hash.elem;
	
	XXH64_hash_t check = XXH3_64bits(name, strlen(name)) % block->hashmax;
	XXH64_hash_t offset = check;
	
	struct Var *varcheck = hash_elem[offset];
	while (varcheck != NULL) {
		if (!strcmp(varcheck->name, name)) return varcheck;
		if (++offset == block->hashmax) offset = 0;
		varcheck = hash_elem[offset];
	}
	return NULL;
}

// Add variable to vars block of Automaton
//
// Returns pointer to new/existing variable
struct Var *Var_add(struct Automaton *a0, char *name)
{
	struct Block *block = &a0->vars;
	struct Var **hash_elem = block->hash.elem;

	XXH64_hash_t check = XXH3_64bits(name, strlen(name)) % block->hashmax;
	XXH64_hash_t offset = check;
	
	struct Var *varcheck = hash_elem[offset];
	while (varcheck != NULL) {
		if (!strcmp(varcheck->name, name)) return varcheck;
		if (++offset == block->hashmax) offset = 0;
		varcheck = hash_elem[offset];
	}

	if (block->size >= block->max) {
		Block_grow(block);

		hash_elem = block->hash.elem;
		
		check = XXH3_64bits(name, strlen(name)) % block->hashmax;
		offset = check;
		
		varcheck = hash_elem[offset];
		while (varcheck != NULL) {
			if (!strcmp(varcheck->name, name)) return varcheck;
			if (++offset == block->hashmax) offset = 0;
			varcheck = hash_elem[offset];
		}
	}
	
	/*struct Stack *datastacks = block->data.elem;
	struct Stack *strip = &datastacks[block->data.size-1];
	struct Var *vars = strip->elem;
	struct Var *newvar = &vars[strip->size++];*/

	struct Stack *strip = Stack_get(&block->data, block->data.size-1);
	struct Var *newvar = Stack_get(strip, strip->size++);

	hash_elem[offset] = newvar;

	newvar->syms = Stack_init(CELL);
	newvar->edits = Stack_init(CHARPTR);
	newvar->name = Name_add(a0, name, 0);

	block->size++;	
	return newvar;
}

// Takes a char * representing a change to a variable and pushes
// it onto the variable's edit stack
// > var's edit stack consists of char *pointers
// > edit contents are stored in the Automaton's name block
//
// Returns void
void Var_edit_add(struct Automaton *a0, struct Var *var, char *edit)
{
	char *newedit = Name_add(a0, edit, 0);
	Stack_push(&var->edits, &newedit);
}

// Frees syms and edits stacks within var
//
// Returns void
size_t Var_free(struct Var *var)
{
	return Stack_free(&var->syms) + Stack_free(&var->edits);
}
