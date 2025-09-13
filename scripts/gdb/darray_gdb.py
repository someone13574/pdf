import gdb
import fmt_helpers

DARRAY_DEBUG_MAGIC = 0x4152525F444247


class DArrayPrinter:
    def __init__(self, val):
        self.val = fmt_helpers.strip_ptr_and_typedefs(val)

        try:
            self.length = int(self.val["len"])
        except Exception:
            self.length = 0

        try:
            self.elements = self.val["elements"]
        except Exception:
            self.elements = None

    def to_string(self):
        return f"darray[{self.length}]"

    def children(self):
        if not self.elements:
            return

        try:
            element_type = self.elements.type

            if self.length < 0:
                return
            for idx in range(self.length):
                try:
                    yield ("[%d]" % idx, self.elements[idx])
                except Exception:
                    yield ("[%d]" % idx, "<error reading element>")
        except Exception:
            return

    def display_hint(self):
        return "array"


def darray_printer_lookup(val):
    try:
        candidate = fmt_helpers.strip_ptr_and_typedefs(val)
        magic_field = candidate["dbg_magic"]
        magic = int(magic_field)
    except Exception:
        return None

    if magic == DARRAY_DEBUG_MAGIC:
        return DArrayPrinter(val)

    return None


def register_darray_printers(objfile=None):
    if objfile is None:
        gdb.pretty_printers.append(darray_printer_lookup)
    else:
        gdb.printing.register_pretty_printer_for_object_file(objfile, darray_printer_lookup)


register_darray_printers()
