#!/bin/bash
#
# proba_casus.sh — compilat quemque casum cum clang et ccc,
# comparat exitus et output.
#

CCC="${CCC:-./ccc}"
LDI="${LDI:-./ldi}"
CC="${CC:-cc}"
DIR="${1:-casus}"
TMP=$(mktemp -d)

trap "rm -rf $TMP" EXIT

tot=0
par=0
comp_ccc_ok=0
comp_cc_ok=0

# curre cum limite temporis per backgrounding
curre_cum_limite() {
    "$@" &
    local pid=$!
    (sleep 5 && kill $pid 2>/dev/null) &
    local watchdog=$!
    wait $pid 2>/dev/null
    local ret=$?
    kill $watchdog 2>/dev/null
    wait $watchdog 2>/dev/null
    return $ret
}

printf "%-16s  %4s %4s  %4s %4s  %s\n" \
       "PLICA" "CC" "CCC" "cc" "ccc" "OUTPUT"
printf "%-16s  %4s %4s  %4s %4s  %s\n" \
       "----" "comp" "comp" "run" "run" "------"

for fons in "$DIR"/*.c; do
    [ -f "$fons" ] || continue
    nomen=$(basename "$fons" .c)
    tot=$((tot + 1))

    bin_cc="$TMP/${nomen}_cc"
    bin_ccc="$TMP/${nomen}_ccc"
    out_cc="$TMP/${nomen}_cc.out"
    out_ccc="$TMP/${nomen}_ccc.out"

    # compila cum clang
    $CC -std=c99 -O0 -o "$bin_cc" "$fons" 2>/dev/null
    cc_comp=$?

    # compila cum ccc + ldi
    obj_ccc="$TMP/${nomen}_ccc.o"
    $CCC -Scapita -c -o "$obj_ccc" "$fons" >/dev/null 2>&1 && \
    $LDI -o "$bin_ccc" "$obj_ccc" >/dev/null 2>&1
    ccc_comp=$?

    cc_run="-"
    ccc_run="-"
    cmp_res="-"

    # curre clang binarium
    if [ $cc_comp -eq 0 ]; then
        comp_cc_ok=$((comp_cc_ok + 1))
        curre_cum_limite "$bin_cc" > "$out_cc" 2>&1
        cc_run=$?
    fi

    # curre ccc binarium
    if [ $ccc_comp -eq 0 ]; then
        comp_ccc_ok=$((comp_ccc_ok + 1))
        curre_cum_limite "$bin_ccc" > "$out_ccc" 2>&1
        ccc_run=$?
    fi

    # compara output
    if [ $cc_comp -eq 0 ] && [ $ccc_comp -eq 0 ]; then
        if [ "$cc_run" != "-" ] && [ "$ccc_run" != "-" ]; then
            if diff -q "$out_cc" "$out_ccc" >/dev/null 2>&1 && \
               [ "$cc_run" = "$ccc_run" ]; then
                cmp_res="PAR"
                par=$((par + 1))
            elif diff -q "$out_cc" "$out_ccc" >/dev/null 2>&1; then
                cmp_res="exit!=$ccc_run"
            else
                cmp_res="DISPAR"
            fi
        fi
    elif [ $ccc_comp -ne 0 ]; then
        cmp_res="CCC:FALLIT"
    fi

    # color
    case "$cmp_res" in
        PAR)        col="\033[32m" ;;
        *FALLIT*)   col="\033[31m" ;;
        *)          col="\033[33m" ;;
    esac
    reset="\033[0m"

    printf "%-16s  %4s %4s  %4s %4s  ${col}%s${reset}\n" \
           "$nomen" "$cc_comp" "$ccc_comp" "$cc_run" "$ccc_run" "$cmp_res"
done

echo ""
echo "=== SUMMA ==="
echo "casus totales:     $tot"
echo "cc compilavit:     $comp_cc_ok / $tot"
echo "ccc compilavit:    $comp_ccc_ok / $tot"
echo "output par:        $par / $tot"
