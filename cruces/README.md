# cr — A Numerical and Topological Toolkit

A collection of small, self-contained C programs for exploring classic
problems in number theory, combinatorics, and low-dimensional topology.
Each tool is a standalone executable that prints its results to standard
output.

Each tool is one idea; there is no shared runtime and no library
dependency beyond libc. Output is plain ASCII and can be piped through
`awk`, `grep`, or any scripting language. Each source file is under
400 lines and can be read straight through.

Mathematically the tools cluster into three overlapping themes:

1. **Prime and multiplicative number theory** — `primi`, `goldbachiana`,
   `mersenniani`, `divisores`, `moebiana`, `totiens`, `perfecti`.
2. **Modular and Diophantine arithmetic** — `residua`, `continuae`,
   `matrices`, `coniectura`.
3. **Combinatorics and low-dimensional topology** — `polyhedra`,
   `euleri`, `superficies`, `simplicium`, `connexio`.

The clusters overlap. Many of the deeper problems draw on two of
them: the Euclid–Euler theorem joins Mersenne primes with divisor
sums; Pell's equation joins Diophantine approximation with modular
arithmetic; the invariance of χ joins polyhedral combinatorics with
simplicial topology. Sections 5 and 7 treat these connections
explicitly.

This README is a tutorial. It covers how to build everything (§1–3),
per-tool worked examples (§4), deeper theorems that tie several tools
together (§5), shell recipes (§6), and exercises (§7) — followed by
reference tables (§8–9).

---

## 1. Building

Build every tool from the source directory:

```sh
face
```

Build a single tool:

```sh
face primi
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

| Tool           | Domain               | Takes args? | One-line summary                                        |
|----------------|----------------------|-------------|---------------------------------------------------------|
| `primi`        | Number theory        | optional N  | Sieve primes below N; list twin prime pairs.            |
| `goldbachiana` | Number theory        | optional N  | Verify Goldbach's conjecture; print largest prime gaps. |
| `mersenniani`  | Number theory        | optional p  | Mersenne primes via Lucas-Lehmer up to exponent p.      |
| `perfecti`     | Number theory        | optional N  | Perfect, abundant, deficient numbers; amicable pairs.   |
| `divisores`    | Number theory        | optional N  | Divisor-sum σₖ(n), τ(n), Dirichlet convolution.         |
| `moebiana`     | Number theory        | optional N  | Möbius function μ(n); counts of squarefree numbers.     |
| `totiens`      | Number theory        | optional N  | Euler totient φ(n); verifies Σ_{d∣n} φ(d) = n.          |
| `residua`      | Modular arithmetic   | —           | Chinese Remainder Theorem solver.                       |
| `coniectura`   | Dynamical systems    | optional N  | Collatz 3n+1 records of length and height.              |
| `matrices`     | Linear algebra       | optional N  | Fibonacci F(n) mod 10⁹+7 via matrix exponentiation.     |
| `continuae`    | Diophantine approx.  | —           | Continued fraction expansions of √2 … √31.              |
| `connexio`     | Graph theory         | —           | Connectivity, components, bridges, articulation points. |
| `polyhedra`    | Geometry             | —           | Euler's formula V − E + F = 2 on the Platonic solids.   |
| `euleri`       | Topology             | —           | Euler characteristic and genus from triangulations.     |
| `superficies`  | Topology             | —           | Classifies closed surfaces by genus and orientability.  |
| `simplicium`   | Algebraic topology   | —           | f-vectors and χ of simplicial complexes.                |

Where "optional N" is shown, the tool takes a single integer on the
command line and uses a compiled-in default if no argument is given.

---

## 3. Quick Start

```sh
face
./primi 100
./coniectura 50
./totiens 30
```

The first run sieves primes below 100, the second computes Collatz
sequence records up to n = 50, the third prints the totient φ(n) for
1 ≤ n ≤ 30.

For example, `./primi 30` produces:

```
Magnitudo cribri: 31 octeti

Numeri primi usque ad 30:
     2     3     5     7    11    13    17    19    23    29
Numerositas primorum: 10

Quadrata primorum usque ad 30:
  2^2 = 4  3^2 = 9  5^2 = 25  7^2 = 49  11^2 = 121
  13^2 = 169  17^2 = 289  19^2 = 361  23^2 = 529  29^2 = 841

Magnitudo VLA booliani: 31 octeti (31 elementa)
Primi gemelli usque ad 30:
  (3, 5)  (5, 7)  (11, 13)  (17, 19)
