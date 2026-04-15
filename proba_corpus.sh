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
comp_cc_ok=0
comp_ccc_ok=0
comp_cc_ldi_ok=0
comp_ccc_ld_ok=0
par_cc_ldi=0
par_ccc_ld=0

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

printf "%-16s   -------- compilātiō ---------    ----------- cursus ----------\n" ""
printf "%-16s" "PLICA"
printf " %7s" "CC/LD" "CCC/LDI" "CC/LDI" "CCC/LD"
printf " "
printf " %7s" "CC/LD" "CCC/LDI" "CC/LDI" "CCC/LD"
printf "\n"

for fons in "$DIR"/*.c; do
    [ -f "$fons" ] || continue
    nomen=$(basename "$fons" .c)
    tot=$((tot + 1))

    bin_cc="$TMP/${nomen}_cc"
    bin_ccc="$TMP/${nomen}_ccc"
    bin_cc_ldi="$TMP/${nomen}_cc_ldi"
    bin_ccc_ld="$TMP/${nomen}_ccc_ld"
    out_cc="$TMP/${nomen}_cc.out"
    out_ccc="$TMP/${nomen}_ccc.out"
    out_cc_ldi="$TMP/${nomen}_cc_ldi.out"
    out_ccc_ld="$TMP/${nomen}_ccc_ld.out"

    # 1. cc/cc
    obj_cc_ld="$TMP/${nomen}_cc_ld.o"
    $CC -std=c99 -O0 -c -o "$obj_cc_ld" "$fons" 2>/dev/null
    cc_comp=$?
    if [ $cc_comp -eq 0 ]; then
        $CC -o "$bin_cc" "$obj_cc_ld" 2>/dev/null
        if [ $? -ne 0 ]; then cc_comp="link"; fi
    else cc_comp="comp"; fi

    # 2. ccc/ldi
    obj_ccc="$TMP/${nomen}_ccc.o"
    $CCC -Scapita -c -o "$obj_ccc" "$fons" >/dev/null 2>&1
    ccc_comp=$?
    if [ $ccc_comp -eq 0 ]; then
        $LDI -o "$bin_ccc" "$obj_ccc" >/dev/null 2>&1
        if [ $? -ne 0 ]; then ccc_comp="link"; fi
    else ccc_comp="comp"; fi

    # 3. cc/ldi
    obj_cc="$TMP/${nomen}_cc.o"
    $CC -std=c99 -O0 -c -o "$obj_cc" "$fons" 2>/dev/null
    cc_ldi_comp=$?
    if [ $cc_ldi_comp -eq 0 ]; then
        $LDI -o "$bin_cc_ldi" "$obj_cc" >/dev/null 2>&1
        if [ $? -ne 0 ]; then cc_ldi_comp="link"; fi
    else cc_ldi_comp="comp"; fi

    # 4. ccc/cc
    $CCC -Scapita -c -o "$obj_ccc" "$fons" >/dev/null 2>&1
    ccc_ld_comp=$?
    if [ $ccc_ld_comp -eq 0 ]; then
        $CC -o "$bin_ccc_ld" "$obj_ccc" 2>/dev/null
        if [ $? -ne 0 ]; then ccc_ld_comp="link"; fi
    else ccc_ld_comp="comp"; fi

    cc_run="-"; ccc_run="-"; cc_ldi_run="-"; ccc_ld_run="-"
    cmp1="-"; cmp2="-"; cmp3="-"

    # curre cc/cc
    if [ "$cc_comp" = "0" ]; then
        comp_cc_ok=$((comp_cc_ok + 1))
        curre_cum_limite "$bin_cc" > "$out_cc" 2>&1
        cc_run=$?
    fi

    # curre ccc/ldi et compara cum cc/cc
    if [ "$ccc_comp" = "0" ]; then
        comp_ccc_ok=$((comp_ccc_ok + 1))
        curre_cum_limite "$bin_ccc" > "$out_ccc" 2>&1
        ccc_run=$?
        if [ "$cc_run" != "-" ] && [ "$ccc_run" -eq 0 ] && [ "$cc_run" -eq 0 ]; then
            if diff -q "$out_cc" "$out_ccc" >/dev/null 2>&1; then
                ccc_run="par"; par=$((par + 1))
            else ccc_run="dis"; fi
        fi
    fi

    # curre cc/ldi et compara cum cc/cc
    if [ "$cc_ldi_comp" = "0" ]; then
        comp_cc_ldi_ok=$((comp_cc_ldi_ok + 1))
        curre_cum_limite "$bin_cc_ldi" > "$out_cc_ldi" 2>&1
        cc_ldi_run=$?
        if [ "$cc_run" != "-" ] && [ "$cc_ldi_run" -eq 0 ] && [ "$cc_run" -eq 0 ]; then
            if diff -q "$out_cc" "$out_cc_ldi" >/dev/null 2>&1; then
                cc_ldi_run="par"; par_cc_ldi=$((par_cc_ldi + 1))
            else cc_ldi_run="dis"; fi
        fi
    fi

    # curre ccc/cc et compara cum cc/cc
    if [ "$ccc_ld_comp" = "0" ]; then
        comp_ccc_ld_ok=$((comp_ccc_ld_ok + 1))
        curre_cum_limite "$bin_ccc_ld" > "$out_ccc_ld" 2>&1
        ccc_ld_run=$?
        if [ "$cc_run" != "-" ] && [ "$ccc_ld_run" -eq 0 ] && [ "$cc_run" -eq 0 ]; then
            if diff -q "$out_cc" "$out_ccc_ld" >/dev/null 2>&1; then
                ccc_ld_run="par"; par_ccc_ld=$((par_ccc_ld + 1))
            else ccc_ld_run="dis"; fi
        fi
    fi

    # color per cellam
    R="\033[31m"; Y="\033[33m"; G="\033[32m"; Z="\033[0m"
    has_bad=0
    for v in "$ccc_comp" "$cc_ldi_comp" "$ccc_ld_comp"; do
        case "$v" in comp|link) has_bad=1 ;; esac
    done
    for v in "$ccc_run" "$cc_ldi_run" "$ccc_ld_run"; do
        case "$v" in dis) has_bad=1 ;; -|0|par) ;; *) has_bad=1 ;; esac
    done
    case "$cc_run" in -|0) ;; *) has_bad=1 ;; esac

    if [ $has_bad -eq 1 ]; then
        printf "${Y}%-16s${Z}" "$nomen"
    else
        printf "%-16s" "$nomen"
    fi

    for v in "$cc_comp" "$ccc_comp" "$cc_ldi_comp" "$ccc_ld_comp"; do
        case "$v" in comp|link) printf " ${R}%7s${Z}" "$v" ;; *) printf " %7s" "$v" ;; esac
    done
    printf " "
    for v in "$cc_run" "$ccc_run" "$cc_ldi_run" "$ccc_ld_run"; do
        case "$v" in
            par) printf " ${G}%7s${Z}" "$v" ;;
            dis) printf " ${R}%7s${Z}" "$v" ;;
            -|0) printf " %7s" "$v" ;;
            *)   printf " ${R}%7s${Z}" "$v" ;;
        esac
    done
    printf "\n"
done

echo ""
echo "=== SUMMA ==="
echo "casus totales:           $tot"
echo "cc/cc compilavit:        $comp_cc_ok / $tot"
echo "ccc/ldi compilavit:      $comp_ccc_ok / $tot"
echo "cc/ldi compilavit:       $comp_cc_ldi_ok / $tot"
echo "ccc/cc compilavit:       $comp_ccc_ld_ok / $tot"
echo "par cc/cc vs ccc/ldi:    $par / $tot"
echo "par cc/cc vs cc/ldi:     $par_cc_ldi / $tot"
echo "par cc/cc vs ccc/cc:     $par_ccc_ld / $tot"
