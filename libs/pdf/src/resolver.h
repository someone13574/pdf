#pragma once

#include "ctx.h"
#include "pdf/resolver.h"

PdfCtx* pdf_resolver_ctx(PdfResolver* resolver);

/// Replaces the resolver's ctx, returning the previous one
PdfCtx* pdf_resolver_swap_ctx(PdfResolver* resolver, PdfCtx* ctx);
