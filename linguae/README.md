# linguae — A Computational and Logical Linguistics Toolkit

A collection of small, self-contained C programs for exploring problems
in natural-language processing, formal semantics, and mathematical
logic. Each tool is a standalone executable that prints its results to
standard output.

Each tool is one idea; there is no shared runtime and no library
dependency beyond libc. Output is plain ASCII and can be piped through
`awk`, `grep`, or any scripting language. Each source file is short
enough to be read straight through.

The tools cluster into four overlapping themes:

1. **Text processing and corpus tools** — `agnitio`, `concordantia`,
   `declinatio`, `metrum`, `radicatio`, `palindromata`, `cifra`,
   `distantia`.
2. **Statistical generation and learning** — `confabulator`,
   `transformer`.
3. **Modal and epistemic logic** — `kripke`, `hintikka`.
4. **Foundations: classical logic, set theory, the λ-calculus** —
   `syllogismus`, `modelum`, `neumanni`, `lambda`.

The clusters overlap. A text pipeline that scans a corpus for named
entities (`agnitio`) can feed the stems it extracts (`radicatio`) into
a Markov confabulator (`confabulator`); the same concordance tool
(`concordantia`) that ranks words by frequency also computes the
contexts needed for the small transformer (`transformer`) to learn
from. On the logical side, the Kripke evaluator and the Hintikka
multi-agent extension share an evaluator pattern, and the
first-order model checker (`modelum`) and Aristotelian syllogism
verifier (`syllogismus`) both reduce to deciding membership in a
small finite structure. Section 5 works through a few of these
connections explicitly.

This README is a tutorial. It covers how to build everything (§1–3),
per-tool worked examples (§4), pipelines and connections that tie
several tools together (§5), shell recipes (§6), and exercises (§7) —
followed by reference tables (§8).

---

## 1. Building

Build every tool from the source directory:

```sh
face
```

Build a single tool:

```sh
face confabulator
```

Remove all built binaries:

```sh
face purga
```

The build uses `cc -std=c99 -Wall -Wextra -pedantic -O2`. Override `CC`
or `CFLAGS` in the environment if you need a different compiler or flag
set.

---

## 2. The Tools at a Glance

| Tool            | Domain                | Takes args? | One-line summary                                         |
|-----------------|-----------------------|-------------|----------------------------------------------------------|
| `declinatio`    | Latin morphology      | optional i  | Full nominal paradigm for the five declensions.          |
| `metrum`        | Latin prosody         | optional s  | Scans dactylic hexameter by syllable quantity.           |
| `radicatio`     | Morphology            | lang, word  | Strips endings down to a stem; Latin / en / de / es.     |
| `agnitio`       | Named-entity lookup   | text…       | Tags persons, places, times, numbers across languages.   |
| `concordantia`  | Corpus tool           | optional t  | Word frequencies and keyword-in-context concordance.     |
| `palindromata`  | Pattern matching      | optional s  | Strict, loose, and word-order palindromes.               |
| `distantia`     | Edit distance         | optional w  | Levenshtein distance; spelling suggestions.              |
| `cifra`         | Classical crypto      | alg key txt | Caesar, ROT13, Atbash, Vigenère.                         |
| `confabulator`  | Generative text       | optional    | Markov-chain text generator over a training corpus.      |
| `transformer`   | Tiny neural net       | optional    | Minimal character-level transformer trainer / sampler.   |
| `kripke`        | Modal logic           | optional φ  | Kripke-semantics evaluator for □ / ◇.                    |
| `hintikka`      | Epistemic logic       | optional φ  | Multi-agent K_a / B_a / common knowledge evaluator.      |
| `syllogismus`   | Classical logic       | optional    | Verifies all 19 valid categorical syllogisms.            |
| `modelum`       | First-order logic     | optional φ  | FOL model checker with quantifiers over a finite domain. |
| `neumanni`      | ZFC set theory        | optional n  | Builds von Neumann ordinals 0 … n, tests axioms.         |
| `lambda`        | λ-calculus            | optional e  | Untyped β-reduction with α-renaming to normal form.      |

Where "optional X" is shown, the tool takes one or more arguments on
the command line and falls back to built-in examples if none are
given.

---

## 3. Quick Start

```sh
face
./declinatio
./metrum "arma virumque cano troiae qui primus ab oris"
./cifra vigenere lemon "attack at dawn"
./lambda "(\\x.\\y.x) a b"
```

