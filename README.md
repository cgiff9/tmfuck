# OTTO

A tool for playing around with DFAs and NFAs

## Building
```
make otto
```

## Arguments
```
-v				verbose
-d				make deterministic
-r <string>			regex string
-m				minimize (DFA)
-f <file>			input string file
```
The verbose flag will show state transition
information. The file supplied to '-f' may
contain multiple strings per line.

## Usage
```
$ ./otto <machine file> <input string>
$./otto samples/auto_divBy8.txt 1000
==>1000
	ACCEPTED
```
NFAs or DFAs can be supplied via the first
"non-option" argument. The input string is
supplied via the second "non-option" argument.

## File Format
```
start: q0;
final: q0, q1;
q0: 0>q0; 1>q1;
q1: 0>q2; 1>q1;
q2: 0>q3; 1>q1;
q3: 0>q0; 1>q1;
```
Each line other than "start: " and "final: "
represents a state. Comma-separated final states
may be supplied. For each line, the state is typed
followed by a colon. Transitions are specified
by a character, followed by a '>', followed by
a state name. Transition characters may be
specified with single quotes, ie.:

```
q0: ';'>q0; 'a'>q1;
```
Following the '>' character can be a comma-separated
list of state names:
```
q0: 'a'>q0,q1;
```
For NFAs, an empty string transition can be supplied
by typing '>' with no preceding character:
```
q1: >q0,q2; a>q0;
```

Note that each transition ends with a semicolon (;).


## Regex
Using the '-r' flag, a regex string may be supplied
supporting a few operations:
```
a*			Kleene star
a+			Repeat at least once
a|b			Union
```
An NFA is built from this regex and matched against
the string(s) supplied. Example usage for regex:
```
$./otto -r '(ab)*' ababab
==>ababab
	ACCEPTED
```
