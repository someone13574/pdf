#pragma once

#define DESERIALIZABLE_STRUCT_REF(base_struct, lowercase_name)                 \
    typedef struct {                                                           \
        PdfIndirectRef ref;                                                    \
        void* resolved;                                                        \
    } base_struct##Ref;                                                        \
    base_struct* pdf_resolve_##lowercase_name(                                 \
        base_struct##Ref* ref,                                                 \
        PdfDocument* doc,                                                      \
        PdfResult* result                                                      \
    );

#define DESERIALIZABLE_ARRAY_TYPE(struct_name)                                 \
    typedef struct {                                                           \
        Vec* elements;                                                         \
    } struct_name;

#define DESERIALIZABLE_OPTIONAL_TYPE(optional_struct, base_struct)             \
    typedef struct {                                                           \
        bool discriminant;                                                     \
        base_struct value;                                                     \
    } optional_struct;
