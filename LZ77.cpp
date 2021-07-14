#include "LZ77.h"
#include <algorithm>
#include <stddef.h>

enum { WINDOW_SIZE_BIT = 13 };
enum { WINDOW_SIZE = 1 << WINDOW_SIZE_BIT };

//不能压缩的标记符为0

enum { BUFFER_SIZE_BIT = 10 };//重复长度为3~1024
enum { BUFFER_SIZE = 1 << BUFFER_SIZE_BIT };

enum { MATCH_SIZE_BIT = 16 };//不压缩时最大长度
enum { MATCH_SIZE = 1 << MATCH_SIZE_BIT };

enum { MATCH_LIMIT = 8 };//不压缩时最小可优化大小
enum { LIST_LIMIT = 32 };//一条链表的最大size

namespace lz77 {

	/*---对于前三个字符进行hash，快速匹配相应位置---*/
	const uint32_t Size = 1 << 14 | 1;
	const uint32_t fib = 2654435769;

	uint32_t List[Size], End[Size];
	uint8_t ListSize[Size];

	uint32_t nxt[WINDOW_SIZE | 1], key[WINDOW_SIZE | 1], stk[WINDOW_SIZE | 1], top;
	uint32_t notused;

	inline uint32_t fibhash(const uint32_t& x) { return (x * fib) >> 18; }

	inline uint32_t gethash(const void* ptr) {
		return fibhash((*(uint32_t*)ptr) & 0xffffff);
	}

	void push_List(const uint8_t* str, uint32_t pos) {

		uint32_t head = gethash(str + pos);

		if (ListSize[head] >= LIST_LIMIT ) {
			//略微降低压缩率以提高速度。
			//尽量弹出前面的保留当前的。
			--ListSize[head];
			stk[++top] = List[head];
			List[head] = nxt[List[head]];
		}

		++ListSize[head];

		uint32_t x = top ? stk[top--] : ++notused;
		key[x] = pos;
		nxt[x] = 0;

		if (List[head]) {
			nxt[End[head]] = x;
			End[head] = x;
		}
		else List[head] = End[head] = x;
	}
	void pop_List(const uint8_t* str, uint32_t pos) {
		uint32_t head = gethash(str + pos);
		if (key[List[head]] != pos)return;

		--ListSize[head];
		stk[++top] = List[head];

		if (List[head] != End[head]) {
			List[head] = nxt[List[head]];
		}
		else {
			List[head] = End[head] = 0;
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

			if (str[windowhead + longest] != buffer[longest])continue;

			while (bufferhead < BUFFER_SIZE - 1 //最大为256
				&& bufferhead < Length - head && str[windowhead] == buffer[bufferhead]) {
				++bufferhead;
				++windowhead;
			}

			if (bufferhead > longest) {
				offset = head - key[i] - 1;
				longest = bufferhead;
				if (longest == BUFFER_SIZE - 1)
					break;
			}
		}

		return longest > 2 ? longest : 0;
	}

	void writebuf(std::string& str, uint32_t& buf, uint32_t& bufsize) {
		while (bufsize >= 8) {
			str.push_back(buf >> bufsize - 8);
			bufsize -= 8;
		}
	}

	void writebuf(uint8_t*& op, uint32_t& buf, uint32_t& bufsize) {
		while (bufsize >= 8) {
			*(op++) = buf >> bufsize - 8;
			bufsize -= 8;
		}
	}

	uint8_t lz77log2[MATCH_SIZE];

	void lz77log2initial() {
		static bool initialized=false;
		if(initialized)return ;
		lz77log2[0]=lz77log2[1]=0;

		for(uint32_t i=2;i<MATCH_SIZE;++i)
			lz77log2[i]=lz77log2[i>>1]+1;
	}

