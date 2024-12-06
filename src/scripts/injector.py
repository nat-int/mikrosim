#!/usr/bin/python3
import sys

cse = {"c", "h", "cpp", "hpp", "cxx", "hxx", "vert", "frag", "comp", "glsl"}
sce = {"py", "sh"}

def phelp():
	print("usage: injector.py [options] <file>")
	print("\tinjects input from stdin into file")
	print("options:")
	print("\t\x1b[1m--after=<line>\x1b[0m insert after line of code, which after trim matches the argument")
	print("\t\x1b[1m--line=<n>\x1b[0m insert \x1b[1mafter\x1b[0m line n")
	print("\t\x1b[1m--mark=<name>\x1b[0m wrap inserted code with mark named <name>, if injection marked with the same mark already exists, it's deleted")
	print("\t\tsupported only for "+", ".join(f"*.{x}" for x in cse)+" files where it inserts c-style comments")
	print("\t\t\t and for "+", ".join(f"*.{x}" for x in sce)+" files wher it inserts one-line comments using '#'")
	print("\t\x1b[1m--verbose -v\x1b[0m show debug output")
	sys.exit(1)

def inject(file, mark, data, mode):
	if mark:
		if '.' in file:
			ext = file[file.rfind('.')+1:]
			if ext in cse:
				mark_fmt = ("/*[INJ_MARK](%s){*/" % mark, "/*}[INJ_MARK](%s)*/" % mark)
				if verbose: print("mark format is /* */ comment")
			elif ext in sce:
				mark_fmt = ("#[INJ_MARK](%s){" % mark, "#}[INJ_MARK](%s)" % mark)
				if verbose: print("mark format is # comment")
			else:
				print(f"Mark for file '{file}' (extension {ext}) is not supported!")
				mark = ""
		else:
			print(f"Mark for file '{file}' is not supported!")
			mark = ""
	with open(file, "r") as f:
		fd = f.readlines()
	line = 0
	if mode[0] == "line":
		line = mode[1]
	elif mode[0] == "after":
		for i, ln in enumerate(fd):
			if ln.strip() == mode[1].strip():
				line = i
				break
		else:
			print(f"error: line to insert after ('{mode[1].strip()}') not found!")
			return
	else:
		print(f"error: unknown mode '{mode[0]}'!")
		return
	if verbose:
		print(f"inserting after line {line}")
	if not 0 <= line < len(fd):
		print(f"line {line} is not present in {file}")
	if mark:
		data = f"{mark_fmt[0]}\n{data}\n{mark_fmt[1]}\n"
	nl = "\n"
	print(f"injecting {data.count(nl)} lines...")
	fd.insert(line+1, data)
	if mark:
		forget = [-1,-1]
		for i, ln in enumerate(fd):
			if ln.strip() == mark_fmt[0]:
				forget[0] = i
			if ln.strip() == mark_fmt[1]:
				forget[1] = i
		sf = [x if x < line else x-1 for x in forget]
		if forget[0] != -1 and forget[1] != -1:
			if verbose: print(f"mark - deleting lines [{sf[0]}; {sf[1]}]")
			if forget[0] > forget[1]:
				print(f"error: mark begin at line {sf[0]} is after mark end at line {sf[1]}")
			else:
				fd = fd[:forget[0]] + fd[forget[1]+1:]
		elif forget[0] == -1 and forget[1] == -1:
			if verbose: print(f"mark not found, no deletion")
		else:
			if forget[0] == -1:
				print(f"warning: found only end mark at line {sf[1]}")
			else:
				print(f"warning: found only begin mark at line {sf[0]}")
	if verbose: print(f"writing result into {file}...")
	with open(file, "w") as f:
		f.write("".join(fd))

def main(argv):
	global verbose
	file = ""
	verbose = False
	inject_mode = ("",)
	mark = ""
	for a in argv:
		if a.startswith('--'):
			if a.startswith("--after="):
				inject_mode = ("after", a[len("--after="):])
			elif a.startswith("--line="):
				inject_mode = ("line", int(a[len("--line="):]))
			elif a.startswith("--mark="):
				mark = a[len("--mark="):]
			elif a == '--verbose':
				verbose = True
			else:
				phelp()
		elif a.startswith('-'):
			for l in a[1:]:
				if l == 'v':
					verbose = True
				else:
					phelp()
		else:
			if file:
				phelp()
			file = a
	if not file or not inject_mode[0]:
		phelp()
	inject(file, mark, sys.stdin.read(), inject_mode)

if __name__ == "__main__":
	main(sys.argv[1:])
