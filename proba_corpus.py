#!/usr/bin/env python3
#
# proba_corpus.py — pro quoque casu (subdirectorio) compilat omnes .c cum
# clang et ccc quattuor compositionibus, coniungit, currit, comparat exitus
# et output.
#

import glob
import os
import shutil
import subprocess
import sys
import tempfile

CCC = os.environ.get("CCC", "./ccc")
LDI = os.environ.get("LDI", "./ldi")
CC  = os.environ.get("CC", "cc")
DIRS = sys.argv[1:] if len(sys.argv) > 1 else ["../corpus/casus"]
TIMEOUT = 5

# quattuor modi compilandi et coniungendi
# nota: ccc locale probatur, ergo -Scapita adhibemus pro capitibus localibus
MODI = [
    # (nomen,   compilator, flagella compilandi,  coniunctor, flagella coniungendi)
    ("CC/LD",   CC,  ["-std=c99", "-O0"],         CC,  []),
    ("CCC/LDI", CCC, ["-Scapita"],                LDI, []),
    ("CC/LDI",  CC,  ["-std=c99", "-O0"],         LDI, []),
    ("CCC/LD",  CCC, ["-Scapita"],                CC,  []),
]

R = "\033[31m"; Y = "\033[33m"; G = "\033[32m"; Z = "\033[0m"

def curre(argv, stdout_path=None):
    """curre mandatum cum limite temporis. reddit (exitus, True) vel (None, False) si tempus excessit."""
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

def compila_et_coniunge(comp, comp_flagella, coniunctor, coni_flagella, fontes, tmp, nomen, idx):
    """compilat omnes fontes in objecta, deinde coniungit. reddit (res, bin_).
    res: "0" si bene, "comp" vel "link" si male."""
    quiet = dict(stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    objs = []
    for j, fons in enumerate(fontes):
        obj = os.path.join(tmp, f"{nomen}_{idx}_{j}.o")
        argv = [comp] + comp_flagella + ["-c", "-o", obj, fons]
        if subprocess.run(argv, **quiet).returncode != 0:
            return "comp", None
        objs.append(obj)
    bin_ = os.path.join(tmp, f"{nomen}_{idx}")
    argv = [coniunctor] + coni_flagella + ["-o", bin_] + objs
    if subprocess.run(argv, **quiet).returncode != 0:
        return "link", None
    return "0", bin_

def color_comp(v):
    return f"{R}{v:>7}{Z}" if v in ("comp", "link") else f"{v:>7}"

def color_run(v):
    if v == "par":  return f"{G}{v:>7}{Z}"
    if v == "dis":  return f"{R}{v:>7}{Z}"
    if v in ("-", "0"): return f"{v:>7}"
    return f"{R}{v:>7}{Z}"

def main():
    programmae = []  # [(label, [fontes...])]
    multi = len(DIRS) > 1
    for d in DIRS:
        for entry in sorted(os.listdir(d)):
            prog_dir = os.path.join(d, entry)
            if not os.path.isdir(prog_dir):
                continue
            fontes = sorted(glob.glob(os.path.join(prog_dir, "*.c")))
            if not fontes:
                continue
            label = f"{os.path.basename(os.path.normpath(d))}/{entry}" if multi else entry
            programmae.append((label, fontes))

    if not programmae:
        print(f"nullus casus in {', '.join(DIRS)}")
        return

    with tempfile.TemporaryDirectory() as tmp:
        tot = 0
        comp_ok  = [0] * 4
        par_cnt  = [0] * 4  # [0] nōn ūsitātur (referentia), [1..3] comparātiōnēs

        # capita
        nomina = [m[0] for m in MODI]
        wlab = max(16, max(len(p[0]) for p in programmae))
        print(f"{'':{wlab}s}   -------- compilātiō ---------    ----------- cursus ----------")
        print(f"{'CASUS':{wlab}s}", end="")
        for n in nomina: print(f" {n:>7}", end="")
        print(" ", end="")
        for n in nomina: print(f" {n:>7}", end="")
        print()

        for idx_p, (nomen, fontes_orig) in enumerate(programmae):
            tot += 1
            safe = nomen.replace("/", "__")

            # copia programmatis directorium in tmp ne quid scribatur in fonte
            prog_tmp = os.path.join(tmp, f"src_{idx_p}_{safe}")
            shutil.copytree(os.path.dirname(fontes_orig[0]), prog_tmp)
            fontes = sorted(glob.glob(os.path.join(prog_tmp, "*.c")))

            comp_res = []
            bins = []
            outs = []

            # compila singulos modos
            for i, (mn, comp, cf, coni, lf) in enumerate(MODI):
                out = os.path.join(tmp, f"{safe}_{i}.out")
                r, bin_ = compila_et_coniunge(comp, cf, coni, lf, fontes, tmp, safe, i)
                comp_res.append(r)
                bins.append(bin_)
                outs.append(out)

            # curre et compara
            run_res = ["-"] * 4
            for i in range(4):
                if comp_res[i] != "0":
                    continue
                comp_ok[i] += 1
                rc, ok = curre(bins[i], outs[i])
                if not ok:
                    run_res[i] = "temp"
                elif rc < 0:
                    run_res[i] = str(128 - rc)  # -6 -> 134, ut bash
                else:
                    run_res[i] = str(rc)

            # compara modos 1..3 cum referentia (0 = CC/LD)
            ref_run = run_res[0]
            for i in range(1, 4):
                if run_res[i] == "-":
                    continue
                if ref_run == "0" and run_res[i] == "0":
                    if open(outs[0], "rb").read() == open(outs[i], "rb").read():
                        run_res[i] = "par"
                        par_cnt[i] += 1
                    else:
                        run_res[i] = "dis"

            # proba an aliquid malum sit
            has_bad = any(comp_res[i] in ("comp", "link") for i in range(1, 4))
            has_bad = has_bad or any(run_res[i] not in ("-", "0", "par") for i in range(1, 4))
            has_bad = has_bad or ref_run not in ("-", "0")

            if has_bad:
                print(f"{Y}{nomen:{wlab}s}{Z}", end="")
            else:
                print(f"{nomen:{wlab}s}", end="")

            for v in comp_res: print(f" {color_comp(v)}", end="")
            print(" ", end="")
            for v in run_res:  print(f" {color_run(v)}", end="")
            print()

        # summa
        print()
        print("=== SUMMA ===")
        print(f"casus totales:           {tot}")
        for i, (mn, *_) in enumerate(MODI):
            print(f"{mn:7s} compilavit:     {comp_ok[i]:>3} / {tot}")
        for i in range(1, 4):
            print(f"par CC/LD vs {MODI[i][0]:7s}:   {par_cnt[i]:>3} / {tot}")

if __name__ == "__main__":
    main()