Running `./declinatio` with no argument prints full paradigms for
`rosa`, `dominus`, `bellum`, `rex`, `manus`, and `dies` — one specimen
per declension. Running `./metrum` with a Virgil line scans it
syllable-by-syllable with `—` for long and `u` for short. The
`cifra` invocation encrypts `"attack at dawn"` with a Vigenère key
of `lemon`. `./lambda` reduces a constant-function application in β.

---

## 4. Worked Examples

### 4.1  Decline a Latin noun

`declinatio` knows the five classical declensions and enough
irregularities to handle `rex → regis`. With no argument it prints
every paradigm in the table; an integer 1 … 6 picks one:

```sh
./declinatio 4            # manus, manus, manui, manum, manu, manus
```

For each specimen you get nominative through vocative, singular and
plural, laid out as a 6 × 2 table.

### 4.2  Scan a line of Virgil

`metrum` takes one argument — a line of text — and prints a quantity
pattern using classical rules: long by nature (diphthongs), long by
position (two consonants), otherwise short. It is a rough scansion
suitable for teaching; it gets roughly three quarters of Aeneid I
right and will not hide its heuristics.

```sh
./metrum "tityre tu patulae recubans sub tegmine fagi"
```

### 4.3  Reduce a word to its stem

`radicatio` is a longest-suffix stemmer with endings tables for four
languages. Pass an ISO code plus one or more words:

```sh
./radicatio la dominorum legendarum currentibus
./radicatio en running operational happiness
./radicatio de arbeitend Schoenheit Maedchen
./radicatio es corriendo hablaban naciones
./radicatio auto venti invention        # heuristic language detection
```

With `auto` (or a single argument) the tool picks whichever language's
endings table produces the longest matching suffix.

### 4.4  Tag named entities

`agnitio` tags each word of its input as `PERSONA`, `LOCUS`, `ORGAN`,
`TEMPUS`, or `NUM` — or leaves it untagged. It uses four classifiers
in priority order: small gazetteer, pure-digit number, preceding
title (`Mr.`, `Dr.`, `Herr`…), and preceding preposition
(`in`, `ad`, `nach`…).

```sh
./agnitio "Cicero ad Romam cum Augusto proficiscitur in ides Martii"
./agnitio "Mr. Shakespeare wrote plays in London during 1600"
```

The first prints `Cicero / PERSONA`, `Romam / LOCUS`, `ides / TEMPUS`;
the second tags Shakespeare (title-preceded), London
(preposition-preceded), and 1600 (numeric).

### 4.5  Build a concordance

`concordantia` builds a hash table of word frequencies, sorts by
count, and prints a keyword-in-context line (KWIC) for the most
alphabetically early words. With no arguments it uses a short passage
from Caesar's *Bellum Gallicum* and a fox pangram.

```sh
./concordantia
./concordantia "$(cat < /path/to/your/text.txt)"
```

The table lets you immediately see the Zipf-like distribution of a
natural-language sample. The KWIC pane shows the first occurrence of
each term in its surrounding 40 characters.

### 4.6  Palindromes, three ways

`palindromata` checks a string in three modes:
- **strictus** — raw byte reversal, case-sensitive;
- **mollis** — ignores case, spaces, and punctuation;
- **verborum** — splits into words, then tests whether the word
  sequence is its own reverse.

```sh
./palindromata "A man a plan a canal Panama"
./palindromata "first ladies rule the state and state the rule ladies first"
```

The first is a classic mollis-palindrome; the second is a word-order
palindrome but not a character palindrome.

### 4.7  Edit distance and spell-check

`distantia` computes Levenshtein distance between two words or, if
given only one, searches a small mixed-language dictionary for the
closest matches:

```sh
./distantia kitten sitting          # distance 3 + operations
./distantia rossa                   # suggestions: rosa (1), casa (2) …
```

The operations printout uses `=x` for match, `~y` for substitution,
`+y` for insertion, `-x` for deletion.

### 4.8  Encrypt and decrypt

`cifra` implements four classical ciphers. All are self-inverse or
numerically reversible:

```sh
./cifra caesar 3 "venti vidi vinci"
./cifra rot13 "ars longa vita brevis"
./cifra atbash "Caesar Augustus"
./cifra vigenere lemon "ATTACK AT DAWN"
```

With no arguments the tool runs a demonstration that encrypts and then
decrypts each example, so you can read the round-trip.

### 4.9  Generate Markov text

`confabulator` reads a corpus (its built-in Latin or English sample
by default, or one you supply with `-c`) and emits a sequence drawn
from the first-order word-transition distribution:

