#pragma once

#define DVEC_NAME PdfVoidVec
#define DVEC_LOWERCASE_NAME pdf_void_vec
#define DVEC_TYPE void*
#include "arena/dvec_decl.h"

#define DESERIALIZABLE_STRUCT_REF(base_struct, lowercase_name)                 \
    typedef struct {                                                           \
        PdfIndirectRef ref;                                                    \
        void* resolved;                                                        \
    } base_struct##Ref;                                                        \
    PdfError* pdf_resolve_##lowercase_name(                                    \
        base_struct##Ref* ref,                                                 \
        PdfResolver* resolver,                                                 \
        base_struct* resolved                                                  \
    );

#define DESERIALIZABLE_ARRAY_TYPE(struct_name)                                 \
    typedef struct {                                                           \
        PdfVoidVec* elements;                                                  \
    } struct_name;

#define DESERIALIZABLE_OPTIONAL_TYPE(optional_struct, base_struct)             \
    typedef struct {                                                           \
        bool discriminant;                                                     \
        base_struct value;                                                     \
    } optional_struct;
