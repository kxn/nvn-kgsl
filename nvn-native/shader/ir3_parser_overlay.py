"""SHA-locked Mesa IR3 assembler lexer fix, applied only to a build overlay.

Pinned Mesa's ISA disassembler emits hp0/hup0 destinations when a cat2
comparison has half sources. Its lexer accepts only p0/up0. parse_reg already
encodes the optional h in the low bit, and new_dst extracts that bit into
IR3_REG_HALF, so no parser, encoder, shader or precision change is necessary.

References in Mesa 26.2.5:
  src/freedreno/isa/ir3-cat2.xml:31-74,160-173
  src/freedreno/ir3/ir3_lexer.l:94-114,188-189
  src/freedreno/ir3/ir3_parser_support.c:188-195

Usage: ir3_parser_overlay.py ORIGINAL_IR3_LEXER.L OVERLAY_IR3_LEXER.L
The caller must regenerate the lexer with the existing flex build rule and
retain IR3_SHADER_DEBUG=asmroundtrip. Never patch an already generated binary.
"""
import hashlib
from pathlib import Path
import sys

SOURCE_SHA = "a33ad765b97c125e1e3e5056c0cae7ebbc833c9eae70f20e5cd41cdb1efc76da"
PROFILE = "mesa-26.2.5-ir3-half-predicate-lexer-v1"

OLD = '''"p0."[xyzw]                       ir3_yylval.num = parse_reg(yytext); return T_P0;
"up0."[xyzw]                      ir3_yylval.num = parse_reg(yytext); return T_UP0;'''
NEW = '''[h]?"p0."[xyzw]                   ir3_yylval.num = parse_reg(yytext); return T_P0;
[h]?"up0."[xyzw]                  ir3_yylval.num = parse_reg(yytext); return T_UP0;'''


def patch(source: bytes) -> bytes:
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA:
        raise ValueError("IR3 lexer changed; audit the half-predicate overlay against the new Mesa source")
    text = source.decode("utf-8")
    if text.count(OLD) != 1:
        raise ValueError("IR3 half-predicate overlay must match exactly one lexer rule pair")
    return text.replace(OLD, NEW).encode("utf-8")


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    source, destination = map(Path, sys.argv[1:])
    if source.resolve() == destination.resolve():
        raise ValueError("the lexer overlay must not overwrite its pinned input")
    destination.write_bytes(patch(source.read_bytes()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
