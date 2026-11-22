#!/bin/bash
# Recette des parties pures de oldmac-esp32, sur le PC (aucune carte).
#   tests/run_unit.sh            toutes les suites
#   tests/run_unit.sh safari     une seule suite
# Chaque suite affiche "<nom>  N verifications, M echec(s)".
set -u
cd "$(dirname "$0")/.." || exit 2
mkdir -p build/tests
rc=0
filtre="${1:-}"

CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -I main"
suite() {   # suite <nom> <binaire> <sources...>
    local nom="$1" bin="$2"; shift 2
    if [ -n "$filtre" ] && [ "$filtre" != "$nom" ]; then return 0; fi
    echo "— $nom"
    if ! g++ $CXXFLAGS -o "build/tests/$bin" "$@"; then
        echo "  COMPILATION ECHOUEE"
        rc=$((rc+1))
        return 0
    fi
    "./build/tests/$bin" || rc=$((rc+1))
}

# HTML -> texte, pagination : le cœur de SAFARI
suite safari test_safari tests/unit/test_safari.cpp main/safari_txt.cpp

# geometrie du clic : les zones tombent-elles sur le dessin ?
suite souris test_souris tests/unit/test_souris.cpp main/ui.cpp main/art.cpp \
    tools/safari_stub.cpp

echo
if [ $rc -eq 0 ]; then echo "unitaires : tout bon"; else echo "unitaires : $rc suite(s) en echec"; fi
exit $rc
