
#include <Arduino.h>
#include "printlnLogging.h"

#ifndef printlnClass2
#define printlnClass2

void print(const char *msg){
  Serial.print(msg);
}

void println(const char *msg){
  Serial.println(msg);
  //Serial.flush();
}

void nprint(const int msg){
  Serial.print(msg);
}

void nprintln(const int msg){
  Serial.println(msg);
  Serial.flush();
}

void printHex(const char msg){
	const char* hexTable = "0123456789ABCDEF";
	Serial.print(hexTable[(msg >> 4) & 0xF]);
	Serial.print(hexTable[msg & 0xF]);
}

void printHexS(const int msg){
	printHex((char)(msg >> 8));
	printHex((char)msg);
}

void printHexI(const int msg){
	printHex((char)(msg >> 24));
	printHex((char)(msg >> 16));
	printHex((char)(msg >> 8));
	printHex((char)msg);
}

void printHexL(const long long msg){
	printHexI((int)(msg >> 32));
	printHexI((int)msg);
}

void printHexBuf(const void* buf, const uint32_t numBytes){
	const char* buffer = (const char*)buf;
	for(int i=0; i<numBytes; i++){
		printHex(buffer[i]);
		Serial.print((i & 15) == 15 ? '\n' : ' ');
	}
	if(numBytes & 15) Serial.print('\n');
	Serial.flush();
}

#endif