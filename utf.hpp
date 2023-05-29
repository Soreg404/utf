/*

Copyright (c) 2023 Stanisław Darowski

This project is licensed under the terms of the MIT license.
Full license is at the end of file.

*/

#pragma once
#ifndef _UTF_H
#define _UTF_H
#include <stdint.h>
#include <string.h>

#define UTF_API extern "C"

enum UTF_TYPE {
	UTF8,
	UTF16,
	UTF32,
};

enum UTF_BOM {
	UTF_BE,
	UTF_LE
};

enum UTF_RESULT {
	UTF_OK,
	UTF_UNEXPECTED_CONTINUATION,
	UTF_TOO_FEW_WORDS,
	UTF_OVERLONG,
	UTF_ILLEGAL_CODEPOINT
};


struct UTF_Point {
	char32_t codepoint = 0;
	UTF_TYPE type{ UTF8 };
	UTF_BOM bom{ UTF_LE };
	UTF_RESULT result{ UTF_OK };
	uint8_t num_words = 0, num_bytes = 0;
	uint8_t bytes[8]{ 0 };
};

UTF_API UTF_Point utf8_decode(const void *stream_beg);

UTF_API UTF_Point utf16_decode(const void *stream_beg, UTF_BOM en);
UTF_API UTF_Point utf16BE_decode(const void *stream_beg);
UTF_API UTF_Point utf16LE_decode(const void *stream_beg);

UTF_API UTF_Point utf32_decode(const void *stream_beg, UTF_BOM en);
UTF_API UTF_Point utf32LE_decode(const void *stream_beg);
UTF_API UTF_Point utf32BE_decode(const void *stream_beg);

UTF_API inline UTF_Point utf_decode(const void *stream_beg, UTF_TYPE type, UTF_BOM bom) {
	switch(type) {
		case UTF8: return utf8_decode(stream_beg);
		case UTF16: return utf16_decode(stream_beg, bom);
		case UTF32: return utf32_decode(stream_beg, bom);
		default: return {};
	}
}


UTF_API UTF_Point utf8_encode(char32_t codepoint);

UTF_API UTF_Point utf16_encode(char32_t codepoint, UTF_BOM en);
UTF_API UTF_Point utf16BE_encode(char32_t codepoint);
UTF_API UTF_Point utf16LE_encode(char32_t codepoint);

UTF_API UTF_Point utf32_encode(char32_t codepoint, UTF_BOM en);
UTF_API UTF_Point utf32BE_encode(char32_t codepoint);
UTF_API UTF_Point utf32LE_encode(char32_t codepoint);

UTF_API inline UTF_Point utf_encode(char32_t codepoint, UTF_TYPE type, UTF_BOM bom) {
	switch(type) {
		case UTF8: return utf8_encode(codepoint);
		case UTF16: return utf16_encode(codepoint, bom);
		case UTF32: return utf32_encode(codepoint, bom);
		default: return {};
	}
}


UTF_API inline bool utf_is_valid_cp(char32_t codepoint) {
	return codepoint <= 0x10FFFF && ((codepoint & 0xFC00) != 0xD800);
}



#ifdef UTF_IMPLEMENTATION

