
#include "ReadableBlock.h"

ReadableBlock::ReadableBlock(char* buf, int off, int len) : buf(buf),off(off),len(len),offsetRestore(off) {}
ReadableBlock::~ReadableBlock(){}
