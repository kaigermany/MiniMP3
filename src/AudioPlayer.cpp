
#include "AudioPlayer.h"
#include "MP3Parser.h"
#include "WAV.h"
#include "LinkedList.h"
#include "Reader.h"
#include "AudioOutputStream.h"
#include "printlnLogging.h"

Reader* AudioPlayerClass::currentSource = nullptr;
LinkedList* AudioPlayerClass::inputBuffer = nullptr;
WAVHeader* AudioPlayerClass::wav = nullptr;
MP3Parser* AudioPlayerClass::mp3 = nullptr;
float AudioPlayerClass::autoAmplifyBias = 1.0/255;
float AudioPlayerClass::adjustStepPerSec = 1.0/100000;
const bool autoAmplify = false;
uint32_t AudioPlayerClass::lastSuccessfullRead = 0;

bool testForMp3(char* headerSector){
	int firstByte = headerSector[0] & 0xFF;
	if(firstByte == 255){//pattern 0xFFF << 16 (mp3 Frame)
		return (headerSector[1] & 0xF0) == 0xF0;//mp3 start frame detected.
	} else if(firstByte == 73){//pattern: 73 68 51 (mp3 ID3 metadata tag)
		return headerSector[1] == 68 && headerSector[2] == 51;//mp3 ID3-Tag detected.
	}
}

void AudioPlayerClass::detectDecoder(){
	if(!inputBuffer) return;
	if(!inputBuffer->firstEntry) return;
	ReadableBlock* block = (ReadableBlock*)inputBuffer->firstEntry->object;
	char* data = block->buf;
	int len = block->len;
	
	if(len >= 3){
		if(testForMp3(data)){
			mp3 = new MP3Parser();
println("mp3 detected.");
			return;
		}
	}
	
	{
		WAVHeader* header = WAV.parseWAVHeader(data, len);
		if(header){
			if(!header->errorText){
println("wav detected.");
				wav = header;
				int skipBytes = header->dataStart;
print("skipBytes = ");
nprintln(skipBytes);
				while(skipBytes){
					block = (ReadableBlock*)inputBuffer->firstEntry->object;
					
					if(block->len - block->off > skipBytes){
						block->off += skipBytes;
						return;
					} else {
						free(block->buf);
						skipBytes -= block->len - block->off;
						inputBuffer->removeFirstEntry();
						free(block);
						if(!inputBuffer->firstEntry) break;
					}
				}
print("WAV: incomplete header skip -> ");
nprintln(skipBytes);
				//Serial.println("WAV: incomplete header skip. expect errors in playback.");
				return;
			}
		}
	}
}

char AudioPlayerClass::fillReadBuffers(){//1: OOM, 2: no more data read
	if(!inputBuffer) {
		inputBuffer = new LinkedList();
		if(!inputBuffer) return 1;
	}
	const int bufSize = 512*2;
	for(int retryCounter=0; retryCounter<16; retryCounter++){
		if(inputBuffer->size >= 16) return 0;//return that our buffer is greatly filled.
		
		char* buf = (char*)malloc(bufSize);
		if(!buf) return 1;
		ReadableBlock* block = new ReadableBlock(buf, 0, bufSize);
		if(!block){
			free(buf);
			return 1;
		}
		int l = currentSource->read(buf, bufSize);//-1: EOF, 0: no new bytes right now, 1..?: len of buf
		//print("fillReadBuffers(): l = ");
		//nprintln(l);
		if(l == 0){
			free(block);
			free(buf);
			return 2;
		}
		if(l == -1){
			println("cp#1");
			currentSource->close();
			println("cp#2");
			free(currentSource);
			println("cp#3");
			currentSource = 0;
			free(block);
			println("cp#4");
			free(buf);
			println("cp#5");
			return 2;
		}
		if(l < bufSize) buf = (char*)realloc(buf, l);//shrink if possible, mostly used on WEB sources.
		block->len = l;//update size value
		if(inputBuffer->addEntry(block)){//if addEntry() fails then... practically rare or impossible.
			free(block);
			free(buf);
			return 1;
		}
	}
	return 2;
}

