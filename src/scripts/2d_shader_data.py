#!/usr/bin/python3
import os
import sys
from cstr import cstr

tmpf = "./2d_shaders/tmp.spv"
outf = "./2d_shaders/out.cpp"

raw = "--raw" in sys.argv

shaders = {}
for fn in os.listdir("./2d_shaders/"):
	if not fn.endswith(".cpp") and not fn.endswith(".spv"):
		rc = os.system(f"glslc ./2d_shaders/{fn} -o {tmpf}")
		if rc != 0:
			print("error occured during compilation - stopping")
			sys.exit(1)
		with open(tmpf, "rb") as f:
			spv = f.read()
		if not raw:
			print(f"{fn} compiled to {len(spv)} bytes")
		if len(spv) % 4 != 0:
			spv += bytearray([0] * (4 - len(spv) % 4))
		shaders[fn] = spv
os.remove(tmpf)

data = ""
sizedata = ""
named = ""
for i, (fn, spv) in enumerate(shaders.items()):
	data += "\t"+cstr(spv, 160)+",\n"
	sizedata += f"{len(spv)}, "
	named += f"constexpr unsigned int {fn.replace('.', '_')} = {i};\n"
out = "constexpr const char *shader_code[] = {\n" + data + "};\nconstexpr unsigned int shader_sizes[] = { " + sizedata + "};\n" +\
	named + f"constexpr unsigned int shader_count = {len(shaders)};"
if not raw:
	print("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-")
print(out)
