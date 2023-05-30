#include "utf.hpp"

#include <vector>
#include <iostream>


int utf_run_tests();

enum TestAction { BOTH = 3, ENCODE = 1, DECODE = 2 };

struct TestCase {
	TestAction action;
	UTF_TYPE type;
	UTF_BOM bom;
	char32_t codepoint{ 0 };
	std::vector<uint8_t> bytes{ 8 };
	int num_words{ 0 };
	UTF_RESULT result;
};


const std::vector<std::pair<std::string, std::vector<TestCase>>> testGroups = {
	{
		"UTF-8: normal encode/decode",
		std::vector<TestCase> {
			{ BOTH, UTF8, UTF_LE, 0, {0}, 1, UTF_OK },
			{ BOTH, UTF8, UTF_LE, 'A', {'A'}, 1, UTF_OK },
			{ BOTH, UTF8, UTF_LE, U'猫', {0xE7, 0x8C, 0xAB}, 3, UTF_OK },
			{ BOTH, UTF8, UTF_LE, 0x10FFFF, {0xF4, 0x8F, 0xBF, 0xBF}, 4, UTF_OK },
		}
	},
	{
		"UTF-8: errors",
		std::vector<TestCase> {
			{ BOTH, UTF8, UTF_LE, 0xD800, {0xED, 0xA0, 0x80}, 3, UTF_ILLEGAL_CODEPOINT },
			{ BOTH, UTF8, UTF_LE, 0x110000, {0xF4, 0x90, 0x80, 0x80}, 4, UTF_ILLEGAL_CODEPOINT },
			{ BOTH, UTF8, UTF_LE, 0xFFFFFFFF, {0xFE, 0x83, 0xBF, 0xBF, 0xBF, 0xBF, 0xBF}, 7, UTF_ILLEGAL_CODEPOINT },
		}
	},
	{
		"UTF-8: decode errors",
		std::vector<TestCase> {
			{ DECODE, UTF8, UTF_LE, 0, {0x80}, 1, UTF_UNEXPECTED_CONTINUATION },

			{ DECODE, UTF8, UTF_LE, 0, {0xE4, 0x80}, 2, UTF_TOO_FEW_WORDS },

			{ DECODE, UTF8, UTF_LE, 0x20, {0xC0, 0xA0}, 2, UTF_OVERLONG },
			{ DECODE, UTF8, UTF_LE, 0x20, {0xF0, 0x80, 0x80, 0xA0}, 4, UTF_OVERLONG },
		}
	},
	{
		"UTF-16BE: normal encode/decode",
		std::vector<TestCase> {
			{ BOTH, UTF16, UTF_BE, 0, {0, 0}, 1, UTF_OK },
			{ BOTH, UTF16, UTF_BE, 'A', {0, 'A'}, 1, UTF_OK },
			{ BOTH, UTF16, UTF_BE, U'猫', {0x73, 0x2B}, 1, UTF_OK },

			{ BOTH, UTF16, UTF_BE, 0x10FFFF, {0xDB, 0xFF, 0xDF, 0xFF}, 2, UTF_OK },
		}
	},
	{
		"UTF-16LE: normal encode/decode",
		std::vector<TestCase> {
			{ BOTH, UTF16, UTF_LE, 0, {0, 0}, 1, UTF_OK },
			{ BOTH, UTF16, UTF_LE, 'A', {'A', 0}, 1, UTF_OK },
			{ BOTH, UTF16, UTF_LE, U'猫', {0x2B, 0x73}, 1, UTF_OK },

			{ BOTH, UTF16, UTF_LE, 0x10FFFF, {0xFF, 0xDB, 0xFF, 0xDF}, 2, UTF_OK },
		}
	},
	{
		"UTF-16BE: decode errors",
		std::vector<TestCase> {
			{ DECODE, UTF16, UTF_BE, U'猫', {0x73, 0x2B}, 1, UTF_OK },

			{ DECODE, UTF16, UTF_BE, 0, {0xd8, 0x00}, 1, UTF_TOO_FEW_WORDS },
		}
	},
	{
		"UTF-16LE: decode errors",
		std::vector<TestCase> {
			{ DECODE, UTF16, UTF_LE, U'猫', {0x2B, 0x73}, 1, UTF_OK },

			{ DECODE, UTF16, UTF_LE, 0, {0x00, 0xd8}, 1, UTF_TOO_FEW_WORDS },
		}
	},
	{
		"UTF-32",
		std::vector<TestCase> {
			{ BOTH, UTF32, UTF_BE, 0x0, {0x0, 0x0, 0x0, 0x0}, 1, UTF_OK },
			{BOTH, UTF32, UTF_BE, 0x10FFFF, {0x0, 0x10, 0xFF, 0xFF}, 1, UTF_OK},

			{BOTH, UTF32, UTF_LE, 0x0, {0x0, 0x0, 0x0, 0x0}, 1, UTF_OK},
			{BOTH, UTF32, UTF_LE, 0x10FFFF, {0xFF, 0xFF, 0x10, 0x00}, 1, UTF_OK},
		}
	}
};

