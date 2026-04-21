#!/usr/bin/env python3
"""
initia.py — praepara directorium initiationis (bootstrap)

Copiat plicas fontis in directorium novum et scribit Faceplicam
quae compilatorem et ligatorem datos utitur, sequens obiecta
enumerata in Faceplica externa.

Usus:  ./initia.py <directorium> <via_ccc> <via_ldi>

Exemplum — initiatio duplex:
    face
    ./initia.py initia ./ccc ./ldi
    face -C initia
    ./initia.py reinitia ./initia/ccc ./initia/ldi
    face -C reinitia
"""

import os
import re
import shutil
import sys
from pathlib import Path


def lege_objecta(faceplica: Path) -> dict[str, str]:
    """Extrahe listas obiectorum ex Faceplica principali."""
    obj = {}
    for linea in faceplica.read_text().splitlines():
        m = re.match(r"^\s*(\w+_OBJECTA)\s*=\s*(.*)$", linea)
        if m:
            obj[m.group(1)] = m.group(2).strip()
    return obj


def scribe_faceplicam(
    dir_initia: Path, via_ccc: str, via_ldi: str, objecta: dict[str, str]
) -> None:
    faceplica = f"""# Faceplica — initiatio ccc, ldi, imm, iccc
#
# Compilatum per: {via_ccc}
# Ligatum per:    {via_ldi}

CCC     = {via_ccc}
LDI     = {via_ldi}

ICCC_OBJECTA = {objecta.get('ICCC_OBJECTA', 'iccc.o')}
CCC_OBJECTA  = {objecta['CCC_OBJECTA']}
LDI_OBJECTA  = {objecta['LDI_OBJECTA']}
IMM_OBJECTA  = {objecta['IMM_OBJECTA']}

omnia: ccc ldi iccc imm

%.o: %.c
\t$(CCC) -S../capita $<

iccc: $(ICCC_OBJECTA)
\t$(LDI) -o $@ $^
\t@echo "==> iccc initiatum"

ccc: $(CCC_OBJECTA)
\t$(LDI) -o $@ $^
\t@echo "==> ccc initiatum"

ldi: $(LDI_OBJECTA)
\t$(LDI) -o $@ $^
\t@echo "==> ldi initiatum"

imm: $(IMM_OBJECTA)
\t$(LDI) -o $@ $^
\t@echo "==> imm initiatum"

purga:
\trm -f *.o ccc ldi iccc imm

.PHONY: omnia purga
"""
    (dir_initia / "Faceplica").write_text(faceplica)


def main() -> int:
    if len(sys.argv) != 4:
        print(
            f"usus: {sys.argv[0]} <directorium> <via_ccc> <via_ldi>",
            file=sys.stderr,
        )
        return 1

    dir_initia = Path(sys.argv[1])
    via_ccc = sys.argv[2]
    via_ldi = sys.argv[3]

    fons_dir = Path(__file__).resolve().parent
    objecta = lege_objecta(fons_dir / "Faceplica")
    for clavis in ("CCC_OBJECTA", "LDI_OBJECTA", "IMM_OBJECTA"):
        if clavis not in objecta:
            print(f"Faceplica sine {clavis}", file=sys.stderr)
            return 1

    dir_initia.mkdir(parents=True, exist_ok=True)

    for p in fons_dir.glob("*.c"):
        shutil.copy2(p, dir_initia / p.name)
    for p in fons_dir.glob("*.h"):
        shutil.copy2(p, dir_initia / p.name)

    scribe_faceplicam(dir_initia, via_ccc, via_ldi, objecta)

    print(f"=== initiatio parata: {dir_initia}/ ===")
    print(f"  compilator: {via_ccc}")
    print(f"  ligator:    {via_ldi}")
    print(f"  ad aedificandum: face -C {dir_initia}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
