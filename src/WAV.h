#ifndef WAV_H
#define WAV_H

typedef struct WAVHeaderStruct {
	int dataStart;
	int dataLength;
	int numFrames;
	short numChannels;
	int sampleRate;
	int bitsPerSample;
	//char errorCode;
	char* errorText;
} WAVHeader;

class WAVClass{
	private:
		
	public:
		static WAVHeader* parseWAVHeader(char* page, int numPageBytes);
};
extern WAVClass WAV;
#endif
