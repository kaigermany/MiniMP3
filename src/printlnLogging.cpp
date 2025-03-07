
#include <Arduino.h>
//extern "C" {
  #include "printlnLogging.h"
//}


#ifndef printlnClass2
#define printlnClass2

void print(const char *msg){
  Serial.print(msg);
}

void println(const char *msg){
  Serial.println(msg);
  Serial.flush();
}

void nprint(const int msg){
  Serial.print(msg);
}

void nprintln(const int msg){
  Serial.println(msg);
  Serial.flush();
}

#endif