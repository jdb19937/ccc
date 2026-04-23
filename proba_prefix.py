#!/usr/bin/env python3
#
# proba_prefix.py — proba optionem -P <praefixum> in ccc et imm.
#
# Casus:
#   (1) ccc -P PFX_  positivus:
#         callee.c  → callee.o cum symbolis _PFX_foo, _PFX_bar, _PFX_glob_val
#         caller.c (vocat _PFX_*) → ligatur, currit, reddit 42
#   (2) ccc sine -P  negativus:
#         callee.o continet _foo (non _PFX_foo); ligatio cum caller defecit
#   (3) imm -P PFX_  positivus:
#         ccc -s callee.c → callee.s; imm -P PFX_ callee.s → callee.o
#         eadem exspectata ac (1)
#   (4) imm sine -P  negativus: ligatio defecit
#   (5) symbolum externum (puts): non est praefixatum in nulla forma
#

import os
import subprocess
import sys
import tempfile

CCC = os.environ.get("CCC", "./ccc")
IMM = os.environ.get("IMM", "./imm")
CC  = os.environ.get("CC",  "cc")

R = "\033[31m"; G = "\033[32m"; Y = "\033[33m"; Z = "\033[0m"

CALLEE_C = r"""
int glob_val = 100;
int foo(void) { return 40; }
int bar(int x) { return x + glob_val; }
/* vocatio ad symbolum externum: puts — NON debet praefixari */
int puts(const char *);
int usus_externi(void) { return puts(""); }
"""

CALLER_C = r"""
extern int PFX_foo(void);
extern int PFX_bar(int x);
extern int PFX_glob_val;
int main(void) {
    int a = PFX_foo();          /* 40 */
    int b = PFX_bar(2);         /* 2 + 100 = 102 */
    if (PFX_glob_val != 100) return 1;
    return a + b - 100;         /* 42 */
}
"""

def curre(argv, **kw):
    return subprocess.run(argv, stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL, **kw).returncode

def symbola(obj):
    r = subprocess.run(["nm", obj], capture_output=True, text=True)
    return r.stdout

tot = 0
fails = 0

def check(nomen, cond, detail=""):
    global tot, fails
    tot += 1
    stat = f"{G}par{Z}" if cond else f"{R}dis{Z}"
    print(f"  {nomen:55s} {stat}{('  '+detail) if detail and not cond else ''}")
    if not cond:
        fails += 1

def scribe_fontes(tmp):
    callee = os.path.join(tmp, "callee.c")
    caller = os.path.join(tmp, "caller.c")
    open(callee, "w").write(CALLEE_C)
    open(caller, "w").write(CALLER_C)
    return callee, caller

def assert_syms_exportati(label, syms, praefixatus):
    """praefixatus=True: exspecta _PFX_foo etc.; False: _foo etc."""
    if praefixatus:
        check(f"[{label}] continet _PFX_foo (T)",     " T _PFX_foo"     in syms)
        check(f"[{label}] continet _PFX_bar (T)",     " T _PFX_bar"     in syms)
        check(f"[{label}] continet _PFX_glob_val",
              (" D _PFX_glob_val" in syms) or (" S _PFX_glob_val" in syms))
        check(f"[{label}] non continet _foo nudum",   " T _foo"         not in syms)
        check(f"[{label}] non continet _bar nudum",   " T _bar"         not in syms)
        check(f"[{label}] non continet _glob_val nudum",
              (" D _glob_val" not in syms) and (" S _glob_val" not in syms))
    else:
        check(f"[{label}] continet _foo nudum",       " T _foo"         in syms)
        check(f"[{label}] continet _bar nudum",       " T _bar"         in syms)
        check(f"[{label}] continet _glob_val nudum",
              (" D _glob_val" in syms) or (" S _glob_val" in syms))
        check(f"[{label}] non continet _PFX_foo",     " T _PFX_foo"     not in syms)
    # externum puts: semper _puts (undef U), numquam praefixatum
    check(f"[{label}] _puts undef (non praefixatum)",
          (" U _puts" in syms) and ("_PFX_puts" not in syms))

def casus_ccc(tmp, praefixum):
    """compila callee cum ccc [-P PFX_]. reddit calleo.o viam."""
    callee, caller = scribe_fontes(tmp)
    callee_o = os.path.join(tmp, f"callee.ccc.{'P' if praefixum else 'N'}.o")
    args = [CCC]
    if praefixum:
        args += ["-P", "PFX_"]
    args += ["-o", callee_o, callee]
    if curre(args) != 0:
        return None, None
    caller_o = os.path.join(tmp, "caller.o")
    if not os.path.exists(caller_o):
        if curre([CCC, "-o", caller_o, caller]) != 0:
            return None, None
    return callee_o, caller_o

def casus_imm(tmp, praefixum):
    """ccc -s callee.c → .s; imm [-P PFX_] .s → .o"""
    callee, caller = scribe_fontes(tmp)
    callee_s = os.path.join(tmp, f"callee.imm.{'P' if praefixum else 'N'}.s")
    if curre([CCC, "-s", "-o", callee_s, callee]) != 0:
        return None, None
    callee_o = os.path.join(tmp, f"callee.imm.{'P' if praefixum else 'N'}.o")
    args = [IMM, callee_s, "-o", callee_o]
    if praefixum:
        args += ["-P", "PFX_"]
    if curre(args) != 0:
        return None, None
    caller_o = os.path.join(tmp, "caller.o")
    if not os.path.exists(caller_o):
        if curre([CCC, "-o", caller_o, caller]) != 0:
            return None, None
    return callee_o, caller_o

def proba_casum(label, callee_o, caller_o, praefixatus, tmp):
    if callee_o is None:
        check(f"[{label}] compilatio successit", False, "compilatio defecit")
        return
    check(f"[{label}] compilatio successit", True)
    syms = symbola(callee_o)
    assert_syms_exportati(label, syms, praefixatus)

    binar = os.path.join(tmp, f"bin.{label.replace(' ','_')}")
    link_rc = curre([CC, "-o", binar, callee_o, caller_o])
    if praefixatus:
        check(f"[{label}] ligatio successit", link_rc == 0)
        if link_rc == 0:
            rc = subprocess.run([binar]).returncode
            check(f"[{label}] programma reddit 42", rc == 42, f"rc={rc}")
    else:
        check(f"[{label}] ligatio defecit (negativa)", link_rc != 0)

def main():
    with tempfile.TemporaryDirectory() as tmp:
        print(f"{Y}=== ccc -P PFX_ (positivus) ==={Z}")
        co, cr = casus_ccc(tmp, praefixum=True)
        proba_casum("ccc +P", co, cr, True, tmp)

        print(f"{Y}=== ccc sine -P (negativus) ==={Z}")
        co, cr = casus_ccc(tmp, praefixum=False)
        proba_casum("ccc -P", co, cr, False, tmp)

        print(f"{Y}=== imm -P PFX_ (positivus) ==={Z}")
        co, cr = casus_imm(tmp, praefixum=True)
        proba_casum("imm +P", co, cr, True, tmp)

        print(f"{Y}=== imm sine -P (negativus) ==={Z}")
        co, cr = casus_imm(tmp, praefixum=False)
        proba_casum("imm -P", co, cr, False, tmp)

        print()
        if fails == 0:
            print(f"{G}OMNIA PROBATA: {tot}/{tot}{Z}")
            sys.exit(0)
        else:
            print(f"{R}PROBATIO DEFECIT: {fails}/{tot}{Z}")
            sys.exit(1)

if __name__ == "__main__":
    main()