	std::string compress(const std::string& arr) {

		std::string str;
		str.append("wjr");

		uint32_t Length = arr.length();

		const uint8_t* ql = (const uint8_t*)arr.c_str();

		uint32_t buf = 0;
		uint32_t bufsize = 0;
		uint32_t offset = 0;
		uint32_t match = 0,matchLength=0;

		lz77log2initial();

		for (uint32_t i = 0, j; i < Length;) {

			j=i;
			matchLength=0;
			while (j < Length && matchLength < MATCH_SIZE - 1 &&!(match = longestmatch(ql, j, Length, offset))) {
				lz77_push(ql,j);
				++j;
				++matchLength;
			}

			if (matchLength) {
				if (matchLength <= MATCH_LIMIT) {
					for (j = 0; j < matchLength; ++j) {
						buf=buf<<9|ql[i++];
						bufsize+=9;
						writebuf(str,buf,bufsize);
					}
				}
				else {
					buf=buf<<3|4;
					bufsize+=3;
					writebuf(str,buf,bufsize);

					uint32_t logMatchLength=lz77log2[matchLength- MATCH_LIMIT];
					buf=buf<<(logMatchLength+1)|(((1<<logMatchLength)-1)<<1);
					bufsize+=logMatchLength+1;
					writebuf(str,buf,bufsize);

					buf=buf<<logMatchLength|(matchLength-MATCH_LIMIT-(1<<logMatchLength));
					bufsize+=logMatchLength;
					writebuf(str,buf,bufsize);

					for (j = 0; j < matchLength; ++j) {
						buf=buf<<8|ql[i++];
						bufsize+=8;
						writebuf(str,buf,bufsize);
					}
				}
			}

			if(!match)continue;

			uint32_t logmatch = lz77log2[match];
			buf = buf << (logmatch + 1) | (((1 << logmatch) - 1) << 1);
			bufsize += logmatch + 1;
			writebuf(str, buf, bufsize);

			buf = buf << logmatch | (match - (1 << logmatch));
			bufsize += logmatch;
			writebuf(str, buf, bufsize);

			buf = buf << WINDOW_SIZE_BIT | offset;
			bufsize += WINDOW_SIZE_BIT;
			writebuf(str, buf, bufsize);

			for (j = i; j < i + match; ++j)
				lz77_push(ql, j);

			i += match;
		}

		if (bufsize) {
			buf <<= (8 - bufsize);
			bufsize = 8;
			writebuf(str, buf, bufsize);
		}
		for (uint32_t i = Length >= WINDOW_SIZE ? Length - WINDOW_SIZE : 0; i < Length; ++i)
			pop_List(ql, i);

		return str;
	}

	int compress(const void* input, int length, void* output) {
		uint8_t* op = (uint8_t*)output;
		*(op++) = 'w';
		*(op++) = 'j';
		*(op++) = 'r';

		uint32_t Length = length;

		const uint8_t* ql = (const uint8_t*)input;

		uint32_t buf = 0;
		uint32_t bufsize = 0;
		uint32_t offset = 0;
		uint32_t match = 0 ,matchLength=0;

		lz77log2initial();

		for (uint32_t i = 0, j; i < Length;) {

			j = i;
			matchLength = 0;
			while (j < Length && matchLength < MATCH_SIZE - 1 && !(match = longestmatch(ql, j, Length, offset))) {
				lz77_push(ql, j);
				++j;
				++matchLength;
			}

			if (matchLength) {
				if (matchLength <= MATCH_LIMIT) {
					for (j = 0; j < matchLength; ++j) {
						buf = buf << 9 | ql[i++];
						bufsize += 9;
						writebuf(op, buf, bufsize);
					}
				}
				else {
					buf = buf << 3 | 4;
					bufsize += 3;
					writebuf(op, buf, bufsize);

					uint32_t logMatchLength = lz77log2[matchLength - MATCH_LIMIT];
					buf = buf << (logMatchLength + 1) | (((1 << logMatchLength) - 1) << 1);
					bufsize += logMatchLength + 1;
					writebuf(op, buf, bufsize);

					buf = buf << logMatchLength | (matchLength - MATCH_LIMIT - (1 << logMatchLength));
					bufsize += logMatchLength;
					writebuf(op, buf, bufsize);

					for (j = 0; j < matchLength; ++j) {
						buf = buf << 8 | ql[i++];
						bufsize += 8;
						writebuf(op, buf, bufsize);
					}
				}
			}

			if (!match)continue;

			uint32_t logmatch = lz77log2[match];
			buf = buf << (logmatch + 1) | (((1 << logmatch) - 1) << 1);
			bufsize += logmatch + 1;
			writebuf(op, buf, bufsize);

			buf = buf << logmatch | (match - (1 << logmatch));
			bufsize += logmatch;
			writebuf(op, buf, bufsize);

			buf = buf << WINDOW_SIZE_BIT | offset;
			bufsize += WINDOW_SIZE_BIT;
			writebuf(op, buf, bufsize);

			for (j = i; j < i + match; ++j)
				lz77_push(ql, j);

			i += match;
		}

		if (bufsize) {
			buf <<= (8 - bufsize);
			bufsize = 8;
			writebuf(op, buf, bufsize);
		}
		for (uint32_t i = Length >= WINDOW_SIZE ? Length - WINDOW_SIZE : 0; i < Length; ++i)
			pop_List(ql, i);

		return op - (uint8_t*)output;
	}