void AudioPlayerClass::prepareAndStoreAudio(char* data, int len, bool isStereo, int freq, bool autoAmplify){
	int numSamples = isStereo ? (len / 4) : (len / 2);//16 bit stereo or mono.
	char* outputAudio = (char*)malloc(numSamples*2);
	//DataEntry* entry = (DataEntry*)malloc(numSamples*2 + 4);//8 bit stereo.
	if(!outputAudio) {
		return;
	}
	
	if(freq > 44100){//TRUE
		int numSamples2 = (int)( (long long)numSamples * (long long)44100 / (long long)freq );

		if(isStereo){//TRUE
			uint32_t* samplePtr = (uint32_t*)data;
			for(int i=0; i<numSamples2; i++){
				int rp = (int)( (long long)i * (long long)freq / (long long)44100 );
				samplePtr[i] = samplePtr[rp];
			}
			len = numSamples2 * 4;
		} else {
			uint16_t* samplePtr = (uint16_t*)data;
			for(int i=0; i<numSamples2; i++){
				int rp = (int)( (long long)i * (long long)freq / (long long)44100 );
				samplePtr[i] = samplePtr[rp];
			}
			len = numSamples2 * 2;
		}
		
		numSamples = numSamples2;
		freq = 44100;
	}
	
	short* arr = (short*)data;
	int numShortValues = len / 2;
	if(autoAmplify){
		int max = 0;
		for(int i=0; i<numShortValues; i++){
			int tmp = arr[i];
			if(tmp < 0) tmp *= -1;
			if(tmp > max) max = tmp;
		}
		float maxAmpVal = max * autoAmplifyBias;
		if(maxAmpVal > 127){
			autoAmplifyBias *= 127.0 / maxAmpVal;
			//autoAmplifyBias = 127.0 / maxAmpVal;
		} else if(maxAmpVal > 64+32){
			float dur = (float)freq / numShortValues;
			autoAmplifyBias -= adjustStepPerSec * dur;
		} else if(maxAmpVal < 64){
			float dur = (float)freq / numShortValues;
			autoAmplifyBias += adjustStepPerSec * dur;
		}
		for(int i=0; i<numShortValues; i++){
			arr[i] = (char)(arr[i] * autoAmplifyBias);
		}
	} else {
		for(int i=0; i<numShortValues; i++){
			arr[i] /= 256;
		}
	}
	
	if(isStereo){//TRUE
		int l = numSamples * 2;
		for(int i=0; i<l; i++){
			outputAudio[i] = (char)(arr[i]);
		}
		numSamples = l;
	} else {
		int c=0;
		for(int i=0; i<numSamples; i++){
			char val = (char)(arr[i]);
			outputAudio[c++] = val;
			outputAudio[c++] = val;
		}
	}
	AudioOutputStream.write(outputAudio, numSamples);
}

int getMaxAllocatableSpace(){
	const int blockSize = 5000;//1024;
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
	/*
	if(capacity > 8192){
		capacity = getMaxAllocatableSpace2();
	}
	*/
	return capacity;
}

void printRam(){
	print("Ram: ");
	nprintln(getMaxAllocatableSpace());
}

void AudioPlayerClass::setSource(Reader* reader){
	currentSource = reader;
	if(currentSource){
		AudioOutputStream.start();
		fillReadBuffers();
		detectDecoder();
	} else {
		println("source = null.");
	}
}

void AudioPlayerClass::closeSource(){
	if(currentSource){
		currentSource->close();
		free(currentSource);
		currentSource = 0;
	}
}

void AudioPlayerClass::close(){
	AudioOutputStream.stop();
	closeSource();
	if(wav){
		free(wav);
		wav = 0;
	}
	if(mp3){
		mp3->~MP3Parser();
		free(mp3);
		mp3 = 0;
	}
	
}

void AudioPlayerClass::backupOffsets(){
	//Serial.println("backupOffsets():");
	LinkedListEntry* next = inputBuffer->firstEntry;
	while(next){
		ReadableBlock* block = (ReadableBlock*)(next->object);
		block->offsetRestore = block->off;
		//Serial.println(block->off);
		next = next->next;
	}
}

void AudioPlayerClass::restoreOffsets(){
	LinkedListEntry* next = inputBuffer->firstEntry;
	//Serial.println("restoreOffsets():");
	while(next){
		ReadableBlock* block = (ReadableBlock*)(next->object);
		block->off = block->offsetRestore;
		//Serial.println(block->off);
		next = next->next;
	}
}

void AudioPlayerClass::refreshBufferList(){//TODO filter mp3 arraylist: if entry 100% full used -> remove entry. maybe ands in empty list.
	int entriesToDelete = 0;
	LinkedListEntry* next = inputBuffer->firstEntry;
	while(next){
		ReadableBlock* block = (ReadableBlock*)(next->object);
		if(block->off >= block->len){
			entriesToDelete++;
		} else {
			break;
		}
		next = next->next;
	}
	while(entriesToDelete > 0){
		ReadableBlock* block = (ReadableBlock*)inputBuffer->removeFirstEntry();
		free(block->buf);
		free(block);
		entriesToDelete--;
	}
}

bool DataReader_readArr2(LinkedList* list, char* array, int len){
	LinkedListEntry* next = list->firstEntry;
	while(next){
		ReadableBlock* block = (ReadableBlock*)(next->object);
		int capacity = block->len - block->off;
		if(capacity > 0){
			if(capacity > len) capacity = len;
			char* srcPtr = block->buf + block->off;
			for(int i=0; i<capacity; i++){
				array[0] = srcPtr[0];
				array++;
				srcPtr++;
			}
			len -= capacity;
			block->off += capacity;
			if(len == 0) return 1;
		}
		next = next->next;
	}
	return 0;
}

