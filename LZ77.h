#ifndef LZ77_H
#define LZ77_H
#include <string>

namespace lz77 {
	int compress(const void*input,int length,void*output);
	std::string compress(const std::string& arr);
	int decompress(const void*input,int length,void*output);
	std::string decompress(const std::string&arr);
}


#endif LZ77_H