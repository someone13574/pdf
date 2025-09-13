import gdb

def strip_ptr_and_typedefs(val):
    """Helper function to remove pointers, qualifiers and typedefs from a value"""
    typ = val.type

    # Strips typedefs and qualifiers
    try:
        typ = typ.strip_typedefs()
    except Exception:
        pass

    # Deference pointers
    if typ.code == gdb.TYPE_CODE_PTR:
        try:
            return val.dereference()
        except gdb.error:
            # Return original value if deference fails (ie. due to NULL)
            return val
    return val
