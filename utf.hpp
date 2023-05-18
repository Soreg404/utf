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


UTF_API UTF_Point utf8_encode(char32_t codepoint);

UTF_API UTF_Point utf16_encode(char32_t codepoint, UTF_BOM en);
UTF_API UTF_Point utf16BE_encode(char32_t codepoint);
UTF_API UTF_Point utf16LE_encode(char32_t codepoint);

UTF_API UTF_Point utf32_encode(char32_t codepoint, UTF_BOM en);
UTF_API UTF_Point utf32BE_encode(char32_t codepoint);
UTF_API UTF_Point utf32LE_encode(char32_t codepoint);


UTF_API inline UTF_RESULT utf_is_valid(char32_t codepoint) {
	return (codepoint <= 0x10FFFF && ((codepoint & 0xFC00) != 0xD800)) ? UTF_OK : UTF_ILLEGAL_CODEPOINT;
}



#ifdef UTF_IMPLEMENTATION

UTF_API UTF_Point utf8_decode(const void *s) {
	const char *stream_beg = static_cast<const char *>(s);
	UTF_Point ret;
	auto set_bytes = [&](uint8_t num_words) {
		ret.num_bytes = ret.num_words = num_words;
		memcpy(ret.bytes, stream_beg, num_words);
	};

	switch(*stream_beg & 0xC0) {
		case 0x00:
		case 0x40:
			// ascii
			ret.codepoint = static_cast<char32_t>(*stream_beg);
			goto return_one_word;
		case 0x80:
			ret.result = UTF_UNEXPECTED_CONTINUATION;
		return_one_word:
			set_bytes(1);
			return ret;
	}

	uint8_t expWords = 2;
	for(; expWords < 7 && (*stream_beg << expWords) & 0x80; expWords++);

	size_t data = *stream_beg & (0xff >> (expWords + 1));

	// TODO: detect overlong

	for(uint8_t i = 1; i < expWords; i++) {
		if(stream_beg[i] == 0 || (stream_beg[i] & 0xC0) != 0x80) {
			ret.result = UTF_TOO_FEW_WORDS;
			set_bytes(i);
			return ret;
		}
		data <<= 6;
		data |= stream_beg[i] & 0x3f;
	}

	ret.codepoint = static_cast<char32_t>(data);
	ret.result = utf_is_valid(ret.codepoint);
	
	set_bytes(expWords);

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
			} else {
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
	ret.result = utf_is_valid(ret.codepoint);
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
	ret.result = utf_is_valid(codepoint);

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
	ret.result = utf_is_valid(codepoint);

	wchar_t W[2];
	if(codepoint < 0x10000) {
		ret.num_words = 1;
		W[0] = codepoint & 0xffff;
	} else {
		ret.num_words = 2;
		codepoint -= 0x10000;
		W[0] = 0xD800 + (codepoint >> 10);
		W[1] = 0xDC00 + (codepoint & 0x3ff);
	}

	ret.num_bytes = ret.num_words * 2;
	for(int i = 0; i < 2; i++) {
		uint8_t hibyte = W[i] >> 8, lobyte = W[i] & 0xff;
		ret.bytes[i * 2]		= (en == UTF_BE) ? hibyte : lobyte;
		ret.bytes[i * 2 + 1]	= (en == UTF_BE) ? lobyte : hibyte;
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
	ret.result = utf_is_valid(codepoint);
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