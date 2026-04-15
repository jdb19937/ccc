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
#   ./initia.sh initia ./ccc ./ldi

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

# scribe Faceplicam
cat > "$DIR_INITIA/Faceplica" << FINIS
# Faceplica — initiatio ccc et ldi
#
# Compilatum per: $VIA_CCC
# Ligatum per:    $VIA_LDI

CCC     = $VIA_CCC
LDI     = $VIA_LDI

CCC_OBJECTA = ccc.o lexator.o parser.o genera.o emitte.o scribo.o biblio.o fluat.o
LDI_OBJECTA = ldi.o liga.o emitte.o scribo.o biblio.o
OBJECTA     = ccc.o ldi.o lexator.o parser.o genera.o emitte.o scribo.o liga.o biblio.o fluat.o

omnia: ccc ldi

%.o: %.c
	\$(CCC) -S../capita -o \$@ -c \$<

ccc: \$(CCC_OBJECTA)
	\$(LDI) -o \$@ \$(CCC_OBJECTA)
	@echo "==> ccc initiatum"

ldi: \$(LDI_OBJECTA)
	\$(LDI) -o \$@ \$(LDI_OBJECTA)
	@echo "==> ldi initiatum"

purga:
	rm -f \$(OBJECTA) ccc ldi

.PHONY: omnia purga
FINIS

echo "=== initiatio parata: $DIR_INITIA/ ==="
echo "  compilator: $VIA_CCC"
echo "  ligator:    $VIA_LDI"
echo "  ad aedificandum: face -C $DIR_INITIA"
