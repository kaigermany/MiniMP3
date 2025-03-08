
#include "WAV.h"
#include <Arduino.h>

inline int readIntLE(char* page, int pos){
	return ((int*)(page + pos))[0];
}
inline short readShortLE(char* page, int pos){
	return ((short*)(page + pos))[0];
}

WAVHeader* WAVClass::parseWAVHeader(char* page, int numPageBytes) {
	WAVHeader* result = (WAVHeader*)malloc(sizeof(WAVHeader));
	if(!result) return 0;
	result->errorText = 0;
	
	int RIFF = readIntLE(page, 0);
	int WAVE = readIntLE(page, 8);
	if (RIFF != 1179011410 || WAVE != 1163280727) {
		result->errorText = "parseWAVHeader() : Missing WAV Header!";
		return result;
	}
	
	int readPos = 12;
	short numChannels = -1;
	int sampleRate = -1;
	int bitsPerSample = -1;
	
	int absoluteStartPos = 12;
	int dataSize = -1;
	int numFrames = -1;
	
	while(1){
		if(readPos+8 >= numPageBytes){
			result->errorText = "parseWAVHeader() : Insufficient Data! suggest to increace your page buffer size.";
			return result;
		}
		int fourCC = readIntLE(page, readPos);
		int pageSize = readIntLE(page, readPos + 4);
		readPos += 8;
		pageSize += (pageSize & 1);
		absoluteStartPos += 8;
		if (fourCC == 1635017060) {//data start
			if(sampleRate == -1){//has NOT FoundMetaData
				result->errorText = "parseWAVHeader() : found data section, but still missing format data!";
				return result;
			}
			int len = (bitsPerSample + 7) / 8 * numChannels;
			
			dataSize = pageSize;
			numFrames = len;
			
			result->dataStart = absoluteStartPos;
			result->dataLength = dataSize;
			result->numFrames = numFrames;
			result->numChannels = numChannels;
			result->sampleRate = sampleRate;
			result->bitsPerSample = bitsPerSample;
			
			return result;
		}
		absoluteStartPos += pageSize;
		if(fourCC == 544501094) {
			if(readPos+pageSize >= numPageBytes){
				result->errorText = "parseWAVHeader() : Insufficient Data! Is your page buffer size too small?";
				return result;
			}
			
			int encType = readShortLE(page, readPos);
			readPos += 2;
			if (encType == 1) {
				//encoding = Encoding.PCM_SIGNED;
			} else if (encType == 6) {
				result->errorText = "parseWAVHeader() : Encoding.ALAW is not supported.";
				return result;
			} else if (encType == 7) {
				result->errorText = "parseWAVHeader() : Encoding.ULAW is not supported.";
				return result;
			} else {
				result->errorText = "parseWAVHeader() : Not a supported WAV file(unknown encoding type)";
				return result;
			}
			numChannels = (short)readShortLE(page, readPos);
			readPos += 2;
			if (numChannels <= 0) {
				result->errorText = "parseWAVHeader() : Invalid number of channels";
				return result;
			}
			
			sampleRate = readIntLE(page, readPos);
			
			readPos += 4+6;
			
			bitsPerSample = readShortLE(page, readPos);
			readPos += 2;
			if (bitsPerSample <= 0) {
				result->errorText = "parseWAVHeader() : Invalid bitsPerSample";
				return result;
			}
			//if (bitsPerSample == 8 && encoding.equals(Encoding.PCM_SIGNED)) encoding = Encoding.PCM_UNSIGNED;
			int bytesLeft = pageSize - 16;//16 bytes read.
			readPos += bytesLeft;
		} else {
			readPos += pageSize;
		}
	}
}
