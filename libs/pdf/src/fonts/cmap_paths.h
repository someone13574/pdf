#include "err/error.h"

/// Translate the name of a cmap into its asset's path.
Error* pdf_cmap_name_to_path(char* cmap_name, const char** cmap_path_out);
