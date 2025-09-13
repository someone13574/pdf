import gdb
import fmt_helpers
import math

DVEC_DEBUG_MAGIC = 0x5645435F444247


class DVecPrinter:
    def __init__(self, val):
        self.val = fmt_helpers.strip_ptr_and_typedefs(val)

        try:
            self.length = int(self.val["len"])
        except Exception:
            self.length = 0

        try:
            self.num_blocks = int(self.val["allocated_blocks"])
        except Exception:
            self.num_blocks = 0

        try:
            self.blocks = self.val["blocks"]
        except Exception:
            self.blocks = None

    def to_string(self):
        return f"dvec[{self.length}] ({self.num_blocks} blocks)"

    def children(self):
        if not self.blocks:
            return

        try:
            block_type = self.blocks.type

            if self.length < 0:
                return
            for idx in range(self.length):
                block_idx = math.floor(math.log2(idx + 1))
                idx_in_block = idx - (2**block_idx - 1)

                if block_idx >= self.num_blocks or idx < 0:
                    yield (f"[{idx}]", "shit")
                else:
                    try:
                        yield (f"[{idx}]", self.blocks[block_idx][idx_in_block])
                    except Exception:
                        yield (f"[{idx}]", "<error reading element>")
        except Exception:
            return

    def display_hint(self):
        return "array"


def dvec_printer_lookup(val):
    try:
        candidate = fmt_helpers.strip_ptr_and_typedefs(val)
        magic_field = candidate["dbg_magic"]
        magic = int(magic_field)
    except Exception:
        return None

    if magic == DVEC_DEBUG_MAGIC:
        return DVecPrinter(val)

    return None


def register_dvec_printers(objfile=None):
    if objfile is None:
        gdb.pretty_printers.append(dvec_printer_lookup)
    else:
        gdb.printing.register_pretty_printer_for_object_file(objfile, dvec_printer_lookup)


register_dvec_printers()
