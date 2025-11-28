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
        if len(reg) < 4 or re.search('test', reg, re.IGNORECASE):
            continue

        rows.append((hexcode, dev_type, reg))
    return rows

def gen_cpp(rows):
    out = []
    out.append("#pragma once\n")
    out.append("// Auto-generated — DO NOT EDIT\n")
    out.append("#include <stdint.h>\n")
    out.append("#include <etl/string_view.h>\n")

    out.append("struct DDBEntry {\n"
               "    const uint32_t hex;\n"
               "    const etl::string_view reg;\n"
               "};\n\n")


    out.append(f"static constexpr size_t DDB_COUNT = {len(rows)};\n")

    out.append(f"static constexpr DDBEntry __in_flash() DDB_DB[DDB_COUNT] = {{\n")

    for hexcode, dev_type, reg in rows:
        out.append(f'    {{ 0x{hexcode}, etl::string_view("{reg}") }},\n')

    out.append("};\n")
    return "".join(out)

if __name__ == "__main__":
    text = download_csv()
    rows = parse_csv(text)

    # Sort by HEX for fast binary search
    rows.sort(key=lambda x: int(x[0], 16))

    cpp = gen_cpp(rows)
    sys.stdout.write(cpp)