UTF_API UTF_Point utf8_decode(const void *s) {
	const char *stream_beg = static_cast<const char *>(s);
	UTF_Point ret;
	UTF_Point *debug_pointer_cuz_vs_sucks = &ret;

	switch(*stream_beg & 0xC0) {
		case 0x00:
		case 0x40:
			// ascii
			ret.num_words = 1;
			ret.codepoint = static_cast<char32_t>(*stream_beg);
			break;
		case 0x80:
			ret.result = UTF_UNEXPECTED_CONTINUATION;
			ret.num_words = 1;
			ret.codepoint = 0;
			break;
		default:
		{
			uint8_t expWords = 2;
			for(; expWords < 7 && (*stream_beg << expWords) & 0x80; expWords++);

			ret.codepoint = *stream_beg & (0xff >> (expWords + 1));

			for(ret.num_words = 1; ret.num_words < expWords; ret.num_words++) {
				if(stream_beg[ret.num_words] == 0 || (stream_beg[ret.num_words] & 0xC0) != 0x80) {
					ret.codepoint = 0;
					ret.result = UTF_TOO_FEW_WORDS;
					break;
				}
				ret.codepoint <<= 6;
				ret.codepoint |= stream_beg[ret.num_words] & 0x3f;
			}

			if(ret.num_words == 1 || ret.result == UTF_TOO_FEW_WORDS) break;

			// detect overlong
			// idea: if you can fit the codepoint in one less word - then its overlong
			
			// total available bits for one less word
			size_t n = ret.num_words - 1;
			size_t avl_bits = n == 1 ? 7 : (6 * (n - 1) + (7 - n));

			size_t overlong_mask = (~static_cast<size_t>(0)) << avl_bits;
			if((ret.codepoint & overlong_mask) == 0) ret.result = UTF_OVERLONG;

			else if(!utf_is_valid_cp(ret.codepoint)) ret.result = UTF_ILLEGAL_CODEPOINT;
		}
	}

	ret.num_bytes = ret.num_words;
	memcpy(ret.bytes, stream_beg, ret.num_bytes);

	return ret;
}


UTF_API UTF_Point utf16_decode(const void *s, UTF_BOM en) {
	const uint8_t *stream_beg = static_cast<const uint8_t *>(s);
	UTF_Point ret;
	UTF_Point *debug_pointer_cuz_vs_sucks = &ret;
	ret.type = UTF16;
	ret.bom = en;

	wchar_t W[2];
	bool is_be = en == UTF_BE;
	for(int i = 0; i < 2; i++) {
		W[i] = (static_cast<wchar_t>(stream_beg[i * 2 + !is_be]) << 8) | static_cast<wchar_t>(stream_beg[i * 2 + is_be]);
	}

	switch(W[0] & 0xfc00) {
		case 0xdc00: // low surrogate (invalid)
			ret.result = UTF_UNEXPECTED_CONTINUATION;
			ret.num_words = 1;
			break;
		case 0xd800: // high surrogate
			if((W[1] & 0xfc00) != 0xdc00) {
				ret.result = UTF_TOO_FEW_WORDS;
				ret.num_words = 1;
			}
			else {
				ret.num_words = 2;
				ret.codepoint = ((W[0] & 0x3ff) << 10) + (W[1] & 0x3ff) + 0x10000;
			}
			break;
		default:
			ret.codepoint = static_cast<char32_t>(W[0]);
			ret.num_words = 1;
	}

	ret.num_bytes = 2 * ret.num_words;
	memcpy(ret.bytes, stream_beg, ret.num_bytes);

	return ret;
}

UTF_API UTF_Point utf16BE_decode(const void *s) {
	return utf16_decode(s, UTF_BE);
}

UTF_API UTF_Point utf16LE_decode(const void *s) {
	return utf16_decode(s, UTF_LE);
}


UTF_API UTF_Point utf32_decode(const void *s, UTF_BOM en) {
	const uint8_t *stream_beg = static_cast<const uint8_t *>(s);
	UTF_Point ret;
	UTF_Point *debug_pointer_cuz_vs_sucks = &ret;
	ret.type = UTF32;
	ret.bom = en;
	ret.num_words = 1;
	ret.num_bytes = 4;
	memcpy(ret.bytes, stream_beg, ret.num_bytes);
	for(int i = 0; i < 4; i++)
		ret.codepoint |= static_cast<char32_t>(stream_beg[en == UTF_BE ? 3 - i : i]) << i * 8;
	ret.result = utf_is_valid_cp(ret.codepoint) ? UTF_OK : UTF_ILLEGAL_CODEPOINT;
	return ret;
}

UTF_API UTF_Point utf32BE_decode(const void *s) {
	return utf32_decode(s, UTF_BE);
}

