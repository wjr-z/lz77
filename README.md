# lz77

LEVEL 1 ：速度略慢于 FastLz，压缩效果略好于FastLz。

LEVEL 2 ：解压速度大约40MB/s，压缩效果较好，压缩后文件平均比zip压缩大10% ~ 30%，但速度为zip的3 ~ 4倍。



### 使用方法：

```C++
#include "lz77.h"

int compress(const void*input,int length,void*output);//返回压缩后长度
std::string compress(const std::string& arr);
int decompress(const void*input,int length,void*output);//返回解压后长度
std::string decompress(const std::string&arr);
```

均包含在命名空间lz77内。
