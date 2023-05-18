#define UTF_IMPLEMENTATION
#include "utf.hpp"
#include <iostream>
#include <conio.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

struct ConInfo {
	COORD cur, size;
};

COORD baseCurPos;
ConInfo getCurPos() {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	return { csbi.dwCursorPosition , csbi.dwSize };
}

void cls() {
	auto info = getCurPos();
	DWORD ncw = 0;
	FillConsoleOutputCharacterA(hConsole, ' ', (info.cur.Y - baseCurPos.Y) * info.size.X + info.cur.X, baseCurPos, &ncw);
}

void clfw(int n) {
	DWORD ncw = 0;
	FillConsoleOutputCharacterA(hConsole, ' ', n, getCurPos().cur, &ncw);
}

uint8_t codepoint_buffer[4]{ 0 };
void print_codepoint(char32_t codepoint);

uint8_t bytes_buffer[8]{ 0 };
void print_bytes();


const char *utf_result_str(UTF_RESULT r) {
	switch(r) {
		case UTF_OK: return "ok";
		case UTF_UNEXPECTED_CONTINUATION: return "unexpected continuation byte";
		case UTF_TOO_FEW_WORDS: return "too few words";
		case UTF_OVERLONG: return "overlong";
		case UTF_ILLEGAL_CODEPOINT: return "invalid codepoint";
		default: return "huh?";
	}
}


int main() {

	SetConsoleOutputCP(65001);

	baseCurPos = getCurPos().cur;
	{
		CONSOLE_CURSOR_INFO ci;
		GetConsoleCursorInfo(hConsole, &ci);
		ci.bVisible = false;
		SetConsoleCursorInfo(hConsole, &ci);
	}

	int mode = 0;
	int inp_pos_cp = 0, inp_pos_by = 0;
	bool inp_first_half = true;

	auto inp_move = [&](bool incr) {
		int *inp_ptr = mode == 0 ? &inp_pos_cp : &inp_pos_by;
		if(incr)
			*inp_ptr < (mode == 0 ? 3 : 7) ? ++(*inp_ptr) : 0;
		else
			*inp_ptr > 0 ? --(*inp_ptr) : 0;

		inp_first_half = true;
	};

	for(;;) {

		SetConsoleCursorPosition(hConsole, baseCurPos);

		char32_t currCodepoint = 0;
		for(int i = 0; i < 4; i++)
			currCodepoint |= static_cast<char32_t>(codepoint_buffer[i]) << (3 - i) * 8;

		if(mode == 0)
			print_codepoint(currCodepoint);
		else
			print_bytes();


		SetConsoleCursorPosition(hConsole, { baseCurPos.X, (short)(baseCurPos.Y + 20) });


		auto print_byte = [&](bool isMode, bool isPos, unsigned byte) {
			printf("%c%.2X%c", isPos ? (isMode ? '<' : '.') : ' ', byte, isPos && isMode ? '>' : ' ');
		};

		printf(" %s codepoint: 0x", mode == 0 ? "->" : "  ");
		for(int i = 0; i < 4; i++) {
			print_byte(mode == 0, inp_pos_cp == i, static_cast<unsigned>(codepoint_buffer[i]));
		}

		printf("\n\n");

		printf(" %s bytes: 0x", mode == 1 ? "->" : "  ");
		for(int i = 0; i < 8; i++) {
			print_byte(mode == 1, inp_pos_by == i, static_cast<unsigned>(bytes_buffer[i]));
		}

		printf(u8"\n\n ↵ enter to switch\n\n");

		int c = _getch();
		if(c == 0 || c == 0xE0)
			c = c << 8 | _getch();
		//cls();
		printf("\n\n%.2X %.2X\n", c >> 8, c & 0xFF);

		uint8_t *edit_byte = mode == 0 ? &codepoint_buffer[inp_pos_cp] : &bytes_buffer[inp_pos_by];

		if((c >= '0' && c <= '9') || ((c & 0x5F) >= 'A' && (c & 0x5F) <= 'F')) {
			uint8_t inp_byte = 0;
			if(c >= '0' && c <= '9') inp_byte = c - '0';
			else inp_byte = (c & 0x5F) - 'A' + 10;

			if(inp_first_half) {
				*edit_byte = (inp_byte << 4) | (*edit_byte & 0x0F);
				inp_first_half = false;
			}
			else {
				*edit_byte = (*edit_byte & 0xF0) | inp_byte;
				inp_move(true);
			}
			continue;
		}

		switch(c) {
			case 8: //bcksp
				*edit_byte = 0;
				inp_first_half = true;
				break;

			case 13: //enter
				cls();
				mode = mode == 0 ? 1 : 0;
				break;

			case 0x2B:
			case 0x3D: //plus
				if(mode != 0) break;
				currCodepoint++;
				for(int i = 0; i < 4; i++)
					codepoint_buffer[i] = static_cast<uint8_t>(currCodepoint >> (3 - i) * 8);
				break;

			case 0x2D:
			case 0x5F: //minus
				if(mode != 0) break;
				currCodepoint--;
				for(int i = 0; i < 4; i++)
					codepoint_buffer[i] = static_cast<uint8_t>(currCodepoint >> (3 - i) * 8);
				break;

			case 0xE04B: //left
				inp_move(false);
				break;

			case 0xE04D: //right
				inp_move(true);
				break;

			case 0xE048: //up
				if(*edit_byte != 0xFF)
					(*edit_byte)++;
				break;

			case 0xE050: //down
				if(*edit_byte != 0)
					(*edit_byte)--;
				break;

			case 0xE053: //del
				*edit_byte = 0;
				inp_move(true);
				break;

			case 0xE047: //home
				mode == 0 ? inp_pos_cp = 0 : inp_pos_by = 0;
				break;

			case 0xE04F: //end
				mode == 0 ? inp_pos_cp = 3 : inp_pos_by = 7;
				break;

			case 0x1B: goto bye; //esc
		}

	}
bye:
	printf("\nbye\n");

	return 0;
}


