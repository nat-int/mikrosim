#include "qoi.hpp"

#include "../log/log.hpp"

namespace format::qoi {
	constexpr u8 table_index(color_t col) { return (col[0] * u8(3) + col[1] * u8(5) + col[2] * u8(7) + col[3] * u8(11)) & u8(0x3f); }
	decoder::decoder(const header *header_data) {
		if (header_data->magic != magic) {
			logs::warnln("qoi decoder", "header magic ", header_data->magic, " does not match the qoi format!");
		}
		width = header_data->width;
		height = header_data->height;
		channels = static_cast<channels_count>(header_data->channels);
		colorspace = static_cast<colorspaces>(header_data->colorspace);
		raw_data.resize(4 * width * height);
		last = { 0,0,0,255 };
		i = 0;
		state = 0;
		//logs::debugln("qoi debug", "[HEADER] size: [", width, "; ", height, "] ", u16(header_data->channels), " channels, colorspace ", (header_data->colorspace ? "linear" : "srgb/la"));
	}
	void decoder::decode(const u8 *data, usize nbytes) {
		if (!nbytes)
			return;
		switch (state) {
		case 1: luma(sdata[0], *(data++)); nbytes--; break;
		case 2:
			if (nbytes < 4) {
				std::copy_n(data, nbytes, sdata.begin());
				state += nbytes;
				return;
			}
			rgba(data[0], data[1], data[2], data[3]);
			data += 4;
			nbytes -= 4;
			break;
		case 3:
			if (nbytes < 3) {
				std::copy_n(data, nbytes, sdata.begin() + 1);
				state += nbytes;
				return;
			}
			rgba(sdata[0], data[0], data[1], data[2]);
			data += 3;
			nbytes -= 3;
			break;
		case 4:{
			if (nbytes < 2) {
				sdata[2] = *data;
				state++;
				return;
			}
			rgba(sdata[0], sdata[1], data[0], data[1]);
			data += 2;
			nbytes -= 2;
			break;}
		case 5: rgba(sdata[0], sdata[1], sdata[2], *(data++)); nbytes--; break;
		case 6:
			if (nbytes < 3) {
				std::copy_n(data, nbytes, sdata.begin());
				state += nbytes;
				return;
			}
			rgb(data[0], data[1], data[2]);
			data += 3;
			nbytes -= 3;
			break;
		case 7:{
			if (nbytes < 2) {
				sdata[1] = *data;
				state++;
				return;
			}
			rgb(sdata[0], data[0], data[1]);
			data += 2;
			nbytes -= 2;
			break;}
		case 8: rgb(sdata[0], sdata[1], *(data++)); nbytes--; break;
		case 248: case 249: case 250: case 251: case 252: case 253:
			if (std::any_of(data, data+std::min<usize>(254-state, nbytes), [](u8 x) { return x != 0; })) {
				logs::warnln("qoi decoder", "invalid end marker!");
			}
			if (nbytes < 255-state) {
				state += nbytes;
				return;
			}
			data += 254-state;
			nbytes -= 254-state;
			[[fallthrough]];
		case 254:
			if (*data != 1)
				logs::warnln("qoi decoder", "invalid end marker!");
			if (!--nbytes)
				break;
			[[fallthrough]];
		case 255:
			logs::warnln("qoi decoder", "data continues after end marker");
			return;
		default: break;
		}
		state = 0;
		while (nbytes) {
			if (i >= raw_data.size()) {
				if (std::any_of(data, data+std::min<usize>(7, nbytes), [](u8 x) { return x != 0; })) {
					logs::warnln("qoi decoder", "invalid end marker!");
				}
				if (nbytes < 8) {
					state = 247+nbytes;
					return;
				}
				if (data[7] != 1)
					logs::warnln("qoi decoder", "invalid end marker!");
				if (nbytes > 8) {
					logs::warnln("qoi decoder", "data continues after end marker");
				}
				state = 255;
				return;
			}
			if (*data == 0xff) {
				if (nbytes < 5) {
					if (nbytes > 1)
						std::copy_n(data+1, nbytes-1, sdata.begin());
					state = nbytes + 1;
					return;
				}
				rgba(data[1], data[2], data[3], data[4]);
				data += 5;
				nbytes -= 5;
			} else if (*data == 0xfe) {
				if (nbytes < 4) {
					if (nbytes > 1)
						std::copy_n(data+1, nbytes-1, sdata.begin());
					state = nbytes + 6;
					return;
				}
				rgb(data[1], data[2], data[3]);
				data += 4;
				nbytes -= 4;
			} else {
				switch (*data >> 6) {
				case 0b00: index(*(data++)); nbytes--; break;
				case 0b01: diff(*(data++)); nbytes--; break;
				case 0b10:{
					if (nbytes == 1) {
						state = 1;
						sdata[0] = *data;
						return;
					}
					luma(data[0], data[1]);
					data += 2;
					nbytes -= 2;
					break;}
				case 0b11: run(*(data++)); nbytes--; break;
				}
			}
		}
	}
	void decoder::decode(std::istream &is) {
		constexpr usize buff_size = 4096;
		usize nbytes;
		for (u8 buffer[buff_size]; (nbytes = is.readsome(reinterpret_cast<char *>(buffer), buff_size)) > 0;) {
			decode(buffer, nbytes);
			if (nbytes < buff_size)
				return;
		}
	}
	void decoder::rgb(u8 r, u8 g, u8 b) {
		//logs::debugln("qoi debug", "[RGB] ", u16(r), " ", u16(g), " ", u16(b), " (a=", u16(last[3]), ")");
		last[0] = r;
		last[1] = g;
		last[2] = b;
		push();
		table[table_index(last)] = last;
	}
	void decoder::rgba(u8 r, u8 g, u8 b, u8 a) {
		//logs::debugln("qoi debug", "[RGBA] ", u16(r), " ", u16(g), " ", u16(b), " ", u16(a));
		last = { r,g,b,a };
		push();
		table[table_index(last)] = last;
	}
	void decoder::index(u8 j) {
		last = table[j];
		//logs::debugln("qoi debug", "[INDEX] ", u16(j), " (-> ", u16(last[0]), " ", u16(last[1]), " ", u16(last[2]), " ", u16(last[3]), ")");
		push();
	}
	void decoder::diff(u8 d) {
		const u8 dr = ((d >> 4) & 3) - u8(2);
		const u8 dg = ((d >> 2) & 3) - u8(2);
		const u8 db = (d & 3) - u8(2);
		last[0] += dr;
		last[1] += dg;
		last[2] += db;
		//logs::debugln("qoi debug", "[DIFF] ", u16(dr), " ", u16(dg), " ", u16(db), " (-> ", u16(last[0]), " ", u16(last[1]), " ", u16(last[2]), " ", u16(last[3]), ")");
		push();
		table[table_index(last)] = last;
	}
	void decoder::luma(u8 d, u8 d2) {
		const u8 dg = u8(d & 0x3f) - u8(32);
		const u8 dr = dg + u8(d2 >> 4) - u8(8);
		const u8 db = dg + u8(d2 & 0xf) - u8(8);
		last[0] += dr;
		last[1] += dg;
		last[2] += db;
		//logs::debugln("qoi debug", "[LUMA] ", u16(dr), " ", u16(dg), " ", u16(db), " (-> ", u16(last[0]), " ", u16(last[1]), " ", u16(last[2]), " ", u16(last[3]), ")");
		push();
		table[table_index(last)] = last;
	}
	void decoder::run(u8 l) {
		l = (l & 0x3f) + 1;
		//logs::debugln("qoi debug", "[RUN] ", u16(l), "x (", u16(last[0]), " ", u16(last[1]), " ", u16(last[2]), " ", u16(last[3]), ")");
		if (i/4 + l > raw_data.size()/4) {
			logs::errorln("qoi decoder", "Data array overflow!");
			l = (raw_data.size()-i) / 4;
		}
		for (u8 j = 0; j < l; j++) {
			for (u8 v : last) {
				raw_data[i++] = v;
			}
		}
	}
	void decoder::push() {
		if (i < raw_data.size()) {
			for (u8 v : last) {
				raw_data[i++] = v;
			}
		} else {
			logs::errorln("qoi decoder", "Data array overflow!");
		}
	}
}
