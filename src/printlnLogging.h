

#include <Arduino.h>

#ifndef printlnClass
#define printlnClass

void print(const char *msg);
void println(const char *msg);
void nprint(const int msg);
void nprintln(const int msg);
void printHex(const char msg);
void printHexS(const int msg);
void printHexI(const int msg);
void printHexL(const long long msg);
void printHexBuf(const void* buf, const uint32_t numBytes);

#endif