#!/bin/bash
#
# initia.sh — praepara directorium initiationis (bootstrap)
#
# Copiat plicas fontis in directorium novum et scribit Faceplicam
# quae compilatorem et ligatorem datos utitur.
#
# Usus:  ./initia.sh <directorium> <via_ccc> <via_ldi>
#
# Exemplum — initiatio duplex:
#   face
#   ./initia.sh initia ./ccc ./ldi ./iccc

if [ $# -ne 3 ]; then
    echo "usus: $0 <directorium> <via_ccc> <via_ldi>" >&2
    exit 1
fi

DIR_INITIA="$1"
VIA_CCC="$2"
VIA_LDI="$3"
VIA_ICCC="$4"

# crea directorium
mkdir -p "$DIR_INITIA"

# copia plicas fontis
cp *.c *.h "$DIR_INITIA/"

# scribe Faceplicam
cat > "$DIR_INITIA/Faceplica" << FINIS
# Faceplica — initiatio ccc et ldi
#
# Compilatum per: $VIA_CCC
# Ligatum per:    $VIA_LDI

CCC     = $VIA_CCC
LDI     = $VIA_LDI
ICCC    = $VIA_ICCC

CCC_OBJECTA = ccc.o lexator.o parser.o genera.o emitte.o scribo.o biblio.o fluat.o typus.o func.o utilia.o
LDI_OBJECTA = ldi.o liga.o emitte.o scribo.o biblio.o typus.o func.o fluat.o utilia.o

omnia: ccc ldi iccc

%.o: %.c
	\$(CCC) -S../capita \$<

iccc: iccc.o
	\$(LDI) -o \$@ \$^
	@echo "==> iccc initiatum"

ccc: \$(CCC_OBJECTA)
	\$(LDI) -o \$@ \$^
	@echo "==> ccc initiatum"

ldi: \$(LDI_OBJECTA)
	\$(LDI) -o \$@ \$^
	@echo "==> ldi initiatum"

purga:
	rm -f *.o ccc ldi iccc

.PHONY: omnia purga
FINIS

echo "=== initiatio parata: $DIR_INITIA/ ==="
echo "  compilator: $VIA_CCC"
echo "  ligator:    $VIA_LDI"
echo "  precompilator:    $VIA_ICCC"
echo "  ad aedificandum: face -C $DIR_INITIA"
