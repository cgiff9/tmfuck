# tmfuck

A simple interpreted language for building Turing machines and other automata
- [x] Determinstic finite automata (DFA)
- [x] Nondeterministic finite automata (NFA)
- [x] Pushdown automata (PDA)
- [x] Deterministic pushdown automata (DPDA)
- [x] Turing machines
- [x] Nondeterministic Turing machines

While the name of this language is inspired by the venerable migrane-inducing 
[brainfuck](https://esolangs.org/wiki/Brainfuck), it is the main goal of this project to 
make the textual design of Turing machines and other automata a more comprehensible excercise,
especially for any struggling students out there.
<br />
<br />
That being said, there is an element of brainfuckery that is unavoidable when exploring these
topics. You have been warned!

## Building
```
make tmf
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
$ ./tmf <machine file> <input string>
$ ./tmf samples/dfa_divBy8.txt 1000
=>1000
	ACCEPTED
```
A DFA, NFA, PDA, or TM file can be supplied via the first
"non-option" argument. The input string is
supplied via the second "non-option" argument.

## File Format
### Directives
There are five directives that govern important aspects
of the machine file:
#### Syntax
```
start:   [one state];
final:   [comma-separated list of states];
reject:  [comma-separated list of states];
blank:   [one character];
bound:   [L | R | H | (empty) ];
```
#### Meaning
```
start:   + the state from which a machine begins
final:   + the state(s) that signal the end of a machine 
           and acceptance of the input string
reject:  + the states(s) that signal the immediate end of 
           a Turing machine and rejection of the input string
blank:   + the character that "fills" the infinite end(s) of
           a Turing machine's tape. Default is '_'
bound:   + indicates which end of a Turing machine's tape is
           not filled with infinite blanks. Default is
           empty (no character), meaning both tape ends are
           infinite.
```
For all types of automata, the "start: " and "final: " 
directives are required. The "reject: ", "blank: ", and
"bound: " directives apply only to Turing machines and are 
optional.
<br />
<br />
Understand that these directives are considered special
state names, so please avoid referring to these names 
when defining transitions. The "final: " and "reject: " 
directives can be used on multiple lines and will be 
aggregated. If the "start: ", "blank: ", and "bound: "
directives are used on multiple lines, the lowest line 
(and its last listed element) will be used.

### General Syntax
State names are defined by strings which contain no spaces 
followed by a colon ':'. Following the colon, on the same line
or any number of new lines, may be a list of transitions defined 
for that state. Each transition definition must be followed by a 
semicolon ';'.
<br />
<br />
States which do not require any transitions can still be 
referenced by the transition definitions of other states. 
This can be helpful for certain "final: " or "reject: "
states that simply act to terminate the program and require no
more input/tape processing. That being said, it is still perfectly 
valid to define a state that contains zero transitions (ie. "q5: ").
<br />
<br />
The file format is quite forgiving of whitespace and empty lines, 
but understand that state names themselves may not contain any 
whitespace. Special characters that would otherwise be considered 
operators by the program may be defined within single quotes (' ') 
for use in transitions. Code comments can be added in a manner similar
to many shells by using the special character '#'.

### Transitions
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
q1: a> q0; b >q3;
```
Multiple transition characters may be defined as a
comma-separated list:
```
q0: 0, 1, 2, ';', a >q0;
q1: a,b,c > q2;
```
Following the '>' character can be a comma-separated
list of state names:
```
q0: 'a' >q0,q1;
q1: 'a','b','c' > q1, q2; 
```
For nondeterministic automata, an empty string (epsilon) transition
can be supplied by typing '>' with no preceding characters:
```
q1: >q0,q2; a>q0;
q2: 
    a > q0;
    > q1, q2;
```

#### Pushdown Automata
Additional transition information can be supplied to design
pushdown automata, or PDAs. Here's an example that recognizes a 
number of 0s followed by the same number of 1s:
```
start: q1;
final: q1, q4;
q1: 
    >q2 (>$);   # go to q2 and push initial stack symbol '$'
q2: 
    0>q2 (>0);  # upon input '0', go to q2 and push '0' to stack
    1>q3 (0>);  # upon input '1', go to q3 and pop '0' from stack
q3: 
    1>q3 (0>);  # upon input '1', stay in q3 and pop '0' from stack
    >q4 ($>);   # go to q4 and pop '$' from stack
q4:
```
Look, now you can actually [parse HTML](https://stackoverflow.com/a/1732454)!
<br />
<br />
In parentheses following the destination state of
the transition, stack operations can be defined.
```
( > x )        push 'x'
( x > )        pop 'x'
( x > y )      pop 'x' and push 'y'
```
These stack characters can also be specified within
single quotes.

#### Turing machines
Specifying transitions for Turing machines, or TMs, is done
in a similar way to PDAs. Rather than a separate stack, TMs employ 
an "input tape" whose head can change direction. After defining 
a destination state in the transition, a TM may either write a 
character to the tape, change direction, both, or neither.
<br />
<br />
Keep in mind that, whether intended or not, a TM can run forever 
if it has been designed to do so (CTRL+C may be your friend). Here
is an example TM that takes a binary string and increments it by one:
```
start: q0;
final: q2;
blank: @;
q0:
    0>q0 (R);
    1>q0 (R);
    @>q1 (L);
q1:
    1>q1 (>0,L);
    0>q2 (>1,L);
    @>q2 (>1,L);
q2:
```
The default blank symbol is the underscore '\_', but this can
be overridden by using the "blank: " directive. This blank character can 
be specified within single quotes.
<br />
<br />
By default, both the left and right ends of the tape are considered 
to be filled with "infinite" blanks. This behavior can be changed with the 
"bound: " directive; 'L' indicates the tape is bounded at the left end, and 'R'
indicates the tape is bounded at the right end. If the head reaches the bounded 
end of the tape and tries to move further in the same direction, it will simply
stay in place (and still apply any write operation specified). However, this behavior
can also be overridden by listing 'H' (for halt) before or after 'R' or 'L'. 
To clarify via some examples:
```
bound: L;
```
I'll call this Sipser style: the tape only extends infinitely to the right, and if the head
tries to move left of the beginning, it stays in place (and still applies a write operation if
given).
```
bound: L, H;
```
In this example, the tape also extends infinitely to the right only, but if the tape head tries to
move left of the tape's beginning, the TM halts and rejects (for that branch). This 
behavior is less common, but I read about it being used somewhere and thought it should be available.
Bounding the right end of the tape with 'R' (and extending infinitely to the left) is also 
a rather unusual option, but who am I to judge? :)
<br />
<br />
Further acknowledging the varying formal definitions out there, 
please note the "reject:" directive is also optional for Turing machines, although they are commonly
employed in the wild. In this program multiple reject states can be listed. I'm not sure why you'd ever 
need more than one reject state, but again, who am I to judge? ;)

## Sleep
The sleep command-line option '-s' may be useful in seeing how your
machine computes in real time. Depending on the machine type, the progress of the tape, stack, and/or 
input string is shown after each state transition. For example, the following command will sleep 
250 milliseconds between each verbose output step:
```
$ ./tmf samples/tm_0lenPow2.txt 00000000 -s 0.25
```

## Regex
Using the '-r' argument, a regex string may be supplied
supporting a few very basic operations:
```
a*          Kleene star
a+          Repeat at least once
a|b         Union
```
An NFA is built from this regex and matched against
the string(s) supplied. Example usage for regex:
```
$./tmf -r '(ab)*' ababab
=>ababab
	ACCEPTED
```
This NFA, or any NFA supplied by file, can be converted
to a minimal DFA with the '-d' argument.

## Disclaimer
I'm quite confident with how this program handles DFAs, NFAs, PDAs, and TMs.
Things get quite gnarly when it comes to nondeterministic TMs though, so I
cannot guarantee this program will execute those properly for every configuration yet.
<br />
<br />
Also, once upon a time "otto" was the name of this repo and program, but that name is
already shared by a good number of other repos out there. I hate to part with a 
palindrome, but here's to tmfuck!