#!/usr/bin/bash
./2d_shader_data.py --raw | ./injector.py --mark=shaders2d "--after=namespace rend {" --verbose ../rendering/2d.cpp
