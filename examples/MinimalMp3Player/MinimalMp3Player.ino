
#include "SPI.h"
#include "SD.h"
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include "LinkedList.h"
#include "Reader.h"
#include "AudioPlayer.h"

const char* wifi_ssid = "AP_Name";
const char* wifi_password = "AP_Password";

LinkedList* fileList;
int fileIndexPointer = 0;

void setup(){
	delay(1000);
	Serial.begin(115200);
	WiFi.mode(WIFI_STA);
	WiFi.begin(wifi_ssid, wifi_password);
	fileList = new LinkedList();
	reloadSD();
	if(fileList->size) AudioPlayer.setSource(new Reader((char*)fileList->getEntry(fileIndexPointer)));
}

void loop(){
	int code = AudioPlayer.updateLoop();
	if(code == 3){
		Serial.println("AudioPlayer.closeSource()");
		AudioPlayer.closeSource();
		if(fileList->size){
			fileIndexPointer = (fileIndexPointer + 1) % fileList->size;
			Serial.print("AudioPlayer.setSource() -> ");
			Serial.println((char*)fileList->getEntry(fileIndexPointer));
			AudioPlayer.setSource(new Reader((char*)fileList->getEntry(fileIndexPointer)));
		}
	} else if(code){
		Serial.print("AudioPlayer error code = ");
		Serial.println(code);
	} else {
		printRam2();
	}
}

void reloadSD(){
	fileList->clear();
	
	SPI.begin(33, 34, 2, 32);
	
	if (!SD.begin(32)) {
		Serial.println("SD initialization failed!");
		return;
	}
	
	Serial.println("<begin dump>");
	scanFileSystem();
	Serial.println("<end dump>");
	
	if(fileList->size == 0){
		SD.end();
		if (!SD.begin(32)) {
			Serial.println("SD initialization failed! (#2)");
			return;
		}
		Serial.println("<begin dump>");
		scanFileSystem();
		Serial.println("<end dump>");
	}
}

void scanDir(char* dirPath, int strLen){
	File dir = SD.open(dirPath);
	if (!dir) return;
	while (true) {
		File entry = dir.openNextFile();
		if (!entry) break;
		bool isDir = entry.isDirectory();
		const char* namePtr = entry.name();
		entry.close();
		
		int len = 0;
		while(namePtr[len]) len++;
		char* name = (char*)malloc(strLen + 1 + len + 1);
		if(!name){
			Serial.println("unable to alloc name str");
			return;
		}
		for(int i=0; i<strLen; i++) name[i] = dirPath[i];
		name[strLen] = '/';
		for(int i=0; i<len; i++) name[strLen + 1 + i] = namePtr[i];
		name[strLen + 1 + len] = 0;
		
		
		if(isDir){
			scanDir(name, strLen + 1 + len);
			free(name);
		} else {
			bool isSupportedEnding = false;
			if(len > 4){
				int typeOffset = strLen + 1 + len - 4;//len - 4;
				//Serial.println(name);
				//Serial.println((char)namePtr[typeOffset]);
				if(name[typeOffset] == '.'){
					isSupportedEnding =
						(name[typeOffset + 1] == 'm' & name[typeOffset + 2] == 'p' & name[typeOffset + 3] == '3') |
						(name[typeOffset + 1] == 'w' & name[typeOffset + 2] == 'a' & name[typeOffset + 3] == 'v');
				}
			}
			if(isSupportedEnding){
				Serial.println(name);
				fileList->addEntry(name);
			} else {
				free(name);
			}
		}
	}
}

void scanFileSystem(){
	const char* rootDirName = "/";
	scanDir((char*)rootDirName, 0);
}

int getMaxAllocatableSpace2(){
	const int blockSize = 5000;
	void* ptrs[200];
	int i;
	for(i=0; i<200; i++){
		ptrs[i] = malloc(blockSize);
		if(!ptrs[i]) break;
	}
	int capacity = i * blockSize;
	while(i > 0){
		i--;
		free((void*)ptrs[i]);
	}
	return capacity;
}
void printRam2(){
	print("Ram: ");
	nprintln(getMaxAllocatableSpace2());
}