#ifndef LZ77_H
#define LZ77_H
#include <string>

#include <Windows.h>
static LARGE_INTEGER freq;

static BOOL initFreq() {
	if (!QueryPerformanceFrequency(&freq))
		return FALSE;
	else
		return TRUE;
}

static double currTime() //使用高精度计时器
{
	LARGE_INTEGER performanceCount;
	double time;
	if (freq.QuadPart == 0) {
		BOOL bRet = initFreq();
		if (!bRet)
			return 0;
	}
	QueryPerformanceCounter(&performanceCount);
	time = performanceCount.HighPart * 4294967296.0 + performanceCount.LowPart;
	time = time / (freq.HighPart * 4294967296.0 + freq.LowPart);
	return time;
}

namespace lz77 {
	std::string compress(const std::string& arr);
	std::string decompress(const std::string&arr);
}

extern double tot;


#endif LZ77_H