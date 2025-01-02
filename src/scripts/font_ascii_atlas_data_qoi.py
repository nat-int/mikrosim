#!/usr/bin/python3
import sys
import struct
from PIL import Image
from cstr import cstr

file = "./font/fira_mono.png"
font_csv = "./font/fira_mono.csv"

with Image.open(file) as img:
	data = list(img.getdata())
	width, height = img.size

ver = "-v" in sys.argv[1:]
uver = "--extra-verbose" in sys.argv[1:]
ver_se = "--verbose-on-stderr" in sys.argv[1:]
uver_lim = "--extra-verbose-small" in sys.argv[1:]
out = bytearray()

if ver_se:
	_print = print
	def eprint(*args):
		sys.stderr.write(" ".join(map(str, args))+"\n")
	print = eprint

out += b"qoif"
out += width.to_bytes(4, "little", signed=False) + height.to_bytes(4, "little", signed=False)
out += len(data[0]).to_bytes(1, "little", signed=False)
out += b"\x01"
if uver:
	print("header:")
	print("\tmagic: \"qoif\"")
	print(f"\tsize: [{width}; {height}]")
	print(f"\tchannels: {len(data[0])}")
	print("\tcolorspace: linear")

last = (0,0,0,255)
table = [None] * 64
idx = lambda r,g,b,a: (r*3+g*5+b*7+a*11) % 64
rle = 0
dccs = {"run":0,"diff":0,"idx":0,"luma":0,"rgb":0,"rgba":0}
for i,(r,g,b,a) in enumerate(data):
	if uver_lim:
		uver = i < 400
	lr,lg,lb,la = last
	dr,dg,db,da = [(x-y) % 256 for x,y in zip((r,g,b,a), last)]
	dgr,dgb = (dr-dg)%256, (db-dg)%256
	if all(x == 0 for x in (dr,dg,db,da)):
		rle += 1
		if rle == 62 or i == len(data) - 1:
			out += bytearray([0b11 << 6 | (rle-1)])
			if uver: print(f"[RUN] {rle}")
			rle = 0
			dccs["run"] += 1
		continue
	elif rle > 0:
		out += bytearray([0b11 << 6 | (rle-1)])
		if uver: print(f"[RUN] {rle}")
		rle = 0
		dccs["run"] += 1
	if da == 0 and all(x < 2 or x >= 254 for x in (dr,dg,db)):
		o = 0b01 << 6 | ((dr + 2) % 256) << 4 | ((dg + 2) % 256) << 2 | ((db + 2) % 256)
		out += bytearray([o])
		if uver: print(f"[DIFF] ({dr}; {dg}; {db})")
		dccs["diff"] += 1
	elif table[idx(r,g,b,a)] == (r,g,b,a):
		out += bytearray([idx(r,g,b,a)])
		if uver: print(f"[IDX] {idx(r,g,b,a)} (-> ({r}; {g}; {b}; {a}))")
		dccs["idx"] += 1
	elif da == 0 and (dgr < 8 or dgr >= 248) and (dg < 32 or dg >= 224) and (dgb < 8 or dgb >= 248):
		out += bytearray([0x80 | ((dg+32) % 256), ((dgr+8) % 256) << 4 | ((dgb+8) % 256)])
		if uver: print(f"[LUMA] ({dg} ~ {dgr}; {dgb}) (-> ({dr}; {dg}; {db}))")
		dccs["luma"] += 1
	elif da == 0:
		out += b"\xfe" + bytearray([r,g,b])
		if uver: print(f"[RGB] ({r}; {g}; {b})")
		dccs["rgb"] += 1
	else:
		out += b"\xff" + bytearray([r,g,b,a])
		if uver: print(f"[RGBA] ({r}; {g}; {b}; {a})")
		dccs["rgba"] += 1
	last = (r,g,b,a)
	table[idx(r,g,b,a)] = last

out += b"\0\0\0\0\0\0\0\x01"
if uver_lim and not uver:
	print("...")

