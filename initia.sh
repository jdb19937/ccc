#!/bin/bash
#
# initia.sh — praepara directorium initiationis (bootstrap)
#
# Copiat plicas fontis in directorium novum et scribit Faceplicam
# quae compilatorem et ligatorem datos utitur, sequens obiecta
# enumerāta in Faceplica externā.
#
# Usus:  ./initia.sh <directorium> <via_ccc> <via_ldi>
#
# Exemplum — initiatio duplex:
#   face
#   ./initia.sh initia ./ccc ./ldi
#   face -C initia
#   ./initia.sh reinitia ./initia/ccc ./initia/ldi
#   face -C reinitia

if [ $# -ne 3 ]; then
    echo "usus: $0 <directorium> <via_ccc> <via_ldi>" >&2
    exit 1
fi

DIR_INITIA="$1"
VIA_CCC="$2"
VIA_LDI="$3"

# crea directorium
mkdir -p "$DIR_INITIA"

# copia plicas fontis
cp *.c *.h "$DIR_INITIA/"

# scribe Faceplicam — obiecta sequuntur Faceplicam principalem.
# Notā: ccc compilat per imm subprocessum (requirit imm in eādem
# directoria ac ccc). Ergo hīc ūtimur VIA_CCC quod sēcum fert suum
# imm (e.g. ../ccc → ../imm). In secundo passū (reinitia), ccc
# initia-compilatum ūtitur imm initia-compilatum in paenē directoria.
cat > "$DIR_INITIA/Faceplica" << FINIS
# Faceplica — initiatio ccc, ldi, imm, iccc
#
# Compilatum per: $VIA_CCC
# Ligatum per:    $VIA_LDI

CCC     = $VIA_CCC
LDI     = $VIA_LDI

ICCC_OBJECTA = iccc.o
CCC_OBJECTA  = ccc.o utilia.o lexator.o parser.o generasym.o emittesym.o emitte.o biblio.o fluat.o typus.o func.o
LDI_OBJECTA  = ldi.o utilia.o liga.o emitte.o scribo.o biblio.o typus.o func.o fluat.o
IMM_OBJECTA  = imm.o utilia.o emitte.o scribo.o biblio.o fluat.o typus.o func.o

omnia: ccc ldi iccc imm

%.o: %.c
	\$(CCC) -S../capita \$<

iccc: \$(ICCC_OBJECTA)
	\$(LDI) -o \$@ \$^
	@echo "==> iccc initiatum"

ccc: \$(CCC_OBJECTA)
	\$(LDI) -o \$@ \$^
	@echo "==> ccc initiatum"

ldi: \$(LDI_OBJECTA)
	\$(LDI) -o \$@ \$^
	@echo "==> ldi initiatum"

imm: \$(IMM_OBJECTA)
	\$(LDI) -o \$@ \$^
	@echo "==> imm initiatum"

purga:
	rm -f *.o ccc ldi iccc imm

.PHONY: omnia purga
FINIS

echo "=== initiatio parata: $DIR_INITIA/ ==="
echo "  compilator: $VIA_CCC"
echo "  ligator:    $VIA_LDI"
echo "  ad aedificandum: face -C $DIR_INITIA"
