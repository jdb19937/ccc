#!/usr/bin/env python3
#
# proba_imm.py — pro quoque casu (subdirectorio cum .c plicis) generat
# .s per `cc -S`, deinde assemblat .s duobus modis: per `cc -c` (referentia)
# et per `imm` (probandus). Coniungit utramque seriem .o per `cc`, currit,
# comparat exitus et output.
#

import glob
import os
import shutil
import subprocess
import sys
import tempfile

IMM = os.environ.get("IMM", "./imm")
CC  = os.environ.get("CC",  "cc")
if len(sys.argv) != 2:
    sys.stderr.write(f"usus: {sys.argv[0]} <directorium>\n")
    sys.exit(2)
DIR = sys.argv[1]
TIMEOUT = 5

R = "\033[31m"; Y = "\033[33m"; G = "\033[32m"; Z = "\033[0m"

def curre(argv, stdout_path=None):
    """curre mandatum cum limite. reddit (exitus, True) vel (None, False)."""
    try:
        f = open(stdout_path, "w") if stdout_path else subprocess.DEVNULL
        r = subprocess.run(argv, stdout=f, stderr=subprocess.DEVNULL, timeout=TIMEOUT)
        if stdout_path:
            f.close()
        return r.returncode, True
    except subprocess.TimeoutExpired:
        if stdout_path:
            f.close()
        return None, False
    except OSError:
        if stdout_path and not f.closed:
            f.close()
        return None, False

def silens(argv):
    quiet = dict(stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return subprocess.run(argv, **quiet).returncode

def color_stat(v, bonus=("0",)):
    if v in bonus: return f"{v:>7}"
    return f"{R}{v:>7}{Z}"

def color_run(v):
    if v == "par":  return f"{G}{v:>7}{Z}"
    if v == "dis":  return f"{R}{v:>7}{Z}"
    if v in ("-", "0"): return f"{v:>7}"
    return f"{R}{v:>7}{Z}"

def main():
    programmae = []
    for entry in sorted(os.listdir(DIR)):
        prog_dir = os.path.join(DIR, entry)
        if not os.path.isdir(prog_dir):
            continue
        fontes = sorted(glob.glob(os.path.join(prog_dir, "*.c")))
        if not fontes:
            continue
        programmae.append((entry, fontes))

    if not programmae:
        print(f"nullus casus in {DIR}/")
        return

    with tempfile.TemporaryDirectory() as tmp:
        tot = 0
        s_ok = cc_asm_ok = imm_asm_ok = 0
        cc_link_ok = imm_link_ok = 0
        par_cnt = 0

        print(f"{'CASUS':16s} {'CC -S':>7} {'CC -c':>7} {'IMM':>7} {'LD/CC':>7} {'LD/IMM':>7} {'RUN/CC':>7} {'RUN/IMM':>7} {'CMP':>7}")

        for nomen, fontes_orig in programmae:
            tot += 1
            prog_tmp = os.path.join(tmp, f"src_{nomen}")
            shutil.copytree(os.path.dirname(fontes_orig[0]), prog_tmp)
            fontes = sorted(glob.glob(os.path.join(prog_tmp, "*.c")))

            # stadium 1: cc -S pro singulis .c
            s_files = []
            s_stat = "0"
            for fons in fontes:
                s = os.path.splitext(fons)[0] + ".s"
                if silens([CC, "-std=c99", "-O0", "-S", "-o", s, fons]) != 0:
                    s_stat = "err"; break
                s_files.append(s)

            # stadium 2: assembla utroque modo
            cc_objs = []; imm_objs = []
            cc_stat = imm_stat = "-"
            if s_stat == "0":
                s_ok += 1
                cc_stat = imm_stat = "0"
                for s in s_files:
                    o_cc  = s[:-2] + ".cc.o"
                    o_imm = s[:-2] + ".imm.o"
                    if cc_stat == "0" and silens([CC, "-c", "-o", o_cc, s]) != 0:
                        cc_stat = "err"
                    else:
                        cc_objs.append(o_cc)
                    if imm_stat == "0" and silens([IMM, s, "-o", o_imm]) != 0:
                        imm_stat = "err"
                    else:
                        imm_objs.append(o_imm)
                if cc_stat == "0": cc_asm_ok += 1
                if imm_stat == "0": imm_asm_ok += 1

            # stadium 3: coniunge utramque seriem
            cc_link = imm_link = "-"
            cc_bin = imm_bin = None
            if cc_stat == "0":
                cc_bin = os.path.join(tmp, f"{nomen}.cc.bin")
                if silens([CC, "-o", cc_bin] + cc_objs) == 0:
                    cc_link = "0"; cc_link_ok += 1
                else:
                    cc_link = "err"; cc_bin = None
            if imm_stat == "0":
                imm_bin = os.path.join(tmp, f"{nomen}.imm.bin")
                if silens([CC, "-o", imm_bin] + imm_objs) == 0:
                    imm_link = "0"; imm_link_ok += 1
                else:
                    imm_link = "err"; imm_bin = None

            # stadium 4: curre et compara
            cc_run  = imm_run = "-"
            cc_out  = os.path.join(tmp, f"{nomen}.cc.out")
            imm_out = os.path.join(tmp, f"{nomen}.imm.out")
            if cc_bin:
                rc, ok = curre([cc_bin], cc_out)
                if not ok: cc_run = "temp"
                elif rc < 0: cc_run = str(128 - rc)
                else: cc_run = str(rc)
            if imm_bin:
                rc, ok = curre([imm_bin], imm_out)
                if not ok: imm_run = "temp"
                elif rc < 0: imm_run = str(128 - rc)
                else: imm_run = str(rc)

            cmp_res = "-"
            if cc_run != "-" and imm_run != "-":
                if cc_run == imm_run:
                    try:
                        a = open(cc_out, "rb").read()
                        b = open(imm_out, "rb").read()
                        if a == b:
                            cmp_res = "par"; par_cnt += 1
                        else:
                            cmp_res = "dis"
                    except OSError:
                        cmp_res = "dis"
                else:
                    cmp_res = "dis"

            has_bad = (s_stat != "0" or cc_stat not in ("0","-") or imm_stat not in ("0","-")
                       or cc_link not in ("0","-") or imm_link not in ("0","-")
                       or cmp_res == "dis"
                       or (imm_run not in ("-",) and imm_run != cc_run))
            nomen_c = f"{Y}{nomen:16s}{Z}" if has_bad else f"{nomen:16s}"

            print(f"{nomen_c}"
                  f" {color_stat(s_stat):>7}"
                  f" {color_stat(cc_stat,('0','-')):>7}"
                  f" {color_stat(imm_stat,('0','-')):>7}"
                  f" {color_stat(cc_link,('0','-')):>7}"
                  f" {color_stat(imm_link,('0','-')):>7}"
                  f" {color_run(cc_run)}"
                  f" {color_run(imm_run)}"
                  f" {color_run(cmp_res)}")

        print()
        print("=== SUMMA ===")
        print(f"casus totales:           {tot}")
        print(f"CC -S  confecit:         {s_ok:>3} / {tot}")
        print(f"CC -c  assemblavit:      {cc_asm_ok:>3} / {tot}")
        print(f"IMM    assemblavit:      {imm_asm_ok:>3} / {tot}")
        print(f"CC     coniunxit (cc):   {cc_link_ok:>3} / {tot}")
        print(f"CC     coniunxit (imm):  {imm_link_ok:>3} / {tot}")
        print(f"par CC vs IMM:           {par_cnt:>3} / {tot}")

if __name__ == "__main__":
    main()