std::vector<std::pair<TestCase, UTF_Point>> fails;

bool compare(const TestCase &tc, UTF_Point &p) {
	bool good = true;

	if(tc.result != p.result) good = false;

	if(p.codepoint != tc.codepoint || p.num_words != tc.num_words || p.num_bytes != tc.bytes.size())
		good = false;

	for(size_t i = 0; i < tc.bytes.size(); i++)
		if(p.bytes[i] != tc.bytes[i]) good = false;

	return good;
}

const char *strResult(UTF_RESULT r) {
	switch(r) {
		case UTF_OK: return "ok";
		case UTF_UNEXPECTED_CONTINUATION: return "unexpected continuation byte";
		case UTF_TOO_FEW_WORDS: return "too few words";
		case UTF_OVERLONG: return "overlong";
		case UTF_ILLEGAL_CODEPOINT: return "invalid codepoint";
		default: return "huh?";
	}
}

const char *strType(UTF_TYPE t) {
	switch(t) {
		case UTF8: return "utf-8";
		case UTF16: return "utf-16";
		case UTF32: return "utf-32";
		default: return "-";
	}
}

const char *strBom(UTF_BOM b) {
	switch(b) {
		case UTF_LE: return "[lit en]";
		case UTF_BE: return "[big en]";
		default: return "-";
	}
}

const char *st_ok = "\x1B[92mok\x1B[m";
const char *st_fail = "\x1B[91mfail\x1B[m";
const char *st_skip = "\x1B[mskip\x1B[m";
#define LOG(x, ...) printf(x "\n", ## __VA_ARGS__)

void print_fail(const TestCase &tc, const UTF_Point &p) {
	printf("|    expected: (%s)", strResult(tc.result));
	printf("\n|      0x%.6X - %s %s", static_cast<unsigned>(tc.codepoint), strType(tc.type), strBom(tc.bom));
	printf("\n|      words: %i; bytes:", tc.num_words);
	for(auto &b : tc.bytes) printf(" 0x%.2X", b);
	printf("\n|");
	printf("\n|    got: (%s)", strResult(p.result));
	printf("\n|      0x%.6X - %s %s", static_cast<unsigned>(p.codepoint), strType(p.type), strBom(p.bom));
	printf("\n|      words: %i; bytes:", p.num_words);
	for(int i = 0; i < p.num_bytes; i++) printf(" 0x%.2X", p.bytes[i]);

	printf("\n");
}

int utf_run_tests() {
	int nerrors = 0;

	LOG("=== utf run tests ===");


	for(auto &group : testGroups) {

		LOG("\n\n[%s] (%zu)", group.first.c_str(), group.second.size());

		int i = 1;
		for(auto &tc : group.second) {
			LOG("|\n| %i:", i++);
			bool res_en = true, res_de = true;
			UTF_Point p_en, p_de;

			if(tc.action & 1) {
				p_en = utf_encode(tc.codepoint, tc.type, tc.bom);
				res_en = compare(tc, p_en);
			}
			if(tc.action & 2) {
				p_de = utf_decode(tc.bytes.data(), tc.type, tc.bom);
				res_de = compare(tc, p_de);
			}

			LOG("|  encode: %s", tc.action & 1 ? (res_en ? st_ok : st_fail) : st_skip);
			if(!res_en) {
				nerrors++;
				print_fail(tc, p_en);
				LOG("|");
			}

			LOG("|  decode: %s", tc.action & 2 ? (res_de ? st_ok : st_fail) : st_skip);
			if(!res_de) {
				nerrors++;
				print_fail(tc, p_de);
			}
		}
	}

	if(nerrors)
		LOG("\n== %i error(s) == ", nerrors);
	else LOG("\n== no errors ==");

	return nerrors;
}