	bool getbit(const uint8_t*& ql, uint8_t& bit, uint8_t& bitsize, uint32_t& i) {
		if (!bitsize)
			bit = *(ql++), bitsize = 8;
		--bitsize;
		++i;
		return (bit >> bitsize) & 1;
	}

	std::string decompress(const std::string& arr) {
		std::string str;
		uint32_t Length = arr.length();
		const uint8_t* ql = (const uint8_t*)arr.c_str();
		if (arr[0] != 'w' || arr[1] != 'j' || arr[2] != 'r')return arr;
		ql += 3;

		uint32_t head = 0;
		uint32_t bufLength = Length << 3;
		bufLength -= 24;

		uint8_t bit, bitsize;
		bit = bitsize = 0;

		for (uint32_t i = 0; i < bufLength;) {
			bool val = getbit(ql, bit, bitsize, i);
			if (!val) {
				if (i + 7 >= bufLength)break;
				uint8_t _ch = 0;
				for (uint32_t j = 0; j < 8; ++j)
					_ch = _ch << 1 | getbit(ql, bit, bitsize, i);
				str.push_back(_ch);
				++head;
			}
			else {
				uint32_t logmatch = 1;
				while (getbit(ql, bit, bitsize, i))++logmatch;

				uint32_t offset = 0;
				uint32_t match = 0;

				for (uint8_t j = 0; j < logmatch; ++j)
					match = match << 1 | getbit(ql, bit, bitsize, i);

				match += (1 << logmatch) ;

				if (match == 2) {
					logmatch=0;
					while(getbit(ql,bit,bitsize,i))++logmatch;
					match=0;
					for (uint8_t j = 0; j < logmatch; ++j) 
						match=match<<1|getbit(ql,bit,bitsize,i);
					match+=(1<<logmatch)+MATCH_LIMIT;
					uint8_t _ch;
					for (uint32_t j = 0; j < match; ++j) {
						_ch=0;
						for (uint8_t k = 0; k < 8; ++k) 
							_ch=_ch<<1|getbit(ql,bit,bitsize,i);
						str.push_back(_ch);
						++head;
					}
					continue;
				}

				for (uint8_t j = 0; j < WINDOW_SIZE_BIT; ++j)
					offset = offset << 1 | getbit(ql, bit, bitsize, i);
				++offset;

				for (uint32_t j = 0; j < match; ++j) {
					str.push_back(str[head - offset]), ++head;
				}
			}
		}
		return str;
	}
	int decompress(const void* input, int length, void* output) {
		uint32_t Length = length;

		const uint8_t* ql = (const uint8_t*)input;

		if (*(ql++) != (uint8_t)'w' || (*ql++) != (uint8_t)'j' || (*ql++) != (uint8_t)'r') {
			memcpy(output, input, length);
			return length;
		}
		uint8_t* op = (uint8_t*)output;

		uint32_t bufLength = Length << 3;
		bufLength -= 24;

		uint8_t bit, bitsize;
		bit = bitsize = 0;

		for (uint32_t i = 0; i < bufLength;) {
			bool val = getbit(ql, bit, bitsize, i);
			if (!val) {
				if (i + 7 >= bufLength)break;
				uint8_t _ch = 0;
				for (uint32_t j = 0; j < 8; ++j)
					_ch = _ch << 1 | getbit(ql, bit, bitsize, i);
				*(op++)=_ch;
			}
			else {
				uint32_t logmatch = 1;
				while (getbit(ql, bit, bitsize, i))++logmatch;

				uint32_t offset = 0;
				uint32_t match = 0;

				for (uint8_t j = 0; j < logmatch; ++j)
					match = match << 1 | getbit(ql, bit, bitsize, i);

				match += (1 << logmatch);

				if (match == 2) {
					logmatch = 0;
					while (getbit(ql, bit, bitsize, i))++logmatch;
					match = 0;
					for (uint8_t j = 0; j < logmatch; ++j)
						match = match << 1 | getbit(ql, bit, bitsize, i);
					match += (1 << logmatch) + MATCH_LIMIT;
					uint8_t _ch;
					for (uint32_t j = 0; j < match; ++j) {
						_ch = 0;
						for (uint8_t k = 0; k < 8; ++k)
							_ch = _ch << 1 | getbit(ql, bit, bitsize, i);
						*(op++)=_ch;
					}
					continue;
				}

				for (uint8_t j = 0; j < WINDOW_SIZE_BIT; ++j)
					offset = offset << 1 | getbit(ql, bit, bitsize, i);
				++offset;

				for (uint32_t j = 0; j < match; ++j) 
					*(op++)=*(op-offset);
			}
		}
		return op - (uint8_t*)output;
	}
}