void print_codepoint(char32_t currCodepoint) {

	UTF_Point u8 = utf8_encode(currCodepoint),
		u16be = utf16BE_encode(currCodepoint),
		u16le = utf16LE_encode(currCodepoint),
		u32be = utf32BE_encode(currCodepoint),
		u32le = utf32LE_encode(currCodepoint);

	if(currCodepoint < 32 || u8.result == UTF_ILLEGAL_CODEPOINT)
		printf("\n [?] %s", u8.result == UTF_ILLEGAL_CODEPOINT ? "(illegal codepoint)" : "");
	else
		printf("\n [%.*s]", u8.num_bytes, u8.bytes);
	clfw(40);
	printf("\n\n");

	auto print_point = [](const char *label, const UTF_Point *p) {
		printf(" %s\n  ", label);
		for(int i = 0; i < p->num_bytes; i++)
			printf("%.2X ", static_cast<unsigned>(p->bytes[i]));
		clfw(30);
		printf("\n\n");
	};

	print_point("utf8", &u8);
	print_point("utf16BE", &u16be);
	print_point("utf16LE", &u16le);
	print_point("utf32BE", &u32be);
	print_point("utf32LE", &u32le);

}

void print_bytes() {
	UTF_Point u8 = utf8_decode(bytes_buffer),
		u16be = utf16BE_decode(bytes_buffer),
		u16le = utf16LE_decode(bytes_buffer),
		u32be = utf32BE_decode(bytes_buffer),
		u32le = utf32LE_decode(bytes_buffer);

	printf("\n\n\n");

	auto print_point = [](const char *label, const UTF_Point *p) {
		printf(" %s: %s", label, utf_result_str(p->result));
		clfw(50);
		printf("\n");
		if(p->codepoint < 0x20 || p->result == UTF_ILLEGAL_CODEPOINT)
			printf("  0x%.8X  [?]", p->codepoint);
		else {
			UTF_Point convu8 = *p;
			if(p->type != UTF8)
				convu8 = utf8_encode(p->codepoint);
			printf("  0x%.8X  [%.*s]", p->codepoint, convu8.num_bytes, convu8.bytes);
		}
		clfw(10);
		printf("\n\n");
	};

	print_point("utf8", &u8);
	print_point("utf16BE", &u16be);
	print_point("utf16LE", &u16le);
	print_point("utf32BE", &u32be);
	print_point("utf32LE", &u32le);

}