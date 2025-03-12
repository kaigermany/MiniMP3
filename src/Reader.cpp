
#include "Reader.h"
#include "printlnLogging.h"

inline int Reader::file_read_impl(char* buffer, int numBytes){
	int len = file->read((uint8_t*)buffer, numBytes);
	if(len <= 0){//only if connection is closed, we return -1 !!!
		len = file->position() != file->size() ? 0 : -1;
		if(file->available() <= 0) len = -1;
	}
	return len;
}

inline int Reader::https_read_impl(char* buffer, int numBytes){
	if(bytesLeft == -1){
		int len = client->read((uint8_t*)buffer, numBytes);
		return len == -1 ? (client->connected() ? 0 : -1) : len;
	}
	if(bytesLeft < numBytes){
		numBytes = (int)bytesLeft;
	}
	int len = client->read((uint8_t*)buffer, numBytes);
	if(len != -1) bytesLeft -= len;
	if(len <= 0){//only if connection is closed, we return -1 !!!
		return bytesLeft == 0 ? -1 : 0;
	}
	return len;
}

char* Reader::splitUrl(char* in, char** outServerName) {
	if (!in || !outServerName) return NULL;

	const char* prefix1 = "http://";
	const char* prefix2 = "https://";
	char* start = NULL;

	// Check for HTTP or HTTPS prefix
	if (strncmp(in, prefix1, strlen(prefix1)) == 0) {
		start = in + strlen(prefix1);
	} else if (strncmp(in, prefix2, strlen(prefix2)) == 0) {
		start = in + strlen(prefix2);
	} else {
		return NULL;  // No valid prefix found
	}

	// Find first '/' after the server name
	char* path = strchr(start, '/');
	if (!path) {
		// No path, return root "/"
		*outServerName = strdup(start);
		return "/";
	}

	// Allocate server name (up to the first '/')
	size_t serverLen = path - start;
	*outServerName = (char*)malloc(serverLen + 1);
	if (!*outServerName) return NULL;

	strncpy(*outServerName, start, serverLen);
	(*outServerName)[serverLen] = 0;//dont forget end-symbol

	return path; // Return pointer to path part
}

void Reader::openHttps(char* server, char* path) {
	file = 0;
	client = new WiFiClientSecure();
	if(!client) return;
	client->setInsecure();
	if(client->connect(server, 443)){
		client->print("GET ");
		client->print(path);
		client->print(" HTTP/1.1\r\n");
		client->print("Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/png,image/svg+xml,*/*;q=0.8\r\n");
		client->print("Accept-Language: de,en-US;q=0.7,en;q=0.3\r\n");
		client->print("Connection: keep-alive\r\n");
		client->print("Host: ");
		client->print(server);
		client->print("\r\n");
		client->print("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:129.0) Gecko/20100101 Firefox/129.0\r\n\r\n");
		client->flush();
		int currHeaderStrLen = 0;
		const char* ContentLengthStr = "Content-Length: ";
		bool isContentLen = true;
		uint64_t lenVal = -1;
		while(client->connected()){
			int chr = client->read();
			if(chr == -1) continue;
			if(chr == '\n'){
				if(currHeaderStrLen < 3) break;//min len = "a: b\r\n";
				currHeaderStrLen = 0;
			}
			
			if(isContentLen){
				if(currHeaderStrLen < 16){
					if(ContentLengthStr[currHeaderStrLen] != chr){
						isContentLen = false;
					}
				} else {
					if(lenVal == -1) lenVal = 0;
					if(chr >= '0' && chr <= '9'){
						lenVal *= 10;
						lenVal += chr - '0';
					} else {
						isContentLen = false;
					}
				}
			}
			
			currHeaderStrLen++;
		};
		bytesLeft = lenVal;
		client = client;
	}
}

Reader::Reader(char* server, char* path) {
	openHttps(server, path);
}

Reader::Reader(char* filepath) {//also checks for https sources.
	char* serverName = NULL;
    char* path = splitUrl(filepath, &serverName);
	if (path) {
		openHttps(serverName, path);
		free(serverName);
		return;
	}
	
	file = new File( SD.open(filepath) );
	client = 0;//client will not be initialized as 0 by default; required for destructor
}

Reader::Reader(File* fileArg) {
	file = fileArg;
	client = 0;
}

void Reader::close(){
	if(file){
		file->close();
		free(file);
		file = 0;
	}
	if(client){
		client->stop();
		free(client);
		client = 0;
	}
}

int Reader::read(char* buffer, int numBytes){
	if(file){
		return file_read_impl(buffer, numBytes);
	} else if(client){
		return https_read_impl(buffer, numBytes);
	}
	return -1;
}

Reader::~Reader(){
	close();
}