UTF_API UTF_Point utf32LE_decode(const void *s) {
	return utf32_decode(s, UTF_LE);
}



UTF_API UTF_Point utf8_encode(char32_t codepoint) {
	UTF_Point ret;
	ret.codepoint = codepoint;
	ret.result = utf_is_valid_cp(codepoint) ? UTF_OK : UTF_ILLEGAL_CODEPOINT;

	if(codepoint < 0x80) {
		ret.num_bytes = ret.num_words = 1;
		ret.bytes[0] = static_cast<char>(codepoint);
		return ret;
	}

	uint8_t words[8]{ 0 };
	uint8_t num_words = 0;
	uint8_t word_val = 0;
	char32_t header_bits = 0x1f;

	for(;;) {
		word_val = codepoint & 0x3f;
		codepoint >>= 6;
		words[7 - num_words] = word_val | 0x80;
		num_words++;

		if((codepoint & ~header_bits) == 0) {
			word_val = static_cast<uint8_t>(0xff << (7 - num_words)) | (codepoint & header_bits);
			words[7 - num_words] = word_val;
			num_words++;
			break;
		}

		header_bits >>= 1;
	}

	ret.num_bytes = ret.num_words = num_words;
	memcpy(ret.bytes, words + 8 - ret.num_bytes, ret.num_bytes);

	return ret;
}


UTF_API UTF_Point utf16_encode(char32_t codepoint, UTF_BOM en) {
	UTF_Point ret;
	UTF_Point *debug_pointer_cuz_vs_sucks = &ret;
	ret.codepoint = codepoint;
	ret.type = UTF16;
	ret.bom = en;
	ret.result = utf_is_valid_cp(codepoint) ? UTF_OK : UTF_ILLEGAL_CODEPOINT;

	wchar_t W[2];
	if(codepoint < 0x10000) {
		ret.num_words = 1;
		W[0] = codepoint & 0xffff;
	}
	else {
		ret.num_words = 2;
		codepoint -= 0x10000;
		W[0] = 0xD800 + (codepoint >> 10);
		W[1] = 0xDC00 + (codepoint & 0x3ff);
	}

	ret.num_bytes = ret.num_words * 2;
	for(int i = 0; i < 2; i++) {
		uint8_t hibyte = W[i] >> 8, lobyte = W[i] & 0xff;
		ret.bytes[i * 2] = (en == UTF_BE) ? hibyte : lobyte;
		ret.bytes[i * 2 + 1] = (en == UTF_BE) ? lobyte : hibyte;
	}
	return ret;
}

UTF_API UTF_Point utf16BE_encode(char32_t codepoint) {
	return utf16_encode(codepoint, UTF_BE);
}

UTF_API UTF_Point utf16LE_encode(char32_t codepoint) {
	return utf16_encode(codepoint, UTF_LE);
}


UTF_API UTF_Point utf32_encode(char32_t codepoint, UTF_BOM en) {
	UTF_Point ret;
	UTF_Point *debug_pointer_cuz_vs_sucks = &ret;
	ret.codepoint = codepoint;
	ret.type = UTF32;
	ret.bom = en;
	ret.num_words = 1;
	ret.num_bytes = 4;
	ret.result = utf_is_valid_cp(codepoint) ? UTF_OK : UTF_ILLEGAL_CODEPOINT;
	for(int i = 0; i < 4; i++)
		ret.bytes[en == UTF_BE ? i : 3 - i] = static_cast<uint8_t>(codepoint >> i * 8);
	return ret;
}

UTF_API UTF_Point utf32BE_encode(char32_t codepoint) {
	return utf32_encode(codepoint, UTF_BE);
}

UTF_API UTF_Point utf32LE_encode(char32_t codepoint) {
	return utf32_encode(codepoint, UTF_LE);
}

#endif // UTF_IMPLEMENTATION

#endif // _UTF_H


/*

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/