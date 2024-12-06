#!/usr/bin/bash
./font_ascii_atlas_data_qoi.py --arr-for-inj -v --verbose-on-stderr --extra-verbose-small | ./injector.py --mark=ascii_font_atlas "--after=namespace rend {" --verbose ../rendering/text.cpp