int DataReader_read2(LinkedList* list){
	LinkedListEntry* next = list->firstEntry;
	while(next){
		ReadableBlock* block = (ReadableBlock*)(next->object);
		if(block->off < block->len){
			int val = (block->buf[block->off]) & 0xFF;
			block->off++;
			return val;
		}
		next = next->next;
	}
	return -1;
}

char AudioPlayerClass::updateLoop(){//3: EOF, 2: mp3 invalid data/no header found, 1: OutOfMemory
	println("updateLoop()::begin");
	const int timeoutMaxDurationMillis = 50;
	if(!currentSource) {
		return 3;
	}
	/*
	uint32_t startTime = millis();
	while(1){
		int returnCode = fillReadBuffers();//TODO add timeout check.
		if(inputBuffer->size > 0 || returnCode == 1) break;//if has data or OOM then break loop.
		if(returnCode == 2){
			if(millis() - startTime > timeoutMaxDurationMillis){
				return 3;
			} else {
				continue;
			}
		}
	}
	*/
	int returnCode = fillReadBuffers();
	//print("fillReadBuffers(): returnCode = ");
	//nprintln(returnCode);
	
	if(returnCode == 0){
		lastSuccessfullRead = millis();
	} else if(returnCode == 2){
		if(millis() - lastSuccessfullRead > timeoutMaxDurationMillis){
			return 3;
		}
	}
	println("updateLoop()::data loaded");
	for(char maxRetrys=0; maxRetrys<1 && AudioOutputStream.getCurrentBufferSize() < 10000; maxRetrys++){
		if(mp3){
			backupOffsets();
			int errorCode[1];
			char* data = mp3->runDecode(inputBuffer, &(errorCode[0]));
			int error = errorCode[0];
			if(!data && error){
				print("mp3 error code: ");
				nprintln(error);
				int dataWasAdded = 0;
				if(error == MP3_ERRORCODE_END_OF_INPUT_BUFFER_REACHED){//ran dry...
					println("restoreOffsets()::begin");
					printRam();
					restoreOffsets();
					println("restoreOffsets()::end");
					printRam();
					//refreshBufferList();
					//dataWasAdded = readNextDataBlock();
					fillReadBuffers();
	println("fillReadBuffers #2 done.");
	printRam();
					if(inputBuffer->size == 0) return 3;
					break;
				} else if(error == MP3_ERRORCODE_BUFFER_HAS_NO_STARTSEQUENCE){//current buffer does not contain any usable data!
					refreshBufferList();//this way we may get rid of empty sectors.
					//dataWasAdded = readNextDataBlock();
					fillReadBuffers();
					if(inputBuffer->size == 0) return 3;//if no more data left then return EOF state.
					return 2;
				} else if(error == MP3_ERRORCODE_INSUFFICENT_MEMORY/* && getNumBlocksInDACBuffer() > 0*/){
					restoreOffsets();
					//refreshBufferList();
					//delay(1);
					//continue;
					return 1;
				} else {
					Serial.print("errorCode = ");
					Serial.println(error);
				}
			} else {
				refreshBufferList();
			}
			prepareAndStoreAudio(data, mp3->lastFrameSampleCount, mp3->isStereo, mp3->samplingFrequency, autoAmplify);
			free(data);
		} else if(wav){
			
			if(inputBuffer && inputBuffer->firstEntry){
				int len = 512*16;
				char* outBuf = (char*)malloc(len);
				if(!outBuf) return 1;
				
				backupOffsets();
				if(!DataReader_readArr2(inputBuffer, outBuf, len)){
					restoreOffsets();
					//try read only the first buf
					int len2 = len;
					int wp = 0;
					LinkedListEntry* next = inputBuffer->firstEntry;
					if(next){
						ReadableBlock* block = (ReadableBlock*)(next->object);
						int capacity = block->len - block->off;
						if(capacity > 0){
							if(capacity > len2) capacity = len2;
							char* srcPtr = block->buf + block->off;
							for(int i=0; i<capacity; i++){
								outBuf[wp] = srcPtr[0];
								wp++;
								srcPtr++;
							}
							block->off += capacity;
						}
					}
					if(wp > 0){
						len = wp;
					} else {
						free(outBuf);
						return 3;
					}				
				}
				refreshBufferList();
				
				prepareAndStoreAudio(outBuf, len, wav->numChannels == 2, wav->sampleRate, autoAmplify);
				free(outBuf);
				
			}
		}
	}
	return 0;
}

inline long long AudioPlayerClass::getCurrentSampleCount(){
	return AudioOutputStream.getCurrentSampleCount();
}

inline bool AudioPlayerClass::isReaderClosed(){
	return currentSource == 0;
}

