#!/usr/bin/env sh
# Хост-тести парсера протоколу JK. Потрібен лише g++/clang++ з C++17.
set -e
cd "$(dirname "$0")/.."
mkdir -p .pio
CXX="${CXX:-g++}"
"$CXX" -std=c++17 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined \
    test/test_protocol.cpp src/jk_protocol.cpp -o .pio/test_protocol
./.pio/test_protocol
