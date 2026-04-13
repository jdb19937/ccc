# ISO/IEC 9899:1999 — De Norma C99

## Ratio

Hoc opus in norma ISO C99 fundatum est. Omnes fontes huius repositorii normam
ISO/IEC 9899:1999 sequi **debent**. Nulla extensio compilatoris, nullum
additamentum non normativum admittendum est nisi causa gravis et documentata
exstet. Norma ipsa lex est; consuetudo compilatorum non est.

## Index Fasciculorum

Sequentes fasciculi in directorio `iso/` continentur, textum normae referentes:

| Fasciculus | Descriptio |
|---|---|
| `toc.txt` | Tabula contentorum |
| `foreword.txt` | Praefatio |
| `introduction.txt` | Introductio |
| `1_scope.txt` | Caput 1 — Ambitus |
| `2_normative_references.txt` | Caput 2 — Referentiae normativae |
| `3_terms_definitions.txt` | Caput 3 — Termini et definitiones |
| `4_conformance.txt` | Caput 4 — Conformitas |
| `5_environment.txt` | Caput 5 — Ambitus (environment) |
| `6_language.txt` | Caput 6 — Lingua |
| `7_library.txt` | Caput 7 — Bibliotheca |
| `annex_a_syntax_summary.txt` | Adnexum A — Summarium syntaxis |
| `annex_b_library_summary.txt` | Adnexum B — Summarium bibliothecae |
| `annex_c_sequence_points.txt` | Adnexum C — Puncta ordinis |
| `annex_d_universal_character_names.txt` | Adnexum D — Nomina characterum universalia |
| `annex_e_implementation_limits.txt` | Adnexum E — Limites exsecutionis |
| `annex_f_iec60559.txt` | Adnexum F — Arithmetica IEC 60559 |
| `annex_g_complex_arithmetic.txt` | Adnexum G — Arithmetica complexa |
| `annex_h_language_independent_arithmetic.txt` | Adnexum H — Arithmetica a lingua independens |
| `annex_i_common_warnings.txt` | Adnexum I — Monitiones communes |
| `annex_j_portability_issues.txt` | Adnexum J — Quaestiones portabilitatis |
| `bibliography.txt` | Bibliographia |
| `index.txt` | Index |

## Praecepta Observanda

1. **Conformitas stricta.** Codicem scribendum est qui normae C99 stricte
   conformet (§4 *Conformance*). Actio indefinita (undefined behavior) vitanda
   est omnibus modis.

2. **Nullae extensiones.** Extensiones compilatorum (`__attribute__`,
   `__builtin_*`, `typeof`, et cetera) adhibendae non sunt. Solum ea quae in
   norma definita sunt licita sunt.

3. **Portabilitas.** Codex in quolibet compilatore normae C99 conformi sine
   mutatione compilari debet. Adnexum J de quaestionibus portabilitatis
   consulendum est.

4. **Actio definita ab exsecutione.** Ubi norma actionem "implementation-defined"
   permittit, documentandum est quae assumptiones factae sint.

5. **Bibliotheca normativa.** Solum functiones in Capite 7 definitae adhibendae
   sunt. Functiones POSIX vel aliae non normativae vitandae sunt nisi necessitas
   compellat.

*Norma loquitur; nos paremus.*
