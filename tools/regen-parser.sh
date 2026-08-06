#!/bin/sh
# Regenerate the committed lexer/parser sources from lexer.l/parser.y.
#
# The generated files are committed so that platforms without flex and
# bison (DOS, consoles, old unices) can build from a plain checkout.
# Run this after editing lexer.l or parser.y.

set -eu
cd "$(dirname "$0")/../src/core"

flex --prefix=ast_yy -o lexer.yy.c lexer.l
# Some libcs used by the generated scanner need inttypes explicitly.
sed -i '1i #include <inttypes.h>\n' lexer.yy.c

bison -p ast_yy -d -o parser.tab.c parser.y

echo "Regenerated lexer.yy.c, parser.tab.c, parser.tab.h."
