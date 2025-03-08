
#include "AudioOutputStream.h"

#include "esp32-hal.h"
#include "soc/dac_channel.h"

#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc.h"

#define DAC1_PIN 25//standard DAC pins, but read your specific datasheet to ensure you get the right pins.
#define DAC2_PIN 26
#define ClockFrequenzyMHz 80


hw_timer_t* AudioOutputStreamClass::Timer0_Cfg = nullptr;
int AudioOutputStreamClass::timerCorrection = 0;
AudioOutputStreamClass::DataEntry* AudioOutputStreamClass::firstFrame = nullptr;
AudioOutputStreamClass::DataEntry* AudioOutputStreamClass::lastFrame = nullptr;
int AudioOutputStreamClass::playbackPointer = 0;
int AudioOutputStreamClass::bufferSize = 0;
uint64_t AudioOutputStreamClass::timerTriggerCounter = 0;

void AudioOutputStreamClass::timerListener(){
	if(!firstFrame) return;

	//if(timerCorrection >= 33.570145) return;
	timerCorrection += 1000000;
	if(timerCorrection >= 33570145) {//finetune the fact that the timer event is a little bit to fast.
		timerCorrection -= 33570145;
		return;
	}
	timerTriggerCounter++;
	
	noInterrupts();
	
	char* data = firstFrame->dataPtr;
	
	//slow as hell, idk why...
	//dacWrite(DAC1_PIN + writerSwaper, data[bufferReadPos + writerSwaper] ^ 0x80);

	//DAC on steroides:
	//https://forum.arduino.cc/t/esp32-dacwrite-ersetzen/653954/4
	SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, data[playbackPointer] + 128, RTC_IO_PDAC1_DAC_S);
	SET_PERI_REG_BITS(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_DAC, data[playbackPointer + 1] + 128, RTC_IO_PDAC2_DAC_S);
	playbackPointer += 2;
	
	while(playbackPointer >= firstFrame->size){
		playbackPointer = 0;
		free(firstFrame->dataPtr);
		bufferSize -= 1;//firstFrame->size;
		DataEntry* next = firstFrame->nextEntryPtr;
		free(firstFrame);
		firstFrame = next;
		if(!firstFrame) break;
	}
	interrupts();
	
}

void IRAM_ATTR AudioOutputStreamClass::Timer0_ISR(){
	AudioOutputStreamClass::timerListener();
}

//call to start the timer.
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
	timerStart(Timer0_Cfg);
	
	CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
	CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN2_M);

	SET_PERI_REG_MASK(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_XPD_DAC | RTC_IO_PDAC2_DAC_XPD_FORCE);
	SET_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_XPD_DAC | RTC_IO_PDAC1_DAC_XPD_FORCE);
}

//call to shutdown everything and clean up.
void AudioOutputStreamClass::stop(){
	if(Timer0_Cfg){
		timerAlarmDisable(Timer0_Cfg);		// Disable further alarms
		timerDetachInterrupt(Timer0_Cfg);	// Detach the ISR to clear the callback
		timerStop(Timer0_Cfg);				// Stop the timer counter
		timerEnd(Timer0_Cfg);				// Free up the timer resources
		Timer0_Cfg = nullptr;
	}
	while(firstFrame){
		DataEntry* next = firstFrame->nextEntryPtr;
		free(firstFrame->dataPtr);
		free(firstFrame);
		firstFrame = next;
	}
}

//add a new data block. expects 8bit stereo with 44100Hz samplerate.
void AudioOutputStreamClass::write(void* buf, int numBytes){//expects 8bit stereo PCM audio data.
	DataEntry* e = (DataEntry*)malloc(sizeof(DataEntry));
	if(!e) return;
	e->dataPtr = (char*)buf;
	e->nextEntryPtr = 0;
	e->size = numBytes & -2; //-2 == ...1111 1110
	//ensure collision-savety with the timer event.
	//this part is very short, so the real delay effect is extremely small.
	noInterrupts();
	if(!firstFrame){
		firstFrame = lastFrame = e;
		bufferSize = 1;
	} else {
		lastFrame->nextEntryPtr = e;
		lastFrame = e;
		bufferSize += 1;
	}
	interrupts();
}

int AudioOutputStreamClass::getCurrentBufferElementCount(){
	return bufferSize;
}

uint64_t AudioOutputStreamClass::getCurrentSampleCount(){
	return timerTriggerCounter;
}

void AudioOutputStreamClass::setCurrentSampleCount(uint64_t newValue){
	timerTriggerCounter = newValue;
}

AudioOutputStreamClass::DataEntry* AudioOutputStreamClass::getCurrentSampleBufferRef(){//the object will disappear in very short time!
	return AudioOutputStreamClass::firstFrame;
}