if ver:
	import os
	raw = width*height*4
	png = os.path.getsize(file)
	qoi = len(out)
	print("-------- COMPRESSION STATS -----------")
	print(f"image size: [{width}; {height}]")
	print(f"raw data size:   {raw:>8} bytes")
	print(f"source png size: {png:>8} bytes")
	print(f"qoi size:        {qoi:>8} bytes")
	print(f"qoi compression ratio: ~{100*qoi // raw:>3}%")
	print(f"png compression ratio: ~{100*png // raw:>3}%")
	print(f"png/qoi compression ratio: ~{100*png // qoi:>3}%")
	print(f"qoi/png compression ratio: ~{100*qoi // png:>3}%")
	print("data chunk counts:")
	tc = sum(dccs.values())
	for dc,c in dccs.items():
		print(f"\t{dc:4}: {c:>5} ~{100*c//tc:>2}%")
	print(f"\ttot : {tc:>5} ~100%")

chars = [None] * 128
with open(font_csv, "r") as f:
	csvd = [x.strip() for x in f.read().split('\n') if x.strip()]
def remap(l,b,r,t):
	return (l, t, r, b)
asc, dsc = 0.935,-0.265
for ln in csvd:
	ln = [float(x) if '.' in x else int(x) for x in ln.split(',')]
	c, adv, bounds, uv = ln[0], ln[1], remap(*ln[2:6]), remap(*ln[6:10])
	bounds = (bounds[0], asc - bounds[1], bounds[2], asc - bounds[3])
	uv = (uv[0]/width, 1-uv[1]/height, uv[2]/width, 1-uv[3]/height)
	chars[c] = (tuple(uv), tuple(bounds), adv)
sw = chars[ord(' ')][2]
chars[ord('\t')] = ((0,0,0,0), (0,0,0,0), 4*sw)
chars[ord('\n')] = ((0,0,0,0), (0,0,0,0), 0)
chars[ord('\r')] = ((0,0,0,0), (0,0,0,0), 0)
chars[ord('\0')] = ((0,0,0,0), (0,0,0,0), 0)
do = ""
for i in range(128):
	if chars[i] is None:
		chars[i] = chars[ord('?')]
		do += "."
	else:
		do += "#"
if ver: print(do)
ciout = bytearray()
for i in range(128):
	ciout += struct.pack("9f", *chars[i][0], *chars[i][1], chars[i][2])

print = _print
if "--arr-for-inj" in sys.argv[1:]:
	# where std::embed? :(
	if len(out) >= 65536: # MSVC, why are you like this :(
		print("static constexpr u8 ascii_font_atlas[] = {")
		buff = "\t"
		for b in out:
			buff += f"{b},"
			if len(buff) >= 160:
				print(buff)
				buff = "\t"
		if len(buff) > 1:
			print(buff)
		print("};")
		print(f"static constexpr usize ascii_font_atlas_bytes = {len(out)};")
		print("#define ascii_font_atlas_header reinterpret_cast<const format::qoi::header *>(ascii_font_atlas)")
		print("#define ascii_font_atlas_data ascii_font_atlas+14")
	else:
		print("static constexpr const char *ascii_font_atlas_str =")
		print("\t"+cstr(out, 160)+";")
		print("#define ascii_font_atlas reinterpret_cast<const u8 *>(ascii_font_atlas_str)")
		print(f"static constexpr usize ascii_font_atlas_bytes = {len(out)};")
		print("#define ascii_font_atlas_header reinterpret_cast<const format::qoi::header *>(ascii_font_atlas_str)")
		print("#define ascii_font_atlas_data reinterpret_cast<const u8 *>(ascii_font_atlas_str+14)")
	print(f"static constexpr usize ascii_font_atlas_data_bytes = {len(out)-14};")
	print("static constexpr const char *ascii_char_info =")
	print("\t"+cstr(ciout, 160)+";")
	print(f"static constexpr usize ascii_char_info_count = 128;",end="")
