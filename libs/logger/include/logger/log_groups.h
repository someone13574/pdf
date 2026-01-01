#pragma once

#define LOG_GROUPS                                                             \
    X(GLOBAL, INFO, )                                                          \
    X(EXAMPLE, TRACE, GLOBAL)                                                  \
    X(TEST, TRACE, GLOBAL)                                                     \
    X(ARENA, OFF, GLOBAL)                                                      \
    X(STRING, INHERIT, ARENA)                                                  \
    X(ARRAY, INHERIT, ARENA)                                                   \
    X(VEC, INHERIT, ARENA)                                                     \
    X(LINKED_LIST, INHERIT, ARENA)                                             \
    X(CODEC, OFF, GLOBAL)                                                      \
    X(CANVAS, INHERIT, GLOBAL)                                                 \
    X(DCEL, INHERIT, CANVAS)                                                   \
    X(PDF, INHERIT, GLOBAL)                                                    \
    X(DOC, INHERIT, PDF)                                                       \
    X(XREF, INHERIT, PDF)                                                      \
    X(CTX, INHERIT, PDF)                                                       \
    X(OBJECT, OFF, PDF)                                                        \
    X(DESERDE, OFF, PDF)                                                       \
    X(RENDER, INHERIT, GLOBAL)                                                 \
    X(FONT, INHERIT, GLOBAL)                                                   \
    X(PS, INFO, GLOBAL)                                                        \
    X(PS_TOKEN, INHERIT, PS)                                                   \
    X(CMAP, INHERIT, FONT)                                                     \
    X(SFNT, OFF, GLOBAL)                                                       \
    X(SFNT_PARSE, INHERIT, SFNT)                                               \
    X(CFF, INHERIT, GLOBAL)
