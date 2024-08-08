#define DEBUG
#ifdef DEBUG
#include <iostream>
#include <stdio.h>
#define PPRINTF(fmt, x...) printf("%s:" fmt, __FUNCTION__, ##x);
#define DPRINTF(x...) printf(x)
#define DCOUT(x...) std::cout << x
#else
#define DPRINTF(x...)
#define DCOUT(x...)
#endif