#pragma once

#define LOG_GROUPS                                                             \
    X(GLOBAL, DEBUG, )                                                         \
    X(EXAMPLE, TRACE, GLOBAL)                                                  \
    X(TEST, TRACE, GLOBAL)                                                     \
    X(ARENA, OFF, GLOBAL)                                                      \
    X(STRING, INHERIT, ARENA)                                                  \
    X(ARRAY, INHERIT, ARENA)                                                   \
    X(VEC, INHERIT, ARENA)                                                     \
    X(LINKED_LIST, INHERIT, ARENA)                                             \
    X(CODEC, INHERIT, GLOBAL)                                                  \
    X(CANVAS, OFF, GLOBAL)                                                     \
    X(DCEL, TRACE, CANVAS)                                                     \
    X(PDF, INHERIT, GLOBAL)                                                    \
    X(DOC, TRACE, PDF)                                                         \
    X(XREF, INHERIT, PDF)                                                      \
    X(CTX, INFO, PDF)                                                          \
    X(OBJECT, OFF, PDF)                                                        \
    X(DESERDE, INHERIT, PDF)                                                   \
    X(RENDER, INHERIT, GLOBAL)                                                 \
    X(SFNT, INHERIT, GLOBAL)                                                   \
    X(SFNT_PARSE, INHERIT, SFNT)
