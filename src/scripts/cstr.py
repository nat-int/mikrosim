
def cstr(b, bl):
	if b[-1] == 0:
		b = b[:-1]
	conv = { "\t":"t", "\n":"n", "\v":"v", "\f":"f" }
	o = [""]
	last_was_oct = False
	for c in b:
		is_oct = False
		if len(o[-1]) >= bl:
			o.append("")
		if chr(c) in "\t\n\v\f\\\"":
			o[-1] += "\\" + (conv[chr(c)] if chr(c) in conv else chr(c))
		elif c < 32 or c >= 127:
			o[-1] += f"\\{c:o}"
			is_oct = True
		else:
			if chr(c) in "01234567" and last_was_oct:
				o[-1] += f"\\{c:o}"
				is_oct = True
			else:
				o[-1] += chr(c)
		last_was_oct = is_oct
	if len(o[-1]) < 5 and len(o) > 1:
		o[-2] += o[-1]
		o.pop()
	return '"'+"\"\n\t\"".join(o)+'"'
