
# tmfuck

A simple free-form interpreted language for building Turing machines and other automata. Deterministic and nondeterministic variants of the following machine types are supported:
- [x] Finite automata (DFA, NFA)
- [x] Pushdown automata (PDA, DPDA)
- [x] Turing machines (TM, NTM)
- [ ] Multitape Turing machines (coming soon)

The name of this language is inspired by the venerable language [brainfuck](https://esolangs.org/wiki/Brainfuck).

**Language Features:**
+ Free-form (arrange code to your visual liking)
+ Define your alphabet on-the-fly (no need to specify it beforehand)
+ Define and manipulate [lists](#symbol-lists) of transition symbols using [variables](#symbol-variables)
+ ASCII and [integer](#integer-symbols) cell symbols
+ Code [comments](#comments)

### Example Machine
This [example](../main/samples/busy-beaver/tm_bb4_107.tmf) Turing machine will execute the 4th Busy Beaver function:
```
start: q0;
final: qh;
blank: 0;     #default blank is _ (underscore)

q0: 0 > q1(1,R); 1 > q1(1,L);
q1: 0 > q0(1,L); 1 > q2(0,L);
q2: 0 > qh(1,R); 1 > q3(1,L);
q3: 0 > q3(1,R); 1 > q0(0,R);
```
Feel free to explore the samples folder to gain a quick intuition of the syntax and features of tmf, which is explained in more detail below. You can also read [spec.tmf](../main/samples/spec.tmf), which specifies a DFA (finite state machine) written in tmf that accepts a valid tmf file.

## Building from source

### Dependencies
This program makes use of the [xxHash library](https://github.com/Cyan4973/xxHash), which is widely available via the package managers of various operating systems.

### Building
```
git clone https://github.com/cgiff9/tmfuck
cd tmfuck
make tmf
```
If your OS does not provide xxHash, or you wish not to use your OS's package, the provided script [xxhash_conf.sh](../main/xxhash_conf.sh) can help you set the include statements across the tmf source and download the needed files for xxHash:
```
./xxhash_conf.sh local
./xxhash_conf.sh download
make localtmf
```
Then you'll have a tmf binary compiled with the downloaded xxhash header and source file. If you wish to recompile using your system's installed xxHash library, you can reset the #include statements and compile normally:
```
./xxhash_conf.sh system
make tmf
```

## Usage
```
$ ./tmf <machine file> <input string>
```
Input string/tape via the command line:
```
$ ./tmf samples/dfa_divBy8.tmf 1000
```
Input string/tape via file:
```
$ ./tmf samples/dfa_divBy8.tmf -i mystring.txt
```
The entire contents of the input file are considered one string/tape, including spaces, newlines and other whitespace. See the [Delimiter](#input-stringtape-delimiters) section below on how to ignore any unintended characters, including whitespace.

### Parameters
```
-v                verbose newline mode
-p                verbose inline (in-place) mode
-i <file>         input string/tape file
-d <char>         input string/tape ASCII delimiter
-s <seconds>      sleep between verbose output steps (decimals allowed)
-c                print machine file to console
-cc               print machine file to console (neat)
-w <width>        maximum number of printed tape/stack characters
-n                force use of the nondeterministic engine
-r <regex>        regex string
-z                print debug information
```
## Syntax Highlighting
For ViM users, you can download a [vim syntax file](../main/extra/vim/tmf.vim) to provide some basic syntax highlighting when crafting *.tmf machines.

## Defining states and transitions
To define the transitions for a state, first type the state's name followed by a colon:
```
q0:
```
Next, type an input symbol, followed by a '>', then followed by some other destination state:
```
q0: A > q1
```
On input symbol 'A', the machine goes from state q0 to q1. Type a semicolon to indicate the end of this transition definition. All statements in tmf are terminated with with a semicolon (except state definitions which contain no transitions).
```
q0: A > q1;
```

### Pushdown Automata
If you're designing a pushdown automaton, you can specify a pop, push, or pop/push combination in parentheses following the destination state:
```
q0: A > q1 (>B);
```
In the above case, the machine pushes a 'B' to the stack before going to state q1.
```
q0: A > q1 (B>);
```
In the above case, the top of the stack must be a 'B' in order for the machine to pop from the stack and proceed to state q1. Else, the machine (or branch) rejects.
```
q0: A > q1 (B>A);
```
The above case is the same as the previous one, but it will push an 'A' to the stack if it is able to pop a 'B' from the stack first.

### Turing Machines
Direction and write symbols may also be specified in parentheses following the destination state. Starting from the basic initial example, let's make the input tape's head go left after reading input symbol 'A':
```
q0: A > q1 (L);
```
If we wish to write a symbol '7' to the tape before moving left:
```
q0: A > q1 (7, L);
```
Nothing is stopping you from using stack operations with Turing machine
instructions, either:
```
q0: A > q1 (7, L, >B);
```
In this case, on input symbol 'A', the machine pushes a 'B' to the stack, writes a 'Z' to the tape, then moves the tape head left. The pop operation imposes the same transition conditions mentioned in the "PDA" section above.

*NOTE: These three operations (tape write, tape direction, and stack pop/push) may be listed in any order between the parentheses, but each operation type may appear at most once.*

#### Tape Properties
The tape extends "infinitely" in both directions, and the default blank character is the '_' underscore. This character may be changed to any type of symbol, including integers outside the printable ASCII range (see the "Directives" section below).

## More Syntax

### Symbol Quoting
Any printable ASCII symbol, whether used for the input, stack, or tape-write operations, may be enclosed in single quotes. Quotes *must* be used when using tmf control characters as input symbols:
```
q0: ';' > q1;
```
#### Control Characters:
All the characters that must be single-quoted (or have their integer equivalents used instead) are listed below:
```
: > $ , ; ( ) ' \ # =
```
State and variable names may consist of non-whitespace printable characters not listed above.

### Symbol Lists
Multiple input symbols may be defined before the '>' character:
```
q0: A,B,C,'$',D > q1(L);
```
*WARNING: Symbol lists may not contain epsilon transitions.*

### Destination Lists
Multiple destinations my also be listed following the '>' character:
```
q0: A,B,C,D > q1(L), q2(1,R), q3;
```
*Note: The set of transitions defined in this statement will force tmf to process the machine with its nondeterministic engine.*

### Symbol Variables
A variable is simply a name followed by an '=', then followed comma-separated list of input symbols. Variables may be defined to stand in place of several input symbols. Notice the difference between variable declarations and variable references, which are similar in syntax to most shell scripting languages.
```
ALPHABET = A, B, C, 1207, ';', D, E ;     # declaration
FEWER = $ALPHABET \ D, E;                 # declaration, reference
ELSESYMS = $ALPHABET \ $FEWER             # declaration, two references
q0: $ALPHABET,Z,Y > q1;                   # reference inside sym list
q1: $FEWER > q2;                          # reference
q2: $ELSESYMS > q2;                       # reference
```
Here is what the above example looks like without variables:
```
q0: A, B, C, 1207, ';', D, E, Z, Y > q1;
q1: A, B, C, 1207, ';' > q2;
q2: D, E > q2;
```
Note how variables and individual symbols may be listed together in transition statements and variable definitions. Also note the terminating semicolon after the list.

*WARNING: Variables may NOT contain epsilon transitions. Epsilon transitions must always be explicitly defined.*

#### Set-difference operator \\
When defining or changing a variable, the set-difference operator '\\' may be used to exclude elements on the right of the '\\' from elements on the left. This operator may only be used once per variable definition, and may not be used in transition definitions. Duplicate symbols are ignored, as tmf will never add a duplicate transition.

### Integer Symbols
Integers may be used as input, pop/push, and tape-write symbols. 

A single ASCII character will always be interpreted as that ASCII character. To specify integers in the range zero through nine, prepend a '0' to the input symbol:
```
q0: 01 > q1; 08 > q1; 8 > q1;
```
In this example, the integer values one, eight, and fifty-six will go to state q1. Consult an [ASCII table](https://www.asciitable.com/) for more information on the difference between ASCII character values and ASCII decimal values.

Negative integers may also be specified, as long as the datatype 
specified by the [CELL_TYPE](#cell_type-macro-advanced) macro (in file auto.h) is signed. Else, the C Language's casting rules will determine how the integer is made to fit within the range of CELL_TYPE.
```
q0: -119 > q1; -1 > q1; -8 > q1;
```
Notice that single-digit negative integers do not require the prepended '0'.

*Note:  ASCII printable characters will always be printed as such in verbose mode, even if they were specified as integers in the input tape/string.*

#### Input String/Tape Delimiters
Delimiters can be used to ignore extraneous characters in your input string/tape, and they are required to indicate integer symbols as well.

To specify integer values in the input tape/string, at least one of the printable
ASCII characters must be "sacrificed" as this delimiter to indicate whether a sequence of digits should be interpreted as an integer:
```
$ ./tmf blah.tmf -d % abc%1207%def
$ ./tmf blah.tmf -d % 1207%abcdef
$ ./tmf blah.tmf -d % abcdef%1207
```
In these samples, the sequence "1207" will be interpreted as a single cell with the integer value 1207. The other printable characters are treated normally, with each one being its own individual cell. A special value to the '-d' parameter can also be used to treat any [whitespace](https://en.cppreference.com/w/c/string/byte/isspace) as a delimiter:
```
% ./tmf blah.tmf -d ws -i input_string.txt
```
The "ws" value ensures that any amount of white space in the file "input_string.txt" will be considered as a separator between individual cells. Let's say input_string.txt looks like this:
```
1207
1852
32000
-934 836  a    bcd -9800
```
In this case, the input string/tape will contain the individual cells 1207, 1852, 32000, -934, 836, a, b, c, d, and -9800. Notice how the delimiters may appear any number of times between cells, and that the delimiters are optional between printable ASCII characters.
```
$./tmf blah.tmf -d % abc%37%def
```
Above, the input string is "abc%def". To specify a delimiter character as a literal cell in the input string, its decimal value must be used. In this case, ASCII character '%' has an ASCII decimal value of 37.

*WARNING: Delimiters are only for preprocessing cells in the input string/tape; do not take it into account when designing your machine file. Input string/tape delimiting is off by default.*

#### CELL_TYPE Macro (Advanced)
The type of each cell used in the stack and input string/tape may be changed BEFORE compilation. The default CELL_TYPE for tmf is the short (signed short int), which has a size of 2 Bytes and a range of -32768 to 32767 on many machines. This macro is defined and explained in further detail in the [auto.h](../main/auto.h) source file.

### Comments
Single-line (#) and inline (#\* ... \*#) comments are supported, and share a syntax and behavior similar to shell scripting comments and C-style inline comments:
<pre><code>q0: >q1(R); <b># qa: n > q1;</b>
q1: A > q2(B,L); 
<b>#* q2: H > q3;
q3: i > q4; *#</b></code></pre>
The commented code portions are shown in bold above. Being a free-form language, comments in tmf may be placed *almost* anywhere, so long as the code excluding them is valid.

## Nondeterminism
What this program considers to be deterministic and nondeterministic 
diverges somewhat from the theoretical definitions. For a machine to
run in tmf's nondeterministic engine, it must contain at least 
one transition with at least one of the following properties:
1. It specifies multiple destinations for a single input symbol.
2. It makes use of one or more epsilon transitions.

A machine with the above properties will be simulated using
the nondeterministic engine. The '-n' parameter may be used to force a "deterministic" machine to run in the nondeterministic engine, if you'd like to compare their performance.

### Epsilon Transitions
Epsilon transitions specify a destination that can be reached without consuming/reading an input symbol. They can be defined in tmf by simply omitting any input characters before the '>' character:
```
q0: > q1(L);
```
Epsilon transitions must always be explicitly invoked, meaning they can not be an element in an input symbol list or a variable.

## Machine File Directives
There are four directives that govern important aspects
of the machine file:
#### Syntax
```
start:   [one state];
final:   [comma-separated list of states];
reject:  [comma-separated list of states];
blank:   [one character];
```
**start:**   The state from which a machine begins.
**final:**   The state(s) that signal the end of a machine and acceptance of the input string.
**reject:**  The states(s) that signal the immediate end of a machine and rejection of the input string.
**blank:**   The character that "fills" the infinite end(s) of a machine's tape. Default is the underscore '_' character.

*Note: You can make and run machines without defining any final states; just know the implications of doing so for your machine type.*

*WARNING: Transitions cannot be defined for state names that match any of these four directive names.*

## Sleep
The sleep command-line option '-s' is useful in seeing how your machine computes input in real time. Depending on the machine type, the progress of the tape, stack, and/or input string is shown after each state transition. For example, the following command will sleep 250 milliseconds between each verbose in-place output step:
```
$ ./tmf samples/tm_0lenPow2.tmf 00000000 -p -s 0.25
```
### Verbose Mode
The '-v' verbose mode will print each transition/tape change to a new set of lines, while the '-p' verbose mode will print these changes in-place.

Understand that verbose mode adds significant overhead to the machine engine. A machine takes little time to compute normally may take an enormous amount of time to finish in verbose mode, especially depending on your specific terminal's settings.

## Regex
Using the '-r' argument, a regex string may be supplied
supporting a few very basic operations:
```
a*          Kleene star
a+          Repeat at least once
a|b         Union
```
This will generate an nondeterministic finite automaton using
[Thompson's construction](https://en.wikipedia.org/wiki/Thompson%27s_construction) that recognizes the language of the regex.

Only printable, non-whitespace ASCII characters excluding '(', ')', '*', '+', and '|' are currently supported for regex.

## Machine file output
If you would like to create a machine file from a regex (or any tmf machine), simply use the '-c' parameter with no input string, which will print the machine to console, where it can be redirected to a file:
```
$ ./tmf -r '(a|b|c|d)*' -c > myreg.tmf
$ cat myreg.tmf
start: q14;
final: q15;
q0: a > q1;
q1: > q5;
q2: b > q3;
q3: > q5;
q4: > q0,q2;
q5: > q9;
q6: c > q7;
q7: > q9;
q8: > q4,q6;
q9: > q13;
q10: d > q11;
q11: > q13;
q12: > q8,q10;
q13: > q12,q15;
q14: > q12,q15;
```
Using the overloaded '-cc' parameter will output the machine file in a more readable format for machines with many transitions per state and large lists of symbols.

*Note: The '-c' options do not preserve variables used in the original machine file. Instead, the individual input symbols of that variable are printed, which can significantly increase the verbosity of the output (see [spec_novar.tmf](../main/samples/spec_novars.tmf) ).*

