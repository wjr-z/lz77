#include "LZ77.h"
#include "Allocator.h"
#include <unordered_map>
#include <stddef.h>

double tot;

enum { WINDOW_SIZE_BIT = 13 };
enum { WINDOW_SIZE = 1 << WINDOW_SIZE_BIT };
enum { BUFFER_SIZE_BIT = 8 };
enum { BUFFER_SIZE = 1 << BUFFER_SIZE_BIT };
enum { BUFFER_LEVEL_1_bit = 3 };
enum { BUFFER_LEVEL_1 = 1 << BUFFER_LEVEL_1_bit };

namespace lz77 {

	const uint32_t Size = 1 << 14 | 1;
	const uint32_t fib = 2654435769;

	uint32_t List[Size], End[Size];
	uint8_t ListSize[Size];

	uint32_t nxt[WINDOW_SIZE | 1], pre[WINDOW_SIZE | 1], key[WINDOW_SIZE | 1], stk[WINDOW_SIZE | 1], top;
	uint32_t notused = 0;

	uint32_t fibhash(const uint32_t& x) { return (x * fib) >> 18; }

	inline uint32_t gethash(const void* ptr) {
		return fibhash((*(uint32_t*)ptr) & 0xffffff);
	}

	void push_List(const uint8_t* str, uint32_t pos) {

		uint32_t head = gethash(str + pos);

		if (ListSize[head] >= 16) //降低压缩率以提高速度
			return;

		++ListSize[head];

		uint32_t x = !top ? ++notused : stk[top--];
		key[x] = pos;
		pre[x] = End[head];
		nxt[x] = 0;

		if (!List[head]) {
			List[head] = End[head] = x;
		}
		else {
			nxt[End[head]] = x;
			End[head] = x;
		}
	}
	void pop_List(const uint8_t* str, uint32_t pos) {

		uint32_t head = gethash(str + pos);
		if (key[List[head]] != pos)return;
		--ListSize[head];

		stk[++top] = List[head];

		if (List[head] == End[head])
			List[head] = End[head] = 0;
		else {
			uint32_t nxtx = nxt[List[head]];
			pre[nxtx] = 0;
			List[head] = nxtx;
		}
	}

	void lz77_push(const uint8_t* str, uint32_t pos) {
		push_List(str, pos);
		if (pos >= WINDOW_SIZE)pop_List(str, pos - WINDOW_SIZE);
	}

	uint32_t longestmatch(const uint8_t* str, uint32_t head, uint32_t Length, uint32_t& offset) {
		//str为压缩字符串的首地址，从str+head开始匹配，总长度为Length，offset为滑动窗口的偏移量。

		if (head + 2 >= Length)return 0;

		const uint8_t* buffer = str + head;

		uint32_t longest = 0;

		uint32_t x = gethash(buffer), bufferhead, windowhead;

		for (uint32_t i = List[x]; i; i = nxt[i]) {
			bufferhead = 0;
			windowhead = key[i];

			while (bufferhead < BUFFER_SIZE
				&& str[windowhead] == buffer[bufferhead]) {
				++bufferhead;
				++windowhead;
			}

			if (bufferhead > longest) {
				offset = key[i] - head + WINDOW_SIZE;
				longest = bufferhead;
				if(longest==BUFFER_SIZE)
					return longest;
			}
		}

		return longest;
	}

	void writebuf(std::string& str, uint32_t& buf, uint32_t& bufsize) {
		while (bufsize >= 8) {
			str.push_back(buf >> bufsize - 8);
			bufsize -= 8;
		}
	}


	std::string compress(const std::string& arr) {

		std::string str;
		uint32_t Length = arr.length();

		const uint8_t* ql = (const uint8_t*)arr.c_str();

		uint32_t buf = 0;
		uint32_t bufsize = 0;
		uint32_t offset;
		uint32_t match;

		for (uint32_t i = 0, j; i < Length;) {

			match = longestmatch(ql, i, Length, offset);
			lz77_push(ql, i);

			if (!match) {
				buf = buf << 9 | ql[i];
				bufsize += 9;
				writebuf(str, buf, bufsize);
				++i;
			}
			else {
				if (match <= BUFFER_LEVEL_1) {
					buf = buf << 2 | 3;

					buf = buf << WINDOW_SIZE_BIT | offset;
					bufsize += WINDOW_SIZE_BIT + 2;

					writebuf(str, buf, bufsize);

					buf = buf << BUFFER_LEVEL_1_bit | (match - 1);
					bufsize += BUFFER_LEVEL_1_bit;

					writebuf(str, buf, bufsize);

					for (j = i + 1; j < i + match; ++j)
						lz77_push(ql, j);

					i += match;
				}
				else {
					buf = buf << 2 | 2;

					buf = buf << WINDOW_SIZE_BIT | offset;
					bufsize += WINDOW_SIZE_BIT + 2;

					writebuf(str, buf, bufsize);

					buf = buf << BUFFER_SIZE_BIT | (match - 1);
					bufsize += BUFFER_SIZE_BIT;

					writebuf(str, buf, bufsize);

					for (j = i + 1; j < i + match; ++j)
						lz77_push(ql, j);

					i += match;
				}
			}
		}

		if (bufsize) {
			buf <<= (8 - bufsize);
			bufsize = 8;
			writebuf(str, buf, bufsize);
		}
		for (int i = Length >= WINDOW_SIZE ? Length - WINDOW_SIZE : 0; i < Length; ++i)
			pop_List(ql, i);


		return str;
	}

	bool getbit(const uint8_t* ql, uint8_t& bit, uint8_t& bitsize, uint32_t& bithead, uint32_t& i) {
		if (!bitsize)
			bit = ql[bithead++], bitsize = 8;
		--bitsize;
		bool val = bit >> bitsize;
		if (val)bit ^= 1 << bitsize;
		++i;
		return val;
	}

	std::string decompress(const std::string& arr) {
		std::string str;
		uint32_t Length = arr.length();
		const uint8_t* ql = (const uint8_t*)arr.c_str();

		uint32_t head = 0;
		uint32_t bufLength = Length << 3;

		uint8_t bit, bitsize;
		uint32_t bithead;
		bit = bitsize = bithead = 0;

		for (uint32_t i = 0; i < bufLength;) {
			bool val = getbit(ql, bit, bitsize, bithead, i);
			if (!val) {
				if (i + 7 >= bufLength)break;
				uint8_t _ch = 0;
				for (uint32_t j = 0; j < 8; ++j)
					_ch = _ch << 1 | getbit(ql, bit, bitsize, bithead, i);
				str.push_back(_ch);
				++head;
			}
			else {
				uint8_t MAXN = 0;

				if (!getbit(ql, bit, bitsize, bithead, i))MAXN = BUFFER_SIZE_BIT;
				else MAXN = BUFFER_LEVEL_1_bit;

				uint32_t offset = 0;
				uint32_t match = 0;

				for (uint8_t j = 0; j < WINDOW_SIZE_BIT; ++j)
					offset = offset << 1 | getbit(ql, bit, bitsize, bithead, i);

				for (uint8_t j = 0; j < MAXN; ++j)
					match = match << 1 | getbit(ql, bit, bitsize, bithead, i);

				++match;

				for (uint32_t j = 0; j < match; ++j) {
					str.push_back(str[head - WINDOW_SIZE + offset]), ++head;
				}


			}
		}

		return str;
	}
}

