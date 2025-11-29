#!/usr/bin/env python3
import csv
import urllib.request
import sys
import re

URL = "https://ddb.glidernet.org/download/"

def download_csv():
    with urllib.request.urlopen(URL) as response:
        return response.read().decode("utf-8", errors="replace")

def parse_csv(text):
    rows = []
    r = csv.reader(text.splitlines())
    header = next(r)   # skip header

    for row in r:
        dev_type = row[0].strip().strip("'")
        hexcode  = row[1].strip().strip("'")
        reg      = row[3].strip().strip("'")   # callsign/registration

        if not hexcode:
            continue

        # Skip empty callsigns
        if len(reg) < 1 or re.search('test', reg, re.IGNORECASE):
            continue

        rows.append((hexcode, dev_type, reg))
    return rows


def gen_cpp(rows):
    out = []
    out.append("#pragma once\n")
    out.append("// Auto-generated — DO NOT EDIT\n")
    out.append("#include <stdint.h>\n\n")

    # ----------------------------------------------------------------------
    # Build big string blob
    # ----------------------------------------------------------------------
    string_blob = []
    offsets = []
    offset = 0

    for hexcode, dev_type, reg in rows:
        offsets.append(offset)
        string_blob.append(reg + "\\0")
        offset += len(reg) + 1

    # Emit string blob
    out.append("static constexpr char DDB_STRINGS[] __in_flash() =\n")
    for s in string_blob:
        escaped = s.replace("\\", "\\").replace('"', '\\"')
        out.append(f'    "{escaped}"\n')
    out.append(";\n\n")

    # ----------------------------------------------------------------------
    # Struct
    # ----------------------------------------------------------------------
    out.append(
        "struct DDBEntry {\n"
        "    uint8_t _hex[3];      // 24-bit ICAO\n"
        "    uint32_t offset;      // offset into DDB_STRINGS\n"
        "    const char *reg() const { return &DDB_STRINGS[offset]; }\n"
        "    uint32_t hex() const {\n"
        "        return (uint32_t(_hex[0]) << 16) |\n"
        "               (uint32_t(_hex[1]) << 8)  |\n"
        "               (uint32_t(_hex[2]));\n"
        "    }\n"
        "};\n\n"
    )

    # ----------------------------------------------------------------------
    # DB entries
    # ----------------------------------------------------------------------
    out.append(f"static constexpr size_t DDB_COUNT = {len(rows)};\n\n")

    out.append("static constexpr DDBEntry __in_flash() DDB_DB[DDB_COUNT] = {\n")
    for i, (hexcode, dev_type, reg) in enumerate(rows):
        h = int(hexcode, 16)
        b0 = (h >> 16) & 0xFF
        b1 = (h >> 8)  & 0xFF
        b2 = (h >> 0)  & 0xFF
        out.append(f"    {{ {{ 0x{b0:02X}, 0x{b1:02X}, 0x{b2:02X} }}, {offsets[i]}u }},\n")
    out.append("};\n\n")

    # ----------------------------------------------------------------------
    # Build 256-bucket index
    # ----------------------------------------------------------------------
    bucket_start = [0] * 256
    bucket_count = [0] * 256

    # rows are sorted by 24-bit ICAO
    current_byte = None
    for i, (hexcode, dev_type, reg) in enumerate(rows):
        h = int(hexcode, 16)
        b0 = (h >> 16) & 0xFF
        if bucket_count[b0] == 0:
            bucket_start[b0] = i
        bucket_count[b0] += 1

    out.append("struct DDBIndexBucket { uint16_t start; uint16_t count; };\n\n")
    out.append("static constexpr DDBIndexBucket DDB_INDEX[256] __in_flash() = {\n")

    for i in range(256):
        out.append(f"    {{ {bucket_start[i]}, {bucket_count[i]} }},\n")

    out.append("};\n")

    return "".join(out)


# Main -------------------------------------------------------------------------
if __name__ == "__main__":
    text = download_csv()
    rows = parse_csv(text)

    swapped_rows = []
    for hexcode, dev_type, reg in rows:
        h = int(hexcode, 16)
        b0 = (h >> 16) & 0xFF   # A
        b1 = (h >> 8)  & 0xFF   # B
        b2 = (h >> 0)  & 0xFF   # C

        swapped = (b2 << 16) | (b1 << 8) | b0  # CC BB AA
        swapped_hex = f"{swapped:06X}"

        swapped_rows.append((swapped_hex, dev_type, reg))

    # Sort by HEX for fast binary search
    swapped_rows.sort(key=lambda x: int(x[0], 16))

    cpp = gen_cpp(swapped_rows)
    sys.stdout.write(cpp)
