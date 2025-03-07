
#include "SPI.h"
#include "SD.h"
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include "LinkedList.h"
#include "Reader.h"
#include "AudioPlayer.h"

const char* wifi_ssid = "AP_Name";
const char* wifi_password = nullptr;

LinkedList* fileList;
int fileIndexPointer = 0;

uint32_t lastConsoleUpdateTime = 0;
uint32_t computationTime = 0;

void setup(){
	delay(1000);
	Serial.begin(115200);
	
	WiFi.mode(WIFI_STA);
	if(wifi_ssid && wifi_password) WiFi.begin(wifi_ssid, wifi_password);
	
	fileList = new LinkedList();
	reloadSD();
	
	Serial.print("WiFi Connecting ..");
	int abbortTimer = 5 * 2;
	while (WiFi.status() != WL_CONNECTED && abbortTimer > 0) {
		Serial.print('.');
		delay(500);
		abbortTimer--;
	}
	
	if(WiFi.status() == WL_CONNECTED){
		Serial.println("WiFi Connected!");
		Serial.print("WiFi Local IP: ");
		Serial.println(WiFi.localIP());
		Serial.print("WiFi RSSI: ");
		Serial.println(WiFi.RSSI());
		
		Serial.println("Schwarzwald Radio livestream");
		AudioPlayer.setSource(new Reader((char*)"edge09.streamonkey.net", (char*)"/fho-schwarzwaldradiolive/stream/mp3"));
	} else {
		WiFi.disconnect();
		Serial.println("WiFi not ready.");
		
		if(fileList->size) AudioPlayer.setSource(new Reader((char*)fileList->getEntry(fileIndexPointer)));
	}
}
void loop(){
	uint32_t time = millis();
	if(time - lastConsoleUpdateTime >= 1000){//if 1 sec passed then
		lastConsoleUpdateTime = time;
		int usage = computationTime / 10; //1000ms = 100% usage
		computationTime = 0;
		
		Serial.print("CPU: ");
		Serial.print(usage);
		Serial.print("%, ");
		printRam();
	}
	
	int code = AudioPlayer.updateLoop();
	computationTime += millis() - time;
	if(code == 3){//track end?
		AudioPlayer.closeSource();
		if(fileList->size){//if there is at least 1 file in list then
			fileIndexPointer = (fileIndexPointer + 1) % fileList->size;
			Serial.print("AudioPlayer.setSource() -> ");
			Serial.println((char*)fileList->getEntry(fileIndexPointer));
			AudioPlayer.setSource(new Reader((char*)fileList->getEntry(fileIndexPointer)));
		}
	} else if(code){
		Serial.print("AudioPlayer error code = ");
		Serial.println(code);
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

int getMaxAllocatableSpace(){//approximates the free ram by trying to allocate as much space as possible.
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
void printRam(){
	print("Free Ram: ");
	nprint(getMaxAllocatableSpace() / 1024);
	println(" KB");
}