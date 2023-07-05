# OTTO

A tool for building DFAs, NFAs,
and PDAs.

## Building
```
make otto
```

## Arguments
```
-v              verbose
-d              make deterministic
-r <string>     regex string
-m              minimize (DFA)
-f <file>       input string file
```
The verbose flag will show state transition
information. The file supplied to the '-f' 
argument may contain multiple strings with 
one per line.

## Usage
```
$ ./otto <machine file> <input string>
$ ./otto samples/auto_divBy8.txt 1000
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

## Pushdown Automata
The file format can be extended to support pushdown 
automata, or PDAs. The following example recognizes
a number of 0s followed by the same number of 1s:
```
start: q1;
final: q1, q4;
q1: >q2 (>$);
q2: 0>q2 (>0); 1>q3 (0>);
q3: 1>q3 (0>); >q4 ($>);
q4:
```
In parentheses following the destination state of
the transition, stack operations can be defined.
```
(>x)         push an 'x'
(x>)         pop an 'x'
(x>y)        pop 'x' and push 'y'
```
These stack characters can also be specified within
single quotes.

## Regex
Using the '-r' argument, a regex string may be supplied
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
This NFA, or any NFA supplied by file, can be converted
to a minimal DFA with the '-d' argument.
