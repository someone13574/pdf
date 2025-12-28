from os import listdir
from os.path import isfile, join
import re
import csv

encodings_path = "assets/encodings"
encoding_files = [f for f in listdir(encodings_path) if isfile(join(encodings_path, f))]

common_mapping: list[str | None] = [None] * 256
mappings: list[tuple[str, list[str | None]]] = []

for path, encoding in [
    (join(encodings_path, f), f.split(".")[0]) for f in encoding_files
]:
    mapping: list[str | None] = [None] * 256

    with open(path, newline="") as csv_file:
        csv_reader = csv.reader(csv_file)
        for [text, number] in csv_reader:
            mapping[int(number)] = text
            if common_mapping[int(number)] is None:
                common_mapping[int(number)] = text
            elif common_mapping[int(number)] != text:
                common_mapping[int(number)] = "NON_MATCHING"

    snake_case = re.sub(r"(?<!^)(?=[A-Z])", "_", encoding).lower()
    mappings += [(snake_case, mapping)]

print(
    "static const char* pdf_encoding_common_lookup[256] = {"
    + ", ".join(
        f'[{i}] = "{x}"'
        for i, x in enumerate(common_mapping)
        if x and x != "NON_MATCHING"
    )
    + "};\n"
)

headers: list[str] = []

for snake_case, mapping in mappings:
    print(
        f"static const char* pdf_encoding_{snake_case}_lookup[256] = {{"
        + ", ".join(
            f'[{i}] = "{x}"'
            for i, x in enumerate(mapping)
            if x and (common_mapping[i] is None or common_mapping[i] == "NON_MATCHING")
        )
        + "};\n"
    )

    header = f"const char* pdf_decode_{snake_case}_codepoint(uint8_t codepoint)"
    headers += [header]

    print(header + " {")
    print("    const char* common = pdf_encoding_common_lookup[codepoint];")
    print("    if (common) {\n        return common;\n    }\n")
    print(f"    return pdf_encoding_{snake_case}_lookup[codepoint];")
    print("}\n")

for header in headers:
    print(header + ";")
