
#ifndef Reader_H
#define Reader_H

#include <WiFiClientSecure.h>
#include "SD.h"

class Reader{
	private:
		File* file;
		
		WiFiClientSecure* client;
		uint64_t bytesLeft;
		
		inline int file_read_impl(char* buffer, int numBytes);
		inline int https_read_impl(char* buffer, int numBytes);
		
		char* splitUrl(char* in, char** outServerName);
		void openHttps(char* server, char* path);
	public:
		Reader(char* server, char* path);
		Reader(char* filepath);
		Reader(File* file);
		~Reader();
		
		int read(char* buffer, int numBytes);
		void close();
};
#endif
