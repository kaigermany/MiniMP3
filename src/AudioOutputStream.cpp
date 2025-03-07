
#include "AudioOutputStream.h"

#include "esp32-hal.h"
#include "soc/dac_channel.h"

#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc.h"


#define DAC1_PIN 25
#define DAC2_PIN 26
#define ClockFrequenzyMHz 80


hw_timer_t* AudioOutputStreamClass::Timer0_Cfg = nullptr;
int AudioOutputStreamClass::timerCorrection = 0;
AudioOutputStreamClass::DataEntry* AudioOutputStreamClass::firstFrame = nullptr;
AudioOutputStreamClass::DataEntry* AudioOutputStreamClass::lastFrame = nullptr;
int AudioOutputStreamClass::playbackPointer = 0;
int AudioOutputStreamClass::bufferSize = 0;
long long AudioOutputStreamClass::timerTriggerCounter = 0;

void AudioOutputStreamClass::timerListener(){
	//triggerCounter++;
	if(!firstFrame) return;

	//if(timerCorrection >= 33.570145) return;
	timerCorrection += 1000000;
	if(timerCorrection >= 33570145) {
		timerCorrection -= 33570145;
		return;
	}
	timerTriggerCounter++;
	/*
	//ensure non-null char* refs:
	while(!firstFrame->dataPtr){
		DataEntry* next = firstFrame->nextEntryPtr;
		free(firstFrame);
		if(!next) return;
		firstFrame = next;
	}
	
	//now we savely have a data reference.
	char* data = firstFrame->dataPtr;
	//Serial.println (xPortGetCoreID());
	
	
	//DAC on steroides: direct system access
	//https://forum.arduino.cc/t/esp32-dacwrite-ersetzen/653954/4
	SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, data[playbackPointer + 0] + 128, RTC_IO_PDAC1_DAC_S);
	SET_PERI_REG_BITS(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC, data[playbackPointer + 1] + 128, RTC_IO_PDAC2_DAC_S);
	
	playbackPointer += 2;
	if(firstFrame->size >= playbackPointer){
		DataEntry* next = firstFrame->nextEntryPtr;
		free(firstFrame->dataPtr);
		bufferSize -= firstFrame->size;
		free(firstFrame);
		firstFrame = next;
		playbackPointer = 0;
	}
	*/
	//ensure non-null char* refs:
	
	while(playbackPointer >= firstFrame->size){
		playbackPointer = 0;
		noInterrupts();
		free(firstFrame->dataPtr);
		bufferSize -= firstFrame->size;
		DataEntry* next = firstFrame->nextEntryPtr;
		free(firstFrame);
		firstFrame = next;
		interrupts();
		if(!firstFrame) return;
	}
	
	char* data = firstFrame->dataPtr;
	
	//slow as hell, idk why...
	//dacWrite(DAC1_PIN + writerSwaper, data[bufferReadPos + writerSwaper] ^ 0x80);

	//DAC on steroides:
	//https://forum.arduino.cc/t/esp32-dacwrite-ersetzen/653954/4
	SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, data[playbackPointer] + 128, RTC_IO_PDAC1_DAC_S);
	SET_PERI_REG_BITS(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC, data[playbackPointer + 1] + 128, RTC_IO_PDAC2_DAC_S);
	playbackPointer += 2;
	
}

void IRAM_ATTR AudioOutputStreamClass::Timer0_ISR(){
	AudioOutputStreamClass::timerListener();
}

void AudioOutputStreamClass::start(){
	timerTriggerCounter = 0;
	if(Timer0_Cfg) return;
	
	timerCorrection = 0;
	playbackPointer = 0;
	firstFrame = 0;
	lastFrame = 0;
	
	Timer0_Cfg = timerBegin(0, ClockFrequenzyMHz, true); // Prescaler is set to 80
	timerAttachInterrupt(Timer0_Cfg, &Timer0_ISR, true);
	timerAlarmWrite(Timer0_Cfg, 22, true); // Alarm value set to 22
	timerAlarmEnable(Timer0_Cfg);
	
	CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
	CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN2_M);

	SET_PERI_REG_MASK(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_XPD_DAC | RTC_IO_PDAC2_DAC_XPD_FORCE);
	SET_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_XPD_DAC | RTC_IO_PDAC1_DAC_XPD_FORCE);
}

void AudioOutputStreamClass::stop(){
	if(Timer0_Cfg){
		timerEnd(Timer0_Cfg);
		timerDetachInterrupt(Timer0_Cfg);
		Timer0_Cfg = NULL;
	}
	while(firstFrame){
		DataEntry* next = firstFrame->nextEntryPtr;
		free(firstFrame->dataPtr);
		free(firstFrame);
		firstFrame = next;
	}
}

void AudioOutputStreamClass::write(void* buf, int numBytes){//expects 8bit stereo PCM audio data.
	DataEntry* e = (DataEntry*)malloc(sizeof(DataEntry));
	if(!e) return;
	e->dataPtr = (char*)buf;
	e->nextEntryPtr = 0;
	e->size = numBytes & -2; //-2 == ...1111 1110
	noInterrupts();
	if(!firstFrame){
		firstFrame = lastFrame = e;
		bufferSize = e->size;
	} else {
		lastFrame->nextEntryPtr = e;
		lastFrame = e;
		bufferSize += e->size;
	}
	interrupts();
}

int AudioOutputStreamClass::getCurrentBufferSize(){
	return bufferSize;
}

long long AudioOutputStreamClass::getCurrentSampleCount(){
	return timerTriggerCounter;
}



