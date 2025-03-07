
#ifndef AudioPlayer_H
#define AudioPlayer_H

#include "AudioPlayer.h"
#include "MP3Parser.h"
#include "WAV.h"
#include "LinkedList.h"
#include "Reader.h"

class AudioPlayerClass{
	private:
		static Reader* currentSource;
		static LinkedList* inputBuffer;
		static WAVHeader* wav;
		static MP3Parser* mp3;
		
		static float autoAmplifyBias;
		
		static /*const*/ float adjustStepPerSec;

		static uint32_t lastSuccessfullRead;
		
		static char fillReadBuffers();
		static void prepareAndStoreAudio(char* data, int len, bool isStereo, int freq, bool autoAmplify);
		static void detectDecoder();
		
		static void backupOffsets();
		static void restoreOffsets();
		static void refreshBufferList();
	public:
		static void setSource(Reader* reader);
		static void close();
		static void closeSource();
		static char updateLoop();
		static long long getCurrentSampleCount();
		static bool isReaderClosed();
		static void awaitBufferDrained();
};

extern AudioPlayerClass AudioPlayer;

#endif
