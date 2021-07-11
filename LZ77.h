#ifndef LZ77_H
#define LZ77_H
#include <string>

namespace lz77 {
	std::string compress(const std::string& arr);
	std::string decompress(const std::string&arr);
}


#endif LZ77_H