```sh
./confabulator                          # Virgil + KJV stubs
./confabulator -n 100 -i arma           # Latin, starting from "arma"
./confabulator -c "all work and no play makes jack a dull boy" -n 30
```

Pass `-s` to fix the random seed; otherwise it is taken from
`time(NULL)`.

### 4.10  Train a tiny transformer

`transformer` is a toy character-level model with 8-dimensional
embeddings, a single uniform-attention head, a linear classifier over
30 characters (a–z + space + `.` + `,` + newline), and SGD training.
It is too small to produce fluent output, but it is big enough to
demonstrate the core loop of training and sampling:

```sh
./transformer                            # default corpus, 500 steps
./transformer -i 5000 -n 400             # more training, more output
./transformer -p "in principio" -s 42    # fixed prompt and seed
```

Expect Zipfian nonsense; the goal is the architecture, not fluency.

### 4.11  Evaluate a Kripke formula

`kripke` encodes a three-world frame (present; possible rainy future;
possible sunny future) and evaluates modal formulas written in a
prefix Polish notation: `p` atom, `!` not, `&` and, `|` or, `>`
implies, `[` box, `<` diamond.

```sh
./kripke '[p'        # necessarily p
./kripke '<p'        # possibly p
./kripke '[>pu'      # necessarily: rain implies open umbrella
```

Each formula is checked at every world of the frame, and the tool
prints `T` or `F` for each.

### 4.12  Multi-agent knowledge

`hintikka` extends Kripke to three agents (Socrates, Plato,
Aristotle). The built-in frame encodes a scenario where Socrates
cannot distinguish two worlds and the other two can; formulas use
`K<n>` for "agent n knows" and `C` for common knowledge:

```sh
./hintikka 'K0p'         # Socrates knows p — false
./hintikka 'K1p'         # Plato knows p    — true in one world
./hintikka 'K1!K0p'      # Plato knows that Socrates does not know p
./hintikka 'Cp'          # common knowledge p
```

### 4.13  Check a syllogism

`syllogismus` verifies an Aristotelian syllogism against the four
traditional rules (middle term distributed, illicit process, no
two negative premises, negativity propagation).

```sh
./syllogismus A A A 1    # Barbara, valid
./syllogismus E A I 1    # EAI-1, invalid
```

With no arguments the tool walks through all nineteen traditional
valid moods — Barbara, Celarent, Darii, Ferio, … plus a few invalid
controls — and reports per-rule diagnostics.

### 4.14  First-order model checking

`modelum` encodes the four-element domain {0, 1, 2, 3} with
predicates `P` (even), `L` (less than), `E` (equal). Write a formula
in prefix: `Ex` for ∃x, `Ax` for ∀x, relations as uppercase with
arguments immediately after.

```sh
./modelum 'AxExLxy'      # ∀x ∃y  x < y       — true
./modelum 'ExAyLxy'      # ∃x ∀y  x < y       — false (no minimum)
```

### 4.15  Von Neumann ordinals

`neumanni` builds ordinals as sets: 0 = ∅, n + 1 = n ∪ {n}. It prints
the set expansion of each ordinal up to n, verifies
transitivity, and demonstrates the axioms of extensionality, pairing,
and union on small finite examples:

```sh
./neumanni 4
```

### 4.16  λ-reduce an expression

`lambda` parses an untyped λ-expression and performs β-reduction in
normal order, with α-renaming to avoid variable capture. It prints
every intermediate term:

```sh
./lambda '(\x.\y.x) a b'                   # K combinator
./lambda '(\f.\x.f (f x)) (\n.n) y'        # Church 2 applied to id
./lambda '(\n.\f.\x.f (n f x)) (\f.\x.x)'  # succ 0 = 1
```

---

## 5. Connections Between the Tools

A few chains that come up in practice.

**Stemming before NER.** Normalising inflection before looking up
a gazetteer catches variants `radicatio` would also find. For a Latin
corpus, pipe each token through `./radicatio la $word` and then match
on the stem:

```sh
echo 'Romam et Athenas vidit' | tr ' ' '\n' | while read w; do
    ./radicatio la "$w" 2>/dev/null | tail -1
done
```

**Frequency × distance = suggestion.** `concordantia` gives you the
words a text actually uses. Feeding those through `distantia` against
a typo lets you weight by in-corpus frequency — a crude Bayesian
spell-checker in two pipes.

