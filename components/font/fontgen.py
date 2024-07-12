import sys
from pathlib import Path

import numpy
import pudb
from skimage import io as skio
import os

fn_fontsheet = "font.png"

if __name__ != '__main__':
    print("Please run this file directly from the command line")
    exit(0)

if len(sys.argv) < 2:
    print("Usage: python fontgen.py [folder_name]\n - Compiles a font sheet into a header for use.\n - folder_name: "
          "What font to compile. Formatted as (name)_WxH/")
    exit(0)

folder = Path(sys.argv[1])
char_size = sys.argv[1].rsplit("_", 1)[1].split("x")  # eg. (16, 24), a font 16 wide by 24 tall
char_size = int(char_size[0]), int(char_size[1])

if not os.path.exists(folder/fn_fontsheet):
    print("Font sheet font.png not found")
    exit(0)

sheet = skio.imread(folder/fn_fontsheet)  # type: numpy.ndarray
print(sheet.shape)
if sheet.ndim > 2:
    print("Font sheet is not in grayscale")
    exit(0)

if sheet.shape[:2] != (char_size[1]*16, char_size[0]*16):
    print("Font sheet does not appear to be 16x16 character grid for "
          f"{char_size} size characters (a {char_size[0]*16}x{char_size[1]*16} px file)")
    exit(0)


output = numpy.ndarray((256, char_size[0]*char_size[1]), dtype=numpy.uint8)

for char_n in range(256):
    char_x = (char_n % 16) * char_size[0]
    char_y = (char_n // 16) * char_size[1]
    for y in range(char_size[1]):
        for x in range(char_size[0]):
            # print(f"Character {char_n}(at {char_x}, {char_y}) at pixel ({x},{y}) has value {sheet[char_y+y, char_x+x]}")
            output[char_n, (x + y*char_size[0])] = sheet[char_y+y, char_x+x]

outfile = f"""
#include <stdint.h>

const static uint8_t font_size_x = {char_size[0]};
const static uint8_t font_size_y = {char_size[1]};

const static uint8_t font_sheet[256][{char_size[0]*char_size[1]}] = {{
"""

for char in output:
    outfile += f"{{{', '.join([str(x) for x in char])}}},\n"

outfile += "};\n"

with open(folder/"font.h", "w") as f:
    f.write(outfile)

print("Font generation complete")