Numerositas parium gemellorum: 4
```

Four labeled blocks: sieve byte count, prime list with count, squares
of those primes, twin prime pairs. All tools use the same format of
labeled plain-text blocks.

**Rebuilding.** After editing a `.c` file, rerun `face <name>` before
re-executing. `face` rebuilds only when the source is newer than the
binary; otherwise it returns the previous result.

---

## 4. Worked Example Problems

Each example is a short, real problem you can attack end-to-end with the
toolkit. Try them in order — they build in complexity.

Some tools take a limit on the command line. Others have their input
compiled in as a small table near the top of the source; for those,
the workflow is: edit the file, rebuild with `face <toolname>`, and
run.

### 4.1  How many twin primes are there below ten thousand?

`primi` sieves primes and also reports twin prime pairs (p, p+2):

```sh
./primi 10000
```

The last line is the count of twin prime pairs in the range. For comparison,
π(10000) = 1229 primes, and the number of twin prime pairs below 10⁴ is 205.

### 4.2  Does Goldbach's conjecture hold up to 100,000?

Every even integer greater than 2 can, conjecturally, be written as a
sum of two primes. Verify it:

```sh
./goldbachiana 100000
```

The program will terminate with a failure message if it finds a
counterexample. (It will not.) It also prints the largest prime gaps it
encountered, which is useful for a second, related investigation:

### 4.3  Where are the largest prime gaps below one million?

Run `goldbachiana` with a high limit and read its prime-gap report. For
example, it exhibits a gap of 148 between 20831323 and 20831471.

### 4.4  Find all Mersenne primes with exponent ≤ 31

```sh
./mersenniani 31
```

The output lists, for each prime exponent p ≤ 31, whether 2ᵖ − 1 is
prime, and (by Euclid–Euler) the corresponding even perfect number
2^(p−1)(2ᵖ − 1). You should see the first eight Mersenne primes:
p = 2, 3, 5, 7, 13, 17, 19, 31.

### 4.5  Are 220 and 284 amicable?

Two numbers are amicable if each equals the sum of the other's proper
divisors. `perfecti` finds both perfect numbers and amicable pairs:

```sh
./perfecti 10000
```

Look for `(220, 284)` and `(1184, 1210)` in the amicable-pairs list.

### 4.6  Solve "what number leaves remainders 2, 3, 2 when divided by 3, 5, 7?"

This is Sunzi's classical problem. `residua` solves it (and a larger
modern system) via the Chinese Remainder Theorem:

```sh
./residua
```

The output prints the systems of congruences and the least non-negative
solution (23 for Sunzi's problem).

**Solving your own congruence system.** Open `residua.c` and find the
two systems defined near the top — each one is a short array of
`{remainder, modulus}` pairs with a length. Replace one of them with
your pairs, adjust the length, rebuild with `face residua`, and run.
For `x ≡ 1 (mod 4), x ≡ 2 (mod 9), x ≡ 3 (mod 25)`, the array becomes
`{{1,4}, {2,9}, {3,25}}` of length 3.

### 4.7  What is the 10,000th Fibonacci number mod 10⁹ + 7?

Naïve recursion is hopeless at that scale. `matrices` raises the 2×2
Fibonacci matrix to the n-th power in O(log n) multiplications:

```sh
./matrices 10000
```

It prints F(n) for n up to the limit and cross-checks the identity
F(m + n) = F(m)F(n+1) + F(m−1)F(n) as a correctness test.

### 4.8  What is the continued fraction expansion of √7?

`continuae` prints the periodic expansion and the convergents
pₖ/qₖ for each of √2 … √31:

```sh
./continuae | less
```

Look for the line beginning `sqrt(7) =`. The convergents are the
best rational approximations in lowest terms; the last one the program
prints is accurate to many decimal places.

**Going past 31.** The upper bound is a single constant in
`continuae.c`. Raise it and rebuild to obtain expansions for every
non-square radicand in the new range.

### 4.9  Verify Euler's identity Σ_{d∣n} φ(d) = n

`totiens` computes the Euler totient two ways and verifies Gauss's
divisor-sum identity:

```sh
./totiens 1000
```

It will terminate with an error if the identity ever fails.

### 4.10  Count squarefree integers up to ten thousand

An integer is squarefree if no prime's square divides it. The Möbius
function μ(n) is nonzero exactly on squarefree n:

```sh
./moebiana 10000
```

The density of squarefree integers tends to 6/π² ≈ 0.6079. The count
printed by `moebiana` divided by N should be close to this.

### 4.11  Is the Petersen graph connected?

`connexio` analyzes three built-in graphs — the Petersen graph, a
disconnected graph, and a tree — reporting connected components,
bridges, and articulation points:

```sh
./connexio
```

**Using your own graph.** Each graph in `connexio.c` is given as a
vertex count plus an array of `{u, v}` edges. Copy one of the three
existing blocks, rename it, substitute your edge list, and add a call
to the analysis in `main`. Rebuild with `face connexio`.

### 4.12  Verify Euler's formula for the dodecahedron

```sh
./polyhedra
```

Prints V, E, F, and V − E + F for each of the five Platonic solids.
All five should give 2.

**Checking a non-Platonic polytope.** Each solid in `polyhedra.c` is a
single struct literal with `{ .nomen = "...", .V = …, .E = …, .F = … }`.
Add a new entry to the array — e.g. a truncated tetrahedron with
V = 12, E = 18, F = 8 — and rebuild. V − E + F should still be 2 for
any convex polyhedron.

### 4.13  What is the genus of a double torus?

`superficies` and `euleri` between them give the Euler characteristic
and genus of each closed surface in their built-in catalog, using the
relation χ = 2 − 2g for orientable surfaces and χ = 2 − k for
non-orientable ones:

```sh
./superficies
./euleri
```

**Adding a surface you care about.** Both files keep their catalog as
an array of named surfaces with V, E, F counts (or genus and
orientability flags). To check a triple torus, append an entry with
genus 3 to the table in `superficies.c`; for a triangulation-based
check in `euleri.c`, append one with the corresponding V, E, F
(e.g. V = 14, E = 42, F = 28 gives χ = 0 — a torus). Rebuild with
`face superficies` or `face euleri`.

### 4.14  Compute the f-vector and χ of a tetrahedral boundary

`simplicium` builds the boundary of a tetrahedron and an octahedron as
simplicial complexes, then reads off the f-vector
(f₀ = vertices, f₁ = edges, f₂ = faces, …) and the alternating sum χ:

```sh
./simplicium
```

The tetrahedral boundary gives f = (4, 6, 4), χ = 2, matching the
sphere S².

**Building your own complex.** The two complexes in `simplicium.c` are
constructed by calling the variadic "add simplex" helper once per
top-dimensional face, listing the vertex indices. To build, say, the
boundary of a 4-simplex (five tetrahedra meeting on a common vertex
set of size 5), replicate the pattern: five calls, each listing four
of the five vertices. χ will come out as 2 (a 3-sphere) — a nice
sanity check before rebuilding with `face simplicium`.

---

## 5. Threads Through the Toolkit

The worked examples in §4 each use a single tool. This section
explains four theorems whose verification pulls several tools together
at once. They're worth walking through before attempting the exercises
in §7.

**Notation.** Glossary of the symbols used below and in §7.

| Symbol        | Name                   | Meaning                                                  |
|---------------|------------------------|----------------------------------------------------------|
| τ(n)          | divisor count          | number of positive divisors of n                         |
| σ(n)          | divisor sum            | sum of positive divisors of n (includes n itself)        |
| σₖ(n)         | k-th divisor power sum | Σ_{d∣n} dᵏ                                               |
| φ(n)          | Euler totient          | count of integers in [1, n] coprime to n                 |
| μ(n)          | Möbius function        | 0 if p²∣n for some prime p; (−1)^k if squarefree with k prime factors; 1 if n = 1 |
| π(N)          | prime counting         | number of primes ≤ N                                     |
| ζ(s)          | Riemann zeta           | Σ_{n≥1} 1/nˢ (for ℜ(s) > 1)                              |
| χ             | Euler characteristic   | V − E + F for polyhedra; Σ_i (−1)^i f_i in general       |
| f_i           | i-th face count        | number of i-dimensional faces of a simplicial complex    |
| g             | genus                  | for closed orientable surfaces, χ = 2 − 2g               |
| n ≡ r (mod m) | congruence             | m divides n − r                                          |
| f ∗ g         | Dirichlet convolution  | (f ∗ g)(n) = Σ_{d∣n} f(d)·g(n/d)                         |

### 5.1  The Euclid–Euler theorem

Euclid, *Elements* IX.36: if 2ᵖ − 1 is prime, then 2^(p−1)·(2ᵖ − 1) is
a perfect number. Euler (two millennia later) proved the converse for
even perfect numbers: every even perfect number has that form. The
odd case is open; no odd perfect number has ever been found, but
neither has a proof that none exists.

Two tools bear on this directly. `mersenniani` finds the Mersenne
primes 2ᵖ − 1; `perfecti` finds perfect numbers by summing proper
divisors. Run both with matching limits: the p-th Mersenne prime from
`mersenniani` yields, after Euclid's multiplication, the p-th perfect
number reported by `perfecti`. Disagreement below any finite bound
would imply either an implementation error or an odd perfect number.

### 5.2  Multiplicative functions and Möbius inversion

Three arithmetic functions sit at the heart of classical number theory:

- τ(n) = number of divisors of n
- σ(n) = sum of divisors of n
- φ(n) = count of integers in [1, n] coprime to n
- μ(n) = Möbius function: 0 if n has a squared prime factor,
  (−1)^k if n is a product of k distinct primes, 1 if n = 1

Three identities connect them, all verifiable with the toolkit:

| Identity                              | Tools to use            |
|---------------------------------------|-------------------------|
| Σ_{d∣n} φ(d) = n                      | `totiens`               |
| φ(n) = Σ_{d∣n} μ(d)·(n/d)             | `totiens`, `moebiana`   |
| σ = id ∗ 1    (Dirichlet convolution) | `divisores`             |

`divisores` checks the third internally. The first two can be
verified by a short script that parses the output of `totiens` and
`moebiana` term by term.

A fourth identity is deeper: the density of squarefree integers in
[1, N] tends to 6/π² = 1/ζ(2). The output of `moebiana` therefore
furnishes a (slowly convergent) numerical estimate of π; see
Exercise 7.2.

### 5.3  Continued fractions, Pell's equation, and matrix exponentiation

Lagrange showed that √D for non-square D has a continued fraction
expansion that is eventually periodic:
√D = [a₀; a₁, a₂, …, aₖ] with the bar over the period. The
convergents pₖ/qₖ generated along the way have a remarkable property:
the convergent just before the period closes gives the fundamental
solution (p, q) to Pell's equation x² − Dy² = ±1. All other solutions
are obtained by taking powers of (p + q√D) in the ring ℤ[√D].

The toolkit covers the whole chain:

- `continuae` prints the continued fraction expansion and the
  convergents for √2 … √31.
- Matrix exponentiation (as implemented in `matrices` for Fibonacci)
  is the algorithmically efficient way to compute powers of (p + q√D),
  i.e., to generate the n-th Pell solution in O(log n) time. The
  recurrence matrix is different but the shape of the code is the
  same.
- `residua` solves the CRT step you need if you want to hybridize the
  Pell recurrence with a modular reduction — e.g., "find the smallest
  Pell solution for D = 61 that is ≡ r (mod m)".

### 5.4  The Euler characteristic as a topological invariant

Euler's polyhedron formula V − E + F = 2 for convex polyhedra is a
special case of a general invariant:
χ = Σ_i (−1)^i · f_i, the alternating sum of face counts of a
simplicial complex, where f_i is the number of i-dimensional faces.
Poincaré showed χ is a topological invariant: any two triangulations
of homeomorphic spaces yield the same χ.

Four tools compute it:

- `polyhedra` computes χ from combinatorial data on convex solids.
- `simplicium` computes χ from the f-vector of a general simplicial
  complex.
- `euleri` computes χ and genus from explicit triangulations of
  surfaces.
- `superficies` tabulates χ and genus for the standard classified
  closed surfaces (sphere, torus, genus-g orientable, projective
  plane, Klein bottle, non-orientable genus-k).

These are four independent computations of the same invariant on the
same families of spaces. Exercise 7.5 makes the cross-check explicit.

---

## 6. Recipes: Combining Tools

The tools are deliberately small and single-purpose, so they combine
well in pipelines.

**Show only even perfect numbers up to 10⁸:**

```sh
./perfecti 100000000 | grep -i perfect
```

**Compare the first few twin primes against the first few Mersenne exponents:**

```sh
./primi 200       > /tmp/primi.txt
./mersenniani 61  > /tmp/mersenne.txt
diff -y /tmp/primi.txt /tmp/mersenne.txt | less
```

**Sanity-check a conjecture of your own.** If you conjecture that every
odd number n ≥ 3 appears as a prime gap somewhere below 10⁸, run
`goldbachiana 100000000`, save the gap column, and take the set
difference against 3, 5, 7, …

---

## 7. Exercises

Seven problems to work through. Each deliberately needs more than one
tool. They progress roughly from concrete to abstract; none requires
advanced techniques, but several reward careful bookkeeping. The last
is a set of open problems, included for scope rather than solution.
Difficulty bars and hints follow each prompt.

### Exercise 7.1  Euclid–Euler correspondence  (★☆☆)

> Verify, for every exponent p ≤ 31, that 2^(p−1)·(2ᵖ − 1) is a
> perfect number **iff** 2ᵖ − 1 is prime. Do not use the same tool for
> both directions of the "iff".

*Approach.* Run `mersenniani 31`, collect the p for which 2ᵖ − 1 is
prime, and manually compute the associated even perfect number.
Independently run `perfecti 2300000000` and read off the perfect
numbers it finds. The two lists should match exactly.

*Extension.* The largest perfect number `perfecti` can reach in
reasonable time is bounded by its inner divisor loop. Where does the
wall hit, and which exponent p produces a perfect number just beyond
it? (`mersenniani` sees this p easily; `perfecti` does not.)

### Exercise 7.2  Estimate π from the density of squarefree integers  (★★☆)

> The density of squarefree integers in [1, N] tends to 6/π². Use
> `moebiana` to estimate π to three decimal places. Report the error
> at N = 10³, 10⁴, 10⁵ and fit the error to a function of N.

*Approach.* `moebiana` prints the count of squarefree integers
below N. Call this Q(N). Then π ≈ √(6N/Q(N)). You may need to raise
the limit cap in `moebiana.c` (a single constant) to push N past the
default; it's a one-line edit.

*Convergence rate.* The error in Q(N)/N − 6/π² is O(1/√N)
unconditionally and O(N^(−1/2+ε)) under the Riemann Hypothesis; many
orders of magnitude of N are required to gain one decimal of π. The
rate of convergence is the content of the exercise.

### Exercise 7.3  Fundamental solution to Pell's equation  (★★☆)

> For each of D = 2, 7, 13, 29, 61, find the smallest positive
> integers (x, y) such that x² − D·y² = 1. Then find the 5th solution
> by iterating (x + y√D)ⁿ in the ring ℤ[√D].

*Approach.* `continuae` prints the convergents of √D up to the end of
the first period. The last convergent in the period gives either a
solution of x² − Dy² = 1 or x² − Dy² = −1, depending on the parity of
the period length. If you get the (−1) case, square the solution.
Iterating: (x + y√D)(a + b√D) = (xa + Dyb) + (xb + ya)√D — a simple
two-integer recurrence.

*Why D = 61.* Fermat posed this case as a challenge to English
mathematicians. The fundamental solution is (1766319049, 226153980).
If `continuae` does not yet cover D = 61, raise its upper bound (see
§4.8); the convergents will eventually reach that pair.

### Exercise 7.4  Möbius inversion, end to end  (★★☆)

> Verify both of the following identities numerically for every
> n ≤ 100:
>
> 1. Σ_{d∣n} φ(d) = n
> 2. φ(n) = Σ_{d∣n} μ(d)·(n/d)
>
> Then explain, in one sentence each, how the second identity implies
> the first by a change of summation.

*Approach.* Run `totiens 100`, `moebiana 100`, and `divisores 100`
(the last supplies the divisor lists implicitly through σ and τ, but
you may want to produce divisors directly — see the `cerne_cribrum`
helper in `primi.c` for the divisor-by-sieve pattern). Pipe outputs
through `awk` or a short Python script to verify the sums term by
term. `totiens` already checks (1) internally and will exit nonzero
on a violation, which is a useful control.

*The inversion.* Both identities are instances of Möbius inversion:
μ and the constant function 1 are mutually inverse under Dirichlet
convolution, so any identity of the form f = g ∗ 1 is equivalent to
g = f ∗ μ.

### Exercise 7.5  χ is a topological invariant  (★★★)

> Produce four independent computations of the Euler characteristic
> of the sphere S² and the torus T², using **all four** of
> `polyhedra`, `euleri`, `simplicium`, `superficies`. Confirm that
> each family of computations gives χ(S²) = 2 and χ(T²) = 0, and
> explain why agreement across different triangulations is not a
> coincidence.

*Approach.*

- `polyhedra` — all five Platonic solids are polyhedral realizations
  of S². Any of the five gives χ = 2.
- `simplicium` — the tetrahedral boundary is already in the source
  as a simplicial model of S². Add a second model: a minimal
  7-vertex triangulation of T² (Möbius's torus), or an 8-vertex
  octahedral boundary as an alternate model of S². Compare the
  f-vectors; confirm χ is unchanged.
- `euleri` — triangulate a surface explicitly by V, E, F counts and
  let the tool compute χ = V − E + F and the derived genus.
- `superficies` — read off the invariant from the classification
  table.

*Why agreement is forced.* χ is a homotopy invariant: any two
simplicial complexes that are homotopy-equivalent — in particular,
any two triangulations of the same manifold — share the same χ. This
holds purely by simplicial bookkeeping, prior to any use of homology.
Poincaré identified the alternating sum with
rk H₀ − rk H₁ + rk H₂ − …, which explains the invariance.

### Exercise 7.6  Collatz as a graph  (★★☆)

> Fix N = 50. Form an undirected graph on the vertex set {2, 3, …, N}
> with an edge {n, T(n)} whenever both n and T(n) lie in the set,
> where T is the Collatz map. Determine the number of connected
> components, and verify that every edge is a bridge.

*Approach.* Run `coniectura 50` to see every trajectory terminate at
1. Extract the edges by iterating the Collatz map yourself (a dozen
lines of `awk` or Python against the tool's trajectory output, or a
one-line loop in a shell). Paste the resulting edge list as a new
graph block in `connexio.c` — copy the tree example already in the
source and replace its edges with yours. Rebuild with
`face connexio` and run.

*What to expect.* The graph is a forest: each vertex has exactly one
outgoing Collatz step, so the underlying undirected graph contains no
cycles. `connexio` will report every edge as a bridge and give a
component count equal to |V| − |E|. Vertices whose Collatz successor
exceeds 50 serve as roots of their component.

*Extension.* Repeat for N = 100 and N = 200, and describe how the
component count varies with N.

### Exercise 7.7  Open problems  (★★★★)

Several tools bear on classical open problems. The items below are
experimental — verification rather than proof — but each can be
pushed to the limits of available computation.

- **Odd perfect numbers.** None is known; none has been proved
  impossible. `perfecti` checks candidates by divisor sum. Push the
  upper limit and record the cost per decade of N.
- **The Collatz conjecture.** Verified to approximately 2⁶⁸;
  unproven. `coniectura` covers a small initial segment. The
  empirical questions concern the growth of the maximum trajectory
  length and peak value as functions of N. Conjectures of Terras and
  others predict specific rates.
- **Twin prime constant.** The Hardy–Littlewood conjecture predicts
  π₂(N) ∼ 2·C₂·N/(ln N)² with C₂ ≈ 0.6601618. Use the twin prime
  counts from `primi` to estimate C₂ and examine the rate of
  convergence.
- **Goldbach's conjecture.** Numerically verified to 4·10¹⁸
  (Oliveira e Silva et al.); unproven. A secondary question is the
  growth of the number of representations of 2n as p + q — the
  Goldbach comet.

---

## 8. Argument and Limit Reference

| Tool           | Default N | Max N      | Fails if                            |
|----------------|-----------|------------|-------------------------------------|
| `primi`        | 1000      | 50000      | N < 2 or non-integer                |
| `coniectura`   | 1000      | 10000000   | N < 2 or non-integer                |
| `goldbachiana` | 1000      | 100000     | N < 4 or odd                        |
| `mersenniani`  | 31        | 61         | exponent bound outside [2, 61]      |
| `perfecti`     | 10000     | 1000000    | N < 1                               |
| `divisores`    | 300       | 300        | N < 1                               |
| `moebiana`     | 1000      | 10000      | N < 1                               |
| `totiens`      | 1000      | 10000      | N < 1                               |
| `matrices`     | 100       | 1000000    | N < 1                               |

All tools print a diagnostic to stderr and exit with status 1 on bad
input.

---

## 9. Exit Codes

- **0** — success; results printed to stdout.
- **1** — invalid command-line argument, or (in the verifier tools) a
  mathematical invariant failed — look at stderr to distinguish the
  two.

---

## 10. Where to Go Next

- Read the header comment of each `*.c` file for a statement of the
  mathematical problem it addresses.
- Modify the built-in data (graphs in `connexio.c`, surfaces in
  `superficies.c`, congruence systems in `residua.c`) and rebuild
  with `face <name>`.
- Pipe the output of one tool into a script to produce input for
  another. The plain-text format is easy to parse.