**Kripke → Hintikka.** The Kripke evaluator is the single-agent case
of `hintikka` with one relation; formulas valid in one directly
correspond to formulas with `K0` in the other. Modifying `kripke.c` to
vary the accessibility relation (reflexive, symmetric, transitive)
gives you the families S4, S5, KD45, etc.

**Church numerals connect λ to ordinals.** The Church encoding
computes n + 1 as `(\n.\f.\x.f (n f x))`, which the `lambda` tool
will reduce symbolically. The von Neumann successor computed by
`neumanni` does the same job extensionally, on actual sets. They
meet, roughly, at category-theoretic *natural number object*.

**Syllogisms are model checks.** Every valid Aristotelian syllogism
is a formula of `modelum` restricted to monadic predicates. Barbara,
for instance, is `Ax(>PxQx) & Ax(>QxRx) > Ax(>PxRx)`. You can
transcribe the nineteen moods and verify them in `modelum` directly.

**Confabulator → Transformer.** `confabulator` is an order-1 Markov
chain over words; `transformer` is a neural n-gram over characters.
The confabulator's perplexity on held-out text is a natural baseline
to beat. Training `transformer` on the same corpus and comparing
output on a fixed prompt is a simple ablation.

---

## 6. Shell Recipes

Count the top ten words in an arbitrary file:

```sh
./concordantia "$(cat yourfile.txt)" | head -14 | tail -10
```

Batch-stem a word list:

```sh
xargs -n1 ./radicatio la < verbalist.txt
```

Encrypt a file with Vigenère and pipe to disk:

```sh
./cifra vigenere SECRET "$(cat plaintext.txt)" > ciphertext.txt
```

Scan every line of a Latin corpus:

```sh
while IFS= read -r line; do ./metrum "$line"; done < aeneid.txt
```

Spell-check against the built-in mixed dictionary:

```sh
for word in apple banana rossa; do ./distantia "$word" | head -3; done
```

Quick modal tautology search (random frames + `./kripke` — an
exercise).

---

## 7. Exercises

1. Add a sixth declension to `declinatio.c` for Greek loan words like
   `thema`, *thema*, *thematis* (3rd-declension neuter).

2. Extend `metrum.c` to recognise elision: `multum ille` → `mult(um)
   ille` with the elided syllable dropped.

3. Add stem tables for a fifth language (Italian, French, Russian) to
   `radicatio.c`.

4. Increase `D_MODEL` and `CTX_LON` in `transformer.c`, add a second
   attention head, and plot log-loss during training.

5. In `syllogismus.c`, add the fifth traditional rule
   ("conclusion does not follow from two universals") and see which
   additional moods become invalid.

6. Replace the uniform attention in `transformer.c` with dot-product
   attention (`softmax(QKᵀ/√d) V`) and compare the change in
   generation quality at fixed training steps.

7. Write a `turing.c` that encodes a small Turing machine and
   simulates it — then connect it to `lambda.c` via Turing-equivalence
   arguments.

8. Build a KWIC concordance of the output of `confabulator`
   (`./confabulator -n 2000 | ./concordantia`) and discuss the
   distribution.

9. Make `hintikka.c` parametric in the number of agents and show
   that for n ≥ 3 the theory ceases to be compact — i.e. find a set
   of formulas whose every finite subset is satisfiable but the whole
   set is not.

---

## 8. Reference

**Source-line counts** (roughly):

```
  agnitio.c       ~310   concordantia.c  ~260
  cifra.c         ~240   confabulator.c  ~220
  declinatio.c    ~190   distantia.c     ~240
  hintikka.c      ~290   kripke.c        ~260
  lambda.c        ~320   metrum.c        ~240
  modelum.c       ~270   neumanni.c      ~210
  palindromata.c  ~220   radicatio.c     ~250
  syllogismus.c   ~320   transformer.c   ~310
```

**Further reading.**

- *Handbook of Modal Logic*, Blackburn, van Benthem, Wolter (2007).
- *Reasoning About Knowledge*, Fagin, Halpern, Moses, Vardi (1995).
- *Introduction to Mathematical Logic*, Mendelson (1997).
- *Set Theory*, Jech (2003) — for von Neumann ordinals and ZFC.
- *The Lambda Calculus*, Barendregt (1984).
- *Speech and Language Processing*, Jurafsky & Martin (3rd ed.) — for
  stemming, NER, edit distance, and the Markov / transformer chapters.
- Aristotle, *Prior Analytics*, Book I.

---

Written to be read. Everything here is short, Latin in its naming,
and independent. Pick a tool and start modifying.
