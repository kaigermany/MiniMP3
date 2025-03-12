
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
	
	WAVHeader* header = WAV.parseWAVHeader(data, len);
	if(header){
		if(!header->errorText){
			wav = header;
			int skipBytes = header->dataStart;
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
			return;
		}
	}
}

char AudioPlayerClass::fillReadBuffers(){//1: OOM, 2: no more data read
	if(!inputBuffer) {
		inputBuffer = new LinkedList();
		if(!inputBuffer) return 1;
	}
	const int bufSize = 512*2;
	for(int retryCounter=0; retryCounter<4; retryCounter++){
		if(inputBuffer->size >= 10) return 0;//return that our buffer is greatly filled.
		
		char* buf = (char*)malloc(bufSize);
		if(!buf) return 1;
		ReadableBlock* block = new ReadableBlock(buf, 0, bufSize);
		if(!block){
			free(buf);
			return 1;
		}
		int l = currentSource->read(buf, bufSize);//-1: EOF, 0: no new bytes right now, 1..?: len of buf
		if(l == 0){
			free(block);
			free(buf);
			return 2;
		}
		if(l == -1){
			currentSource->close();
			free(currentSource);
			currentSource = 0;
			free(block);
			free(buf);
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
	char* outputAudio = (char*)malloc(numSamples*2);//8 bit stereo.
	if(!outputAudio) {
		return;
	}
	
	if(freq > 44100){
		int numSamples2 = (int)( (long long)numSamples * (long long)44100 / (long long)freq );

		if(isStereo){
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
	
	if(isStereo){
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

void AudioPlayerClass::awaitBufferDrained(){
	while(1){
		char code = updateLoop();
		if(code) break;
		delay(10);
	}
}

//set a new input source and 
void AudioPlayerClass::setSource(Reader* reader){
	closeSource();//ensure last reader is closed.
	currentSource = reader;
	if(currentSource){//if reader is not null init decoder pipeline.
		AudioOutputStream.start();		//start player timer
		fillReadBuffers();				//read first blocks
		lastSuccessfullRead = millis();	//reset timeout value
		detectDecoder();				//detect media type by the first data read.
		if(mp3 == nullptr && wav == nullptr){//none triggered?
			close();//save close of everything
		}
	}
}

//closes the current source and await the decoding of all buffers left in input queue.
void AudioPlayerClass::closeSource(){
	if(!currentSource) return;
	currentSource->close();//future read() calls return -1 in any case.
	
	awaitBufferDrained();//flush
	
	free(currentSource);//drop reader object
	currentSource = 0;
	
	if(wav){//drop wav header
		free(wav);
		wav = 0;
	}
	if(mp3){//drop mp3 instance
		mp3->~MP3Parser();
		free(mp3);
		mp3 = 0;
	}
}

//closes and deallocated the player.
void AudioPlayerClass::close(){
	if(inputBuffer){
		
		LinkedListEntry* next = inputBuffer->firstEntry;
		while(next){
			ReadableBlock* block = (ReadableBlock*)(next->object);
			free(block->buf);
			next = next->next;
		}
		inputBuffer->clear();
		
		//now no buffers are left to play in drain loop.
		closeSource();
		
		free(inputBuffer);//drop input list pointer
		inputBuffer = 0;
	}
	AudioOutputStream.stop();//drop output buffers & stop player timer
}

void AudioPlayerClass::backupOffsets(){
	LinkedListEntry* next = inputBuffer->firstEntry;
	while(next){
		ReadableBlock* block = (ReadableBlock*)(next->object);
		block->offsetRestore = block->off;
		next = next->next;
	}
}

void AudioPlayerClass::restoreOffsets(){
	LinkedListEntry* next = inputBuffer->firstEntry;
	while(next){
		ReadableBlock* block = (ReadableBlock*)(next->object);
		block->off = block->offsetRestore;
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
	const int timeoutMaxDurationMillis = 1500;
	if(!currentSource) {
		return 3;
	}
	
	if(fillReadBuffers() < 2) lastSuccessfullRead = millis();
	
	for(char maxRetrys=0; maxRetrys<2 && AudioOutputStream.getCurrentBufferElementCount() < 5; maxRetrys++){
		if(mp3){
			backupOffsets();
			int errorCode[1];
			char* data = mp3->runDecode(inputBuffer, &(errorCode[0]));
			int error = errorCode[0];
			if(!data && error){
				int dataWasAdded = 0;
				if(error == MP3_ERRORCODE_END_OF_INPUT_BUFFER_REACHED){//ran dry...
					restoreOffsets();
					if(fillReadBuffers() < 2) lastSuccessfullRead = millis();
					if(inputBuffer->size == 0) goto checkForTimeout;
					break;
				} else if(error == MP3_ERRORCODE_BUFFER_HAS_NO_STARTSEQUENCE){//current buffer does not contain any usable data!
					refreshBufferList();//this way we may get rid of empty sectors.
					if(fillReadBuffers() < 2) lastSuccessfullRead = millis();
					if(inputBuffer->size == 0) goto checkForTimeout;//if no more data left then return EOF state.
					return 2;
				} else if(error == MP3_ERRORCODE_INSUFFICENT_MEMORY/* && getNumBlocksInDACBuffer() > 0*/){
					restoreOffsets();
					return 1;
				} else {
					Serial.print("mp3 errorCode = ");
					Serial.println(error);
				}
			} else {
				refreshBufferList();//removes used buffers.
				lastSuccessfullRead = millis();
			}
			prepareAndStoreAudio(data, mp3->lastFrameSampleCount, mp3->isStereo, mp3->samplingFrequency, autoAmplify);
			free(data);
		} else if(wav){
			int returnCode = fillReadBuffers();
			if(returnCode == 2){
				goto checkForTimeout;
			} else {
				lastSuccessfullRead = millis();
			}
			
			if(inputBuffer && inputBuffer->firstEntry){
				int len = 512*8;
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
			} else {
				return 3;
			}
		}
	}
	return 0;
	
	checkForTimeout:
	if(millis() - lastSuccessfullRead > timeoutMaxDurationMillis){
		print("timed out: ");
		nprintln((int)(millis() - lastSuccessfullRead));
		return 3;
	} else {
		return 0;
	}
}

inline long long AudioPlayerClass::getCurrentSampleCount(){
	return AudioOutputStream.getCurrentSampleCount();
}

inline bool AudioPlayerClass::isReaderClosed(){
	return currentSource == 0;
}
