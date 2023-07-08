# OTTO

A tool for building DFAs, NFAs,
and PDAs.

## Building
```
make otto
```

## Arguments
```
-v                verbose
-f <file>         input string file
-d                convert NFA to DFA
-m                minimize DFA
-r <string>       regex string
-s <seconds>      sleep between verbose output steps
```
The verbose flag will show state transition
information. The file supplied to the '-f' 
argument may contain multiple strings with 
one per line.

## Usage
```
$ ./otto <machine file> <input string>
$ ./otto samples/dfa_divBy8.txt 1000
=>1000
	ACCEPTED
```
A DFA, NFA, or PDA can be supplied via the first
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
To specify a state's transitions, first type out
the state's name followed by a colon. Next type a
character that will trigger a transition, followed
by a '>', then followed by a destination state name. 
Transition characters may be specified within single 
quotes, ie.:

```
q0: ';'>q0; 'a'>q1;
```
Following the '>' character can be a comma-separated
list of state names:
```
q0: 'a'>q0,q1;
```
For NFAs and PDAs, an empty string (epsilon) transition
can be supplied by typing '>' with no preceding character:
```
q1: >q0,q2; a>q0;
```
Comments may be added with a preceding '#' on a whole
line or at the end of a line. Transitions may be specified
on lines below the state name, so long as each transition is
still followed by a semicolon:
```
q0: 
    0>q0;
    1>q1;
q1:
    0>q2;
    1>q1;
```

## Pushdown Automata
The file format can be extended to support pushdown 
automata, or PDAs. The following example recognizes
a number of 0s followed by the same number of 1s:
```
start: q1;
final: q1, q4;
q1: 
    >q2 (>$);   # push initial stack symbol '$'
q2: 
    0>q2 (>0);  # push '0' and go to q2
    1>q3 (0>);  # pop '0' and go to q3
q3: 
    1>q3 (0>);  # pop '0' and stay in q3
    >q4 ($>);   # pop '$'
q4:
```
In parentheses following the destination state of
the transition, stack operations can be defined.
```
(>x)        push 'x'
(x>)        pop 'x'
(x>y)       pop 'x' and push 'y'
```
These stack characters can also be specified within
single quotes.

## Regex
Using the '-r' argument, a regex string may be supplied
supporting a few operations:
```
a*          Kleene star
a+          Repeat at least once
a|b         Union
```
An NFA is built from this regex and matched against
the string(s) supplied. Example usage for regex:
```
$./otto -r '(ab)*' ababab
=>ababab
	ACCEPTED
```
This NFA, or any NFA supplied by file, can be converted
to a minimal DFA with the '-d' argument.
