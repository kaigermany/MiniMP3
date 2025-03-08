
#ifndef AudioOutputStream_H
#define AudioOutputStream_H
#include <Arduino.h>

class AudioOutputStreamClass {
	public:
		typedef struct DataEntryStruct {
			char* dataPtr;
			struct DataEntryStruct* nextEntryPtr;
			unsigned short size;
		} DataEntry;
		
	private:
		static void IRAM_ATTR Timer0_ISR();
		static hw_timer_t *Timer0_Cfg;
		static int timerCorrection;
		static struct DataEntryStruct* firstFrame;
		static struct DataEntryStruct* lastFrame;
		static int playbackPointer;
		static int bufferSize;
		static uint64_t timerTriggerCounter;
		
		static void timerListener();
		
	public:
		static void start();
		static void stop();
		static void write(void* buf, int numBytes);
		static int getCurrentBufferElementCount();
		static uint64_t getCurrentSampleCount();
		static void setCurrentSampleCount(uint64_t newValue);
		static AudioOutputStreamClass::DataEntry* getCurrentSampleBufferRef();
};
extern AudioOutputStreamClass AudioOutputStream;
#endif