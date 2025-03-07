
#include "MP3Parser.h"

#include "LinkedList.h"
#include "ReadableBlock.h"
#include "printlnLogging.h"

// Constructor
MP3Parser::MP3Parser() : instance(nullptr) {
	print("sizeof(Mp3Instance) = ");
	nprintln(sizeof(Mp3Instance));
	Mp3Instance* a = (Mp3Instance*)calloc(1, sizeof(Mp3Instance));
	if(!a) return;
	a->cachedHeader = 0;
	a->sample_wp1 = 15;
	a->sample_wp2 = 15;
	a->frame_start = 0;
	a->bufReadPointer = 0;
	a->bufWritePointer = 0;
	//a->lastNumBytesRead = 0;
	
	for (int ch = 0; ch < 2; ch++) {
		for (int j = 0; j < 576; j++) {
			a->prevblck[ch][j] = 0;
		}
	}
	
	instance = a;
}

// Destructor
MP3Parser::~MP3Parser() {
    //end();  // Ensure proper cleanup
	free(instance);
}

int readBitsFromBuffer(char* frameBuffer, int frameSize, int* frameBufferReadPos, int bitsToRead) {
	int result = 0;
	// Ensure the number of bits to read is within a valid range
	if (bitsToRead & ~31) {//bitsToRead <= 0 || bitsToRead > 32
		//throw new IllegalArgumentException("bitsToRead must be between 1 and 32, " + bitsToRead + " is invalid.");
		//print("bitsToRead must be between 1 and 32, given: ");
		//nprintln(bitsToRead);
		return -1;
	}
	frameSize *= 8;
	int bitIndex = frameBufferReadPos[0];
	while (bitsToRead > 0) {
		if (bitIndex >= frameSize) {
			//throw new IndexOutOfBoundsException("Reached end of buffer");
			//println("Reached end of buffer");
			return -1;
		}

		// Calculate the byte index and the bit index within that byte
		int byteIndex = bitIndex >> 3;
		int bitInByte = bitIndex & 7;

		// Determine the number of bits we can read in this byte
		int bitsAvailableInByte = 8 - bitInByte;
		int bitsToReadNow = min(bitsAvailableInByte, bitsToRead);

		// Mask to extract the relevant bits
		int mask = (1 << bitsToReadNow) - 1;
		int extractedBits = (frameBuffer[byteIndex] >> (bitsAvailableInByte - bitsToReadNow)) & mask;

		// Shift the extracted bits into the result
		result <<= bitsToReadNow;
		result |= extractedBits;

		// Update the position and the remaining bits to read
		bitIndex += bitsToReadNow;
		bitsToRead -= bitsToReadNow;
	}
	frameBufferReadPos[0] = bitIndex;
	return result;
}

bool get_side_info(int header_version, Mp3Instance* mp3Instance, int channels, char* frameBuffer, int frameSize, int* frameBufferReadPos){
	int ch, gr;
	if (header_version == MPEG1) {
		//skip private bits
		readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, channels == 1 ? 5 : 3);

		for (ch=0; ch<channels; ch++) {
			mp3Instance->si_ch_scfsi[ch*4+0] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);
			mp3Instance->si_ch_scfsi[ch*4+1] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);
			mp3Instance->si_ch_scfsi[ch*4+2] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);
			mp3Instance->si_ch_scfsi[ch*4+3] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);
	   }

		for (gr=0; gr<2; gr++) {
			for (ch=0; ch<channels; ch++) {
				gr_info_s* info = &(mp3Instance->si_ch_gr[ch*2+gr]);
				info->part2_3_length = readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 12);
				info->big_values = readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 9);
				info->global_gain = readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 8);
				info->scalefac_compress = readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 4);
				info->window_switching_flag = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);
				if ((info->window_switching_flag) != 0) {
					info->block_type       = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 2);
					info->mixed_block_flag = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);

					info->table_select[0]  = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 5);
					info->table_select[1]  = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 5);

					info->subblock_gain[0] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 3);
					info->subblock_gain[1] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 3);
					info->subblock_gain[2] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 3);

					// Set region_count parameters since they are implicit in this case.

					if (info->block_type == 0) {
						// Side info bad: block_type == 0 in split block
						return 0;
					} else if (info->block_type == 2 && info->mixed_block_flag == 0) {
						info->region0_count = 8;
					} else {
						info->region0_count = 7;
					}
					info->region1_count = (char)(20 - info->region0_count);
				} else {
					info->table_select[0] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 5);
					info->table_select[1] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 5);
					info->table_select[2] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 5);
					info->region0_count = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 4);
					info->region1_count = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 3);
					info->block_type = 0;
				}
				info->preflag = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);
				info->scalefac_scale = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);
				info->count1table_select = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);
			}
		}
	} else {  	// MPEG-2 LSF, SZD: MPEG-2.5 LSF
		//skip private bit(s)
		readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, channels);

		for (ch = 0; ch < channels; ch++) {
			gr_info_s* info = &(mp3Instance->si_ch_gr[ch * 2 + 0]);

			info->part2_3_length = readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 12);
			info->big_values = readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 9);
			info->global_gain = readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 8);
			info->scalefac_compress = readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 9);
			info->window_switching_flag = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);

			if ((info->window_switching_flag) != 0) {

				info->block_type = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 2);
				info->mixed_block_flag = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);
				info->table_select[0] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 5);
				info->table_select[1] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 5);

				info->subblock_gain[0] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 3);
				info->subblock_gain[1] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 3);
				info->subblock_gain[2] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 3);

				// Set region_count parameters since they are implicit in
				// this case.

				if (info->block_type == 0) {
					// Side info bad: block_type == 0 in split block
					return 0;
				} else if (info->block_type == 2 && info->mixed_block_flag == 0) {
					info->region0_count = 8;
				} else {
					info->region0_count = 7;
					info->region1_count = (char)(20 - info->region0_count);
				}

			} else {
				info->table_select[0] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 5);
				info->table_select[1] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 5);
				info->table_select[2] = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 5);
				info->region0_count = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 4);
				info->region1_count = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 3);
				info->block_type = 0;
			}

			info->scalefac_scale = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);
			info->count1table_select = (char)readBitsFromBuffer(frameBuffer, frameSize, frameBufferReadPos, 1);
		} // for(ch=0; ch<channels; ch++)
	} // if (header.version() == MPEG1)
	return 1;
}

void hputbuf_array(Mp3Instance* mp3Instance, char* data, int offset, int len) {
	int pos = mp3Instance->bufWritePointer;
	for(int i=0; i<len; i++){
		mp3Instance->buf[pos + i] = data[offset + i];
	}
	mp3Instance->bufWritePointer = pos + len;
}

int hget1bit(Mp3Instance* mp3Instance) {
	//mp3Instance.totalBitsRead++;
	int bit = mp3Instance->bufReadPointer;
	mp3Instance->bufReadPointer++;
	//mp3Instance.bufReadPointer &= BUFSIZE_MASK;
	int bits = mp3Instance->buf[bit >> 3] & 0xFF;
	bit &= 7;
	bits = (bits >> (7 - bit)) & 1;
	//int bits = readBitsFromBuffer(mp3Instance.buf, BUFSIZE, a, 1);
	return bits;
}

int hgetbits(Mp3Instance* mp3Instance, short num) {
	int outVal = 0;
	for (short i = 0; i < num; i++) {
		outVal <<= 1;
		outVal |= hget1bit(mp3Instance);
	}
	return outVal;
}

/**
 * Do the huffman-decoding.
 * note! for counta,countb -the 4 bit value is returned in y,
 * discard x.
 */
int huffman_decoder(const huffcodetab* h, int* x, int* y, int* v, int* w, Mp3Instance* mp3Instance) {
	// array of all huffcodtable headers
	// 0..31 Huffman code table 0..31
	// 32,33 count1-tables

	int dmask = 1 << ((4 * 8) - 1);
	// int hs = 4 * 8;
	int level;
	int point = 0;
	int numBitsRead = 0;
	//int error = 1;
	level = dmask;

	if (h->val == 0) return 2;

	/* table 0 needs no bits */
	if (h->treelen == 0) {
		x[0] = y[0] = 0;
		return 0;
	}

	/* Lookup in Huffman table. */
	do {
		if (h->val[point][0] == 0) { /* end of tree */
			x[0] = (unsigned int)(h->val[point][1]) >> 4;
			y[0] = h->val[point][1] & 0xf;
			//error = 0;
			break;
		}

		// hget1bit() is called thousands of times, and so needs to be
		// ultra fast.
		numBitsRead++;
		if (hget1bit(mp3Instance) != 0) {
			while (h->val[point][1] >= MXOFF)
				point += h->val[point][1];
			point += h->val[point][1];
		} else {
			while (h->val[point][0] >= MXOFF)
				point += h->val[point][0];
			point += h->val[point][0];
		}
		//level >>>= 1;
		level = ((unsigned int)level) >> 1;
		// MDM: ht[0] is always 0;
	} while ((level != 0) || (point < 0 /* ht[0].treelen */));

	/* Process sign encodings for quadruples tables. */
	if (h->isSpecial) {
		v[0] = (y[0] >> 3) & 1;
		w[0] = (y[0] >> 2) & 1;
		x[0] = (y[0] >> 1) & 1;
		y[0] = y[0] & 1;

		/*
		 * v, w, x and y are reversed in the bitstream. switch them around
		 * to make test bistream work.
		 */

		if (v[0] != 0) {
			numBitsRead++;
			if (hget1bit(mp3Instance) != 0) {
				v[0] = -v[0];
			}
		}
		if (w[0] != 0) {
			numBitsRead++;
			if (hget1bit(mp3Instance) != 0) {
				w[0] = -w[0];
			}
		}
		if (x[0] != 0) {
			numBitsRead++;
			if (hget1bit(mp3Instance) != 0) {
				x[0] = -x[0];
			}
		}
		if (y[0] != 0) {
			numBitsRead++;
			if (hget1bit(mp3Instance) != 0) {
				y[0] = -y[0];
			}
		}
	} else {
		// Process sign and escape encodings for dual tables.
		// x and y are reversed in the test bitstream.
		// Reverse x and y here to make test bitstream work.

		if (h->linbits != 0) {
			if ((h->xlen - 1) == x[0]) {
				numBitsRead += h->linbits;
				x[0] += hgetbits(mp3Instance, h->linbits);
			}
		}
		if (x[0] != 0) {
			numBitsRead++;
			if (hget1bit(mp3Instance) != 0) {
				x[0] = -x[0];
			}
		}
		if (h->linbits != 0) {
			if ((h->ylen - 1) == y[0]) {
				numBitsRead += h->linbits;
				y[0] += hgetbits(mp3Instance, h->linbits);
			}
		}
		if (y[0] != 0) {
			numBitsRead++;
			if (hget1bit(mp3Instance) != 0) {
				y[0] = -y[0];
			}
		}
	}
	return numBitsRead;
}

int huffman_decode(int ch, int gr, gr_info_s* info, int sfreq, int localLen, int* is_1d, Mp3Instance* mp3Instance) {
	int xyvw[4];
	int* x = &(xyvw[0]);
	int* y = &(xyvw[1]);
	int* v = &(xyvw[2]);
	int* w = &(xyvw[3]);

	x[0] = 0;
	y[0] = 0;
	v[0] = 0;
	w[0] = 0;
	// int part2_start = 0;
	int part2_3_end = info->part2_3_length - localLen;
	// int num_bits;
	int region1Start;
	int region2Start;

	const huffcodetab* h = 0;

	// Find region boundary for short block case

	if (((info->window_switching_flag) != 0) && (info->block_type == 2)) {
		// Region2.
		// MS: Extrahandling for 8KHZ
		region1Start = (sfreq == 8) ? 72 : 36; // sfb[9/3]*3=36 or in case
												// 8KHZ = 72
		region2Start = 576; // No Region2 for short block case
	} else { // Find region boundary for long block case
		int buf = info->region0_count + 1;
		int buf1 = buf + info->region1_count + 1;
		
		//sfBandIndex_l[sfreq].length == const 23 for all [x] entries.
		const int sfBandIndex_l_length = 23;
		if (buf1 > sfBandIndex_l_length - 1) {
			buf1 = sfBandIndex_l_length - 1;
		}

		region1Start = sfBandIndex_l[sfreq][buf];
		region2Start = sfBandIndex_l[sfreq][buf1]; /* MI */
	}

	int index = 0;
	int huffmanBitsRead = 0;
	// Read bigvalues area
	for (int i = 0; i < (info->big_values << 1); i += 2) {
		if (i < region1Start)
			h = &(htArr[info->table_select[0]]);
		else if (i < region2Start)
			h = &(htArr[info->table_select[1]]);
		else
			h = &(htArr[info->table_select[2]]);

		huffmanBitsRead += huffman_decoder(h, x, y, v, w, mp3Instance);

		is_1d[index++] = x[0];
		is_1d[index++] = y[0];
	}

	// Read count1 area
	h = &(htArr[info->count1table_select + 32]);
	// num_bits = mp3Instance.bufReadPointer;

	while ((huffmanBitsRead < part2_3_end) && (index < 576)) {

		huffmanBitsRead += huffman_decoder(h, x, y, v, w, mp3Instance);

		is_1d[index++] = v[0];
		is_1d[index++] = w[0];
		is_1d[index++] = x[0];
		is_1d[index++] = y[0];
		// num_bits = mp3Instance.bufReadPointer;
	}
	/*
	if (num_bits > part2_3_end) {
		rewindNbits(mp3Instance, num_bits - part2_3_end);
		index -= 4;
	}
	*/
	// num_bits = mp3Instance.bufReadPointer;

	// Dismiss stuffing bits
	if (huffmanBitsRead < part2_3_end) {
		hgetbits(mp3Instance, part2_3_end - /* num_bits */huffmanBitsRead);
	}

	// Zero out rest
	int iterationCount = 576;
	if (index < 576) {
		iterationCount = index;
	}

	if (index < 0)
		index = 0;

	// may not be necessary
	for (; index < 576; index++) {
		is_1d[index] = 0;
	}

	return iterationCount;
}

int get_scale_factors(int ch, int gr, int(*scalefac_s)[13], int* scalefac_l, Mp3Instance* mp3Instance) {
	int sfb, window;
	gr_info_s* gr_info = &(mp3Instance->si_ch_gr[ch * 2 + gr]);
	int scale_comp = gr_info->scalefac_compress;

	if ((gr_info->window_switching_flag != 0) && (gr_info->block_type == 2)) {
		if ((gr_info->mixed_block_flag) != 0) { // MIXED
			int len1 = slen[0][gr_info->scalefac_compress];
			int len2 = slen[1][gr_info->scalefac_compress];
			for (sfb = 0; sfb < 8; sfb++) {
				scalefac_l[sfb] = hgetbits(mp3Instance, len1);
			}
			
			for (sfb = 3; sfb < 6; sfb++){
				for (window = 0; window < 3; window++) {
					scalefac_s[window][sfb] = hgetbits(mp3Instance, len1);
				}
			}
			
			for (sfb = 6; sfb < 12; sfb++){
				for (window = 0; window < 3; window++) {
					scalefac_s[window][sfb] = hgetbits(mp3Instance, len2);
				}
			}
			
			for (sfb = 12, window = 0; window < 3; window++) {
				scalefac_s[window][sfb] = 0;
			}
			return len1 * (8 + 3*3) + len2 * 6*3;
		} else { // SHORT
			int length0 = slen[0][scale_comp];
			int length1 = slen[1][scale_comp];
			scalefac_s[0][0] = hgetbits(mp3Instance, length0);
			scalefac_s[1][0] = hgetbits(mp3Instance, length0);
			scalefac_s[2][0] = hgetbits(mp3Instance, length0);
			scalefac_s[0][1] = hgetbits(mp3Instance, length0);
			scalefac_s[1][1] = hgetbits(mp3Instance, length0);
			scalefac_s[2][1] = hgetbits(mp3Instance, length0);
			scalefac_s[0][2] = hgetbits(mp3Instance, length0);
			scalefac_s[1][2] = hgetbits(mp3Instance, length0);
			scalefac_s[2][2] = hgetbits(mp3Instance, length0);
			scalefac_s[0][3] = hgetbits(mp3Instance, length0);
			scalefac_s[1][3] = hgetbits(mp3Instance, length0);
			scalefac_s[2][3] = hgetbits(mp3Instance, length0);
			scalefac_s[0][4] = hgetbits(mp3Instance, length0);
			scalefac_s[1][4] = hgetbits(mp3Instance, length0);
			scalefac_s[2][4] = hgetbits(mp3Instance, length0);
			scalefac_s[0][5] = hgetbits(mp3Instance, length0);
			scalefac_s[1][5] = hgetbits(mp3Instance, length0);
			scalefac_s[2][5] = hgetbits(mp3Instance, length0);
			scalefac_s[0][6] = hgetbits(mp3Instance, length1);
			scalefac_s[1][6] = hgetbits(mp3Instance, length1);
			scalefac_s[2][6] = hgetbits(mp3Instance, length1);
			scalefac_s[0][7] = hgetbits(mp3Instance, length1);
			scalefac_s[1][7] = hgetbits(mp3Instance, length1);
			scalefac_s[2][7] = hgetbits(mp3Instance, length1);
			scalefac_s[0][8] = hgetbits(mp3Instance, length1);
			scalefac_s[1][8] = hgetbits(mp3Instance, length1);
			scalefac_s[2][8] = hgetbits(mp3Instance, length1);
			scalefac_s[0][9] = hgetbits(mp3Instance, length1);
			scalefac_s[1][9] = hgetbits(mp3Instance, length1);
			scalefac_s[2][9] = hgetbits(mp3Instance, length1);
			scalefac_s[0][10] = hgetbits(mp3Instance, length1);
			scalefac_s[1][10] = hgetbits(mp3Instance, length1);
			scalefac_s[2][10] = hgetbits(mp3Instance, length1);
			scalefac_s[0][11] = hgetbits(mp3Instance, length1);
			scalefac_s[1][11] = hgetbits(mp3Instance, length1);
			scalefac_s[2][11] = hgetbits(mp3Instance, length1);
			scalefac_s[0][12] = 0;
			scalefac_s[1][12] = 0;
			scalefac_s[2][12] = 0;
			return length0*18 + length1*18;
		} // SHORT
	} else { // LONG types 0,1,3
		int length0 = slen[0][scale_comp];
		int length1 = slen[1][scale_comp];
		int len = 0;
		if ((mp3Instance->si_ch_scfsi[ch * 4 + 0] == 0) || (gr == 0)) {
			scalefac_l[0] = hgetbits(mp3Instance, length0);
			scalefac_l[1] = hgetbits(mp3Instance, length0);
			scalefac_l[2] = hgetbits(mp3Instance, length0);
			scalefac_l[3] = hgetbits(mp3Instance, length0);
			scalefac_l[4] = hgetbits(mp3Instance, length0);
			scalefac_l[5] = hgetbits(mp3Instance, length0);
			len = length0*6;
		}
		if ((mp3Instance->si_ch_scfsi[ch * 4 + 1] == 0) || (gr == 0)) {
			scalefac_l[6] = hgetbits(mp3Instance, length0);
			scalefac_l[7] = hgetbits(mp3Instance, length0);
			scalefac_l[8] = hgetbits(mp3Instance, length0);
			scalefac_l[9] = hgetbits(mp3Instance, length0);
			scalefac_l[10] = hgetbits(mp3Instance, length0);
			len += length0*5;
		}
		if ((mp3Instance->si_ch_scfsi[ch * 4 + 2] == 0) || (gr == 0)) {
			scalefac_l[11] = hgetbits(mp3Instance, length1);
			scalefac_l[12] = hgetbits(mp3Instance, length1);
			scalefac_l[13] = hgetbits(mp3Instance, length1);
			scalefac_l[14] = hgetbits(mp3Instance, length1);
			scalefac_l[15] = hgetbits(mp3Instance, length1);
			len += length1*5;
		}
		if ((mp3Instance->si_ch_scfsi[ch * 4 + 3] == 0) || (gr == 0)) {
			scalefac_l[16] = hgetbits(mp3Instance, length1);
			scalefac_l[17] = hgetbits(mp3Instance, length1);
			scalefac_l[18] = hgetbits(mp3Instance, length1);
			scalefac_l[19] = hgetbits(mp3Instance, length1);
			scalefac_l[20] = hgetbits(mp3Instance, length1);
			len += length1*5;
		}
		scalefac_l[21] = 0;
		scalefac_l[22] = 0;
		return len;
	}
}

int get_LSF_scale_factors(int ch, int gr, int header_mode_extension, gr_info_s* gr_info, int(*scalefac_s)[13] , int* scalefac_l, Mp3Instance* mp3Instance){
	int readLen = 0;
	int scalefac_buffer[54];
	
	//get_LSF_scale_data(gr_info, ch, header_mode_extension, scalefac_buffer);
	{
		int new_slen[4];
		int scalefac_comp, int_scalefac_comp;
		int m;
		int blocktypenumber;
		int blocknumber = 0;

		scalefac_comp =  gr_info->scalefac_compress;

		if (gr_info->block_type == 2) {
			if (gr_info->mixed_block_flag == 0) {
				blocktypenumber = 1;
			} else if (gr_info->mixed_block_flag == 1) {
				blocktypenumber = 2;
			} else {
				blocktypenumber = 0;
			}
		} else {
			blocktypenumber = 0;
		}

	   if(!(((header_mode_extension == 1) || (header_mode_extension == 3)) && (ch == 1))) {

			if(scalefac_comp < 400) {

				new_slen[0] = ((unsigned int)scalefac_comp >> 4) / 5 ;
				new_slen[1] = ((unsigned int)scalefac_comp >> 4) % 5 ;
				new_slen[2] = (unsigned int)(scalefac_comp & 0xF) >> 2 ;
				new_slen[3] = (scalefac_comp & 3);
				gr_info->preflag = 0;
				blocknumber = 0;

		   } else if (scalefac_comp  < 500) {

				new_slen[0] = ((unsigned int)(scalefac_comp - 400) >> 2) / 5 ;
				new_slen[1] = ((unsigned int)(scalefac_comp - 400) >> 2) % 5 ;
				new_slen[2] = (scalefac_comp - 400 ) & 3 ;
				new_slen[3] = 0;
				gr_info->preflag = 0;
				blocknumber = 1;

		   } else if (scalefac_comp < 512) {

				new_slen[0] = (scalefac_comp - 500 ) / 3 ;
				new_slen[1] = (scalefac_comp - 500)  % 3 ;
				new_slen[2] = 0;
				new_slen[3] = 0;
				gr_info->preflag = 1;
				blocknumber = 2;
		   }
	   }

	   if((((header_mode_extension == 1) || (header_mode_extension == 3)) && (ch == 1)))
	   {
		  int_scalefac_comp = (unsigned int)scalefac_comp >> 1;

		  if (int_scalefac_comp < 180)
		  {
				new_slen[0] = int_scalefac_comp  / 36 ;
				new_slen[1] = (int_scalefac_comp % 36 ) / 6 ;
				new_slen[2] = (int_scalefac_comp % 36) % 6;
				new_slen[3] = 0;
				gr_info->preflag = 0;
				blocknumber = 3;
		  } else if (int_scalefac_comp < 244) {
				new_slen[0] = (unsigned int)((int_scalefac_comp - 180 )  & 0x3F) >> 4 ;
				new_slen[1] = (unsigned int)((int_scalefac_comp - 180) & 0xF) >> 2 ;
				new_slen[2] = (int_scalefac_comp - 180 ) & 3 ;
				new_slen[3] = 0;
				gr_info->preflag = 0;
				blocknumber = 4;
		  } else if (int_scalefac_comp < 255) {
				new_slen[0] = (int_scalefac_comp - 244 ) / 3 ;
				new_slen[1] = (int_scalefac_comp - 244 )  % 3 ;
				new_slen[2] = 0;
				new_slen[3] = 0;
				gr_info->preflag = 0;
				blocknumber = 5;
		  }
	   }
		// why 45, not 54?
		for (int x = 0; x < 45; x++) {
			scalefac_buffer[x] = 0;
		}

	   m = 0;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < nr_of_sfb_block[blocknumber][blocktypenumber][i]; j++) {
				scalefac_buffer[m] = (new_slen[i] == 0) ? 0 : hgetbits(mp3Instance,new_slen[i]);
				readLen += new_slen[i];
				m++;

			} // for (unint32 j ...
		} // for (uint32 i ...
	}

	int sfb, window;

	int m = 0;

	if ((gr_info->window_switching_flag != 0) && (gr_info->block_type == 2)) {
		if (gr_info->mixed_block_flag != 0) { // MIXED
			for (sfb = 0; sfb < 8; sfb++) {
				scalefac_l[sfb] = scalefac_buffer[m];
				m++;
			}
			for (sfb = 3; sfb < 12; sfb++) {
				for (window = 0; window < 3; window++) {
					scalefac_s[window][sfb] = scalefac_buffer[m];
					m++;
				}
			}
			for (window = 0; window < 3; window++)
				scalefac_s[window][12] = 0;

		} else { // SHORT

			for (sfb = 0; sfb < 12; sfb++) {
				for (window = 0; window < 3; window++) {
					scalefac_s[window][sfb] = scalefac_buffer[m];
					m++;
				}
			}

			for (window = 0; window < 3; window++)
				scalefac_s[window][12] = 0;
		}
	} else { // LONG types 0,1,3

		for (sfb = 0; sfb < 21; sfb++) {
			scalefac_l[sfb] = scalefac_buffer[m];
			m++;
		}
		scalefac_l[21] = 0; // Jeff
		scalefac_l[22] = 0;
	}
	return readLen;
}


const int fast_pow2_scale4_fractionTable[4] = {
	0,
	0b00110000011011111110000,
	0b01101010000010011110011,
	0b10101110100010011111101
};
// Split into integer and fractional parts
float fast_pow2_scale4(int param) {
	int integer_part = param >> 2;
	integer_part = (127 + integer_part) << 23; // 127 is the bias for the exponent
	integer_part |= fast_pow2_scale4_fractionTable[param & 3];
	//return Float.intBitsToFloat(integer_part);
	return *(float*)&integer_part;//interpret now as float.
	//return result_int;
}

inline float fastCubeRoot(float x) {//expecting x = integer, 0 .. 8192 !!!
    if (x <= 1) return x;  // Cube root of 0 is 0
	int i = *(int*)&x;  // Treat the float as an int
    i = i / 3 + 709921077;  // Initial guess using bit manipulation
    float guess = *(float*)&i;
    //float guess = x / 3.0f;  // Initial guess
    for (int i = 0; i < 3; ++i) {  // Typically, 2-3 iterations are sufficient
        guess = (2.0f * guess + x / (guess * guess)) / 3.0f;
    }
    return guess;
}

inline float pow43(int x){
	float y = fastCubeRoot(x);
	y *= y;
	y *= y;
	return y;
}

void dequantize_sample(float(*xr_1d)[SS_LIMIT], int ch, int gr, int(*scalefac_s)[13], int* scalefac_l,
		gr_info_s* gr_info, int sfreq, int* is_1d, int iterationCount) {
	int next_cb_boundary;
	int cb_width;
	int index = 0, t_index, j;
	float g_gain;
	// choose correct scalefactor band per block type, initalize boundary
	boolean windowCheck = gr_info->window_switching_flag != 0;
	boolean blockType2Check = gr_info->block_type == 2;
	boolean isMixedBlock = gr_info->mixed_block_flag != 0;
	boolean windowAndBlock = windowCheck & blockType2Check;
	
	if (windowAndBlock && !isMixedBlock) {
		cb_width = sfBandIndex_s[sfreq][1];
		next_cb_boundary = (cb_width << 2) - cb_width;
	} else {
		cb_width = 0;
		// LONG blocks: 0,1,3
		next_cb_boundary = sfBandIndex_l[sfreq][1];
	}
	
	// Compute overall (global) scaling.
	int temp = gr_info->global_gain - 210;
	if(temp >= -127 && temp <= 127){
		g_gain = fast_pow2_scale4(temp);
	} else {
		g_gain = (float)pow(2.0, (0.25 * temp));
	}

	int cb = 0;
	int cb_begin = 0;
	for (j = 0; j < iterationCount; j++) {
		// Modif E.B 02/22/99
		int reste = j % SS_LIMIT;
		int quotien = (int) ((j - reste) / SS_LIMIT);
		int abv = is_1d[j];
		if (abv == 0) {
			xr_1d[quotien][reste] = 0;
		} else {
			int isNeg = abv < 0 ? -1 : 1;
			abv *= isNeg;
			float val;
			if (abv < t_43_length) {
				//val = POW43_Table[abv];
				val = pow43(abv);//POW43_Table[abv];//mp3Static->t_43[abv];
			} else {
				const float d43 = ((float)4.0 / 3.0);
				val = (float)pow(abv, d43);
			}
			xr_1d[quotien][reste] = val * g_gain * isNeg;
		}
	}

	// apply formula per block type
	for (j = 0; j < iterationCount; j++) {
		// Modif E.B 02/22/99
		int reste = j % SS_LIMIT;
		int quotien = (int) ((j - reste) / SS_LIMIT);

		if (index == next_cb_boundary) { /* Adjust critical band boundary */
			if (windowAndBlock) {
				if (isMixedBlock) {

					if (index == sfBandIndex_l[sfreq][8]) {
						next_cb_boundary = sfBandIndex_s[sfreq][4];
						next_cb_boundary = (next_cb_boundary << 2) - next_cb_boundary;
						cb = 3;
						cb_width = sfBandIndex_s[sfreq][4] - sfBandIndex_s[sfreq][3];

						cb_begin = sfBandIndex_s[sfreq][3];
						cb_begin = (cb_begin << 2) - cb_begin;

					} else if (index < sfBandIndex_l[sfreq][8]) {
						next_cb_boundary = sfBandIndex_l[sfreq][(++cb) + 1];
					} else {
						next_cb_boundary = sfBandIndex_s[sfreq][(++cb) + 1];
						next_cb_boundary = (next_cb_boundary << 2) - next_cb_boundary;

						cb_begin = sfBandIndex_s[sfreq][cb];
						cb_width = sfBandIndex_s[sfreq][cb + 1] - cb_begin;
						cb_begin = (cb_begin << 2) - cb_begin;
					}

				} else {
					next_cb_boundary = sfBandIndex_s[sfreq][(++cb) + 1];
					next_cb_boundary = (next_cb_boundary << 2) - next_cb_boundary;

					cb_begin = sfBandIndex_s[sfreq][cb];
					cb_width = sfBandIndex_s[sfreq][cb + 1] - cb_begin;
					cb_begin = (cb_begin << 2) - cb_begin;
				}

			} else { // long blocks
				next_cb_boundary = sfBandIndex_l[sfreq][(++cb) + 1];
			}
		}
		// Do long/short dependent scaling operations

		if (windowCheck && (
				(blockType2Check && !isMixedBlock) || ((gr_info->block_type == 2) && isMixedBlock && (j >= 36))
			)) {

			t_index = (index - cb_begin) / cb_width;
			// xr[sb][ss] *= pow(2.0, ((-2.0 * gr_info->subblock_gain[t_index]) -(0.5 * (1.0 + gr_info->scalefac_scale) scalefac[ch].s[t_index][cb])));
			int idx = scalefac_s[t_index][cb] << gr_info->scalefac_scale;
			idx += (gr_info->subblock_gain[t_index] << 2);

			xr_1d[quotien][reste] *= two_to_negative_half_pow[idx];

		} else { // LONG block types 0,1,3 & 1st 2 subbands of switched
					// blocks
			
			  //xr[sb][ss] *= pow(2.0, -0.5 * (1.0+gr_info->scalefac_scale)
			  //(scalefac[ch].l[cb] + gr_info->preflag * pretab[cb]));
			 
			int idx = scalefac_l[cb];

			if (gr_info->preflag != 0){
				idx += pretab[cb];
			}

			idx = idx << gr_info->scalefac_scale;
			xr_1d[quotien][reste] *= two_to_negative_half_pow[idx];
		}
		index++;
	}

	for (j = iterationCount; j < 576; j++) {
		// Modif E.B 02/22/99
		int reste = j % SS_LIMIT;
		int quotien = (int) ((j - reste) / SS_LIMIT);
		if (reste < 0) {
			reste = 0;
		}
		if (quotien < 0) {
			quotien = 0;
		}
		xr_1d[quotien][reste] = 0.0f;
	}

	return;
}



void reorder(float(*xr_1d)[SS_LIMIT], int ch, int gr, gr_info_s* gr_info, int sfreq, float* out_1d) {
	int freq, freq3;
	int index;
	int sfb, sfb_start, sfb_lines;
	int src_line, des_line;
	//float[][] xr_1d = xr;

	if ((gr_info->window_switching_flag != 0) && (gr_info->block_type == 2)) {

		for (index = 0; index < 576; index++)
			out_1d[index] = 0.0f;

		if (gr_info->mixed_block_flag != 0) {
			// NO REORDER FOR LOW 2 SUBBANDS
			for (index = 0; index < 36; index++) {
				// Modif E.B 02/22/99
				int reste = index % SS_LIMIT;
				int quotien = (int) ((index - reste) / SS_LIMIT);
				out_1d[index] = xr_1d[quotien][reste];
			}
			// REORDERING FOR REST SWITCHED SHORT

			for (sfb = 3; sfb < 13; sfb++) {
				sfb_start = sfBandIndex_s[sfreq][sfb];
				sfb_lines = sfBandIndex_s[sfreq][sfb + 1] - sfb_start;

				int sfb_start3 = (sfb_start << 2) - sfb_start;

				for (freq = 0, freq3 = 0; freq < sfb_lines; freq++, freq3 += 3) {

					src_line = sfb_start3 + freq;
					des_line = sfb_start3 + freq3;
					// Modif E.B 02/22/99
					int reste = src_line % SS_LIMIT;
					int quotien = (int) ((src_line - reste) / SS_LIMIT);

					out_1d[des_line] = xr_1d[quotien][reste];
					src_line += sfb_lines;
					des_line++;

					reste = src_line % SS_LIMIT;
					quotien = (int) ((src_line - reste) / SS_LIMIT);

					out_1d[des_line] = xr_1d[quotien][reste];
					src_line += sfb_lines;
					des_line++;

					reste = src_line % SS_LIMIT;
					quotien = (int) ((src_line - reste) / SS_LIMIT);

					out_1d[des_line] = xr_1d[quotien][reste];
				}
			}

		} else { // pure short
			/*
			for (index = 0; index < 576; index++) {
				int j = mp3Static->reorder_table[sfreq][index];
				int reste = j % SS_LIMIT;
				int quotien = (int) ((j - reste) / SS_LIMIT);
				out_1d[index] = xr_1d[quotien][reste];
			}
			*/
			unsigned char* scalefac_band = (unsigned char*)&(sfBandIndex_s[sfreq][0]);
			short j = 0;
			for(int sfb = 0; sfb < 13; sfb++) {
				unsigned char start = scalefac_band[sfb];
				unsigned char end   = scalefac_band[sfb + 1];
				for(int window = 0; window < 3; window++){
					for(unsigned char i = start; i < end; i++){
						index = 3 * i + window;
						//ix_out[index] = j;
						
						int reste = j % SS_LIMIT;
						int quotien = (int) ((j - reste) / SS_LIMIT);
						out_1d[index] = xr_1d[quotien][reste];
						
						j++;
					}
				}
			}
			
			
			
			
		}
	} else { // long blocks
		for (index = 0; index < 576; index++) {
			// Modif E.B 02/22/99
			int reste = index % SS_LIMIT;
			int quotien = (int) ((index - reste) / SS_LIMIT);
			out_1d[index] = xr_1d[quotien][reste];
		}
	}
}

void i_stereo_k_values(char is_pos, int io_type, int i, Mp3Instance* mp3Instance) {
	/*
	if (is_pos == 0) {
		mp3Instance->k[0][i] = 1;
		mp3Instance->k[1][i] = 1;
	} else if ((is_pos & 1) != 0) {
		mp3Instance->k[0][i] = io[io_type][(unsigned int)(is_pos + 1) >> 1];
		mp3Instance->k[1][i] = 1;
	} else {
		mp3Instance->k[0][i] = 1;
		mp3Instance->k[1][i] = io[io_type][(unsigned int)is_pos >> 1];
	}
	*/
	if (is_pos == 0) {
		mp3Instance->k[0][i] = 0;
		mp3Instance->k[1][i] = 0;
	} else if ((is_pos & 1) != 0) {
		mp3Instance->k[0][i] = ((unsigned int)(is_pos + 1) >> 1) + (io_type * 32);
		mp3Instance->k[1][i] = 0;
	} else {
		mp3Instance->k[0][i] = 0;
		mp3Instance->k[1][i] = ((unsigned int)is_pos >> 1) + (io_type * 32);
	}
}

void stereo(int gr, int header_version, int header_mode_extension, gr_info_s* gr_info,
		Mp3Instance* mp3Instance, int sfreq, int channels, int header_mode, float(*ro)[SB_LIMIT][SS_LIMIT], float(*lr)[SB_LIMIT][SS_LIMIT]) {
	int sb, ss;
	
	if (channels == 1) { // mono , bypass xr[0][][] to lr[0][][]

		for (sb = 0; sb < SB_LIMIT; sb++){
			for (ss = 0; ss < SS_LIMIT; ss += 3) {
				lr[0][sb][ss] = ro[0][sb][ss];
				lr[0][sb][ss + 1] = ro[0][sb][ss + 1];
				lr[0][sb][ss + 2] = ro[0][sb][ss + 2];
			}
		}
	} else {
		int mode_ext = header_mode_extension;
		int sfb;
		int i;
		int lines, temp, temp2;

		bool ms_stereo = ((header_mode == JOINT_STEREO) && ((mode_ext & 0x2) != 0));
		bool i_stereo = ((header_mode == JOINT_STEREO) && ((mode_ext & 0x1) != 0));
		bool lsf = ((header_version == MPEG2_LSF || header_version == MPEG25_LSF)); // SZD

		int io_type = (gr_info->scalefac_compress & 1);

		// initialization

		for (i = 0; i < 576; i++) {
			mp3Instance->is_pos[i] = 7;
			mp3Instance->is_ratio[i] = 0;
		}

		if (i_stereo) {
			if ((gr_info->window_switching_flag != 0) && (gr_info->block_type == 2)) {
				if (gr_info->mixed_block_flag != 0) {

					int max_sfb = 0;

					for (int j = 0; j < 3; j++) {
						int sfbcnt;
						sfbcnt = 2;
						for (sfb = 12; sfb >= 3; sfb--) {
							i = sfBandIndex_s[sfreq][sfb];
							lines = sfBandIndex_s[sfreq][sfb + 1] - i;
							i = (i << 2) - i + (j + 1) * lines - 1;

							while (lines > 0) {
								if (ro[1][i / 18][i % 18] != 0.0f) {
									sfbcnt = sfb;
									sfb = -10;
									lines = -10;
								}

								lines--;
								i--;

							} // while (lines > 0)

						} // for (sfb=12 ...
						sfb = sfbcnt + 1;

						if (sfb > max_sfb)
							max_sfb = sfb;

						while (sfb < 12) {
							temp = sfBandIndex_s[sfreq][sfb];
							sb = sfBandIndex_s[sfreq][sfb + 1] - temp;
							i = (temp << 2) - temp + j * sb;

							for (; sb > 0; sb--) {
								mp3Instance->is_pos[i] = (char)mp3Instance->scalefac1_s[j][sfb];
								if (mp3Instance->is_pos[i] != 7)
									if (lsf) {
										i_stereo_k_values(mp3Instance->is_pos[i], io_type, i, mp3Instance);
									} else {
										mp3Instance->is_ratio[i] = mp3Instance->is_pos[i];
									}
								i++;
							} // for (; sb>0...
							sfb++;
						} // while (sfb < 12)
						sfb = sfBandIndex_s[sfreq][10];
						sb = sfBandIndex_s[sfreq][11] - sfb;
						sfb = (sfb << 2) - sfb + j * sb;
						temp = sfBandIndex_s[sfreq][11];
						sb = sfBandIndex_s[sfreq][12] - temp;
						i = (temp << 2) - temp + j * sb;

						for (; sb > 0; sb--) {
							mp3Instance->is_pos[i] = mp3Instance->is_pos[sfb];

							if (lsf) {
								mp3Instance->k[0][i] = mp3Instance->k[0][sfb];
								mp3Instance->k[1][i] = mp3Instance->k[1][sfb];
							} else {
								mp3Instance->is_ratio[i] = mp3Instance->is_ratio[sfb];
							}
							i++;
						} // for (; sb > 0 ...
					}
					if (max_sfb <= 3) {
						i = 2;
						ss = 17;
						sb = -1;
						while (i >= 0) {
							if (ro[1][i][ss] != 0.0f) {
								sb = (i << 4) + (i << 1) + ss;
								i = -1;
							} else {
								ss--;
								if (ss < 0) {
									i--;
									ss = 17;
								}
							} // if (ro ...
						} // while (i>=0)
						i = 0;
						while (sfBandIndex_l[sfreq][i] <= sb)
							i++;
						sfb = i;
						i = sfBandIndex_l[sfreq][i];
						for (; sfb < 8; sfb++) {
							sb = sfBandIndex_l[sfreq][sfb + 1]
									- sfBandIndex_l[sfreq][sfb];
							for (; sb > 0; sb--) {
								mp3Instance->is_pos[i] = (char)mp3Instance->scalefac1_l[sfb];
								if (mp3Instance->is_pos[i] != 7)
									if (lsf) {
										i_stereo_k_values(mp3Instance->is_pos[i], io_type, i, mp3Instance);
									} else {
										mp3Instance->is_ratio[i] = mp3Instance->is_pos[i];
									}
								i++;
							} // for (; sb>0 ...
						} // for (; sfb<8 ...
					} // for (j=0 ...
				} else { // if (gr_info.mixed_block_flag)
					for (int j = 0; j < 3; j++) {
						int sfbcnt;
						sfbcnt = -1;
						for (sfb = 12; sfb >= 0; sfb--) {
							temp = sfBandIndex_s[sfreq][sfb];
							lines = sfBandIndex_s[sfreq][sfb + 1] - temp;
							i = (temp << 2) - temp + (j + 1) * lines - 1;

							while (lines > 0) {
								if (ro[1][i / 18][i % 18] != 0.0f) {
									sfbcnt = sfb;
									sfb = -10;
									lines = -10;
								}
								lines--;
								i--;
							} // while (lines > 0) */

						} // for (sfb=12 ...
						sfb = sfbcnt + 1;
						while (sfb < 12) {
							temp = sfBandIndex_s[sfreq][sfb];
							sb = sfBandIndex_s[sfreq][sfb + 1] - temp;
							i = (temp << 2) - temp + j * sb;
							for (; sb > 0; sb--) {
								mp3Instance->is_pos[i] = (char)mp3Instance->scalefac1_s[j][sfb];
								if (mp3Instance->is_pos[i] != 7)
									if (lsf) {
										i_stereo_k_values(mp3Instance->is_pos[i], io_type, i, mp3Instance);
									} else {
										mp3Instance->is_ratio[i] = mp3Instance->is_pos[i];
									}
								i++;
							} // for (; sb>0 ...
							sfb++;
						} // while (sfb<12)

						temp = sfBandIndex_s[sfreq][10];
						temp2 = sfBandIndex_s[sfreq][11];
						sb = temp2 - temp;
						sfb = (temp << 2) - temp + j * sb;
						sb = sfBandIndex_s[sfreq][12] - temp2;
						i = (temp2 << 2) - temp2 + j * sb;

						for (; sb > 0; sb--) {
							mp3Instance->is_pos[i] = mp3Instance->is_pos[sfb];

							if (lsf) {
								mp3Instance->k[0][i] = mp3Instance->k[0][sfb];
								mp3Instance->k[1][i] = mp3Instance->k[1][sfb];
							} else {
								mp3Instance->is_ratio[i] = mp3Instance->is_ratio[sfb];
							}
							i++;
						} // for (; sb>0 ...
					} // for (sfb=12
				} // for (j=0 ...
			} else { // if (gr_info.window_switching_flag ...
				i = 31;
				ss = 17;
				sb = 0;
				while (i >= 0) {
					if (ro[1][i][ss] != 0.0f) {
						sb = (i << 4) + (i << 1) + ss;
						i = -1;
					} else {
						ss--;
						if (ss < 0) {
							i--;
							ss = 17;
						}
					}
				}
				i = 0;
				while (sfBandIndex_l[sfreq][i] <= sb)
					i++;

				sfb = i;
				i = sfBandIndex_l[sfreq][i];
				for (; sfb < 21; sfb++) {
					sb = sfBandIndex_l[sfreq][sfb + 1] - sfBandIndex_l[sfreq][sfb];
					for (; sb > 0; sb--) {
						mp3Instance->is_pos[i] = (char)mp3Instance->scalefac1_l[sfb];
						if (mp3Instance->is_pos[i] != 7)
							if (lsf) {
								i_stereo_k_values(mp3Instance->is_pos[i], io_type, i, mp3Instance);
							} else {
								mp3Instance->is_ratio[i] = mp3Instance->is_pos[i];
							}
						i++;
					}
				}
				sfb = sfBandIndex_l[sfreq][20];
				for (sb = 576 - sfBandIndex_l[sfreq][21]; (sb > 0) && (i < 576); sb--) {
					mp3Instance->is_pos[i] = mp3Instance->is_pos[sfb];
					// error here: i>=576
					if (lsf) {
						mp3Instance->k[0][i] = mp3Instance->k[0][sfb];
						mp3Instance->k[1][i] = mp3Instance->k[1][sfb];
					} else {
						mp3Instance->is_ratio[i] = mp3Instance->is_ratio[sfb];
					}
					i++;
				} // if (gr_info.mixed_block_flag)
			} // if (gr_info.window_switching_flag ...
		} // if (i_stereo)

		i = 0;
		for (sb = 0; sb < SB_LIMIT; sb++){
			for (ss = 0; ss < SS_LIMIT; ss++) {
				if (mp3Instance->is_pos[i] == 7) {
					if (ms_stereo) {
						lr[0][sb][ss] = (ro[0][sb][ss] + ro[1][sb][ss]) * 0.707106781f;
						lr[1][sb][ss] = (ro[0][sb][ss] - ro[1][sb][ss]) * 0.707106781f;
					} else {
						lr[0][sb][ss] = ro[0][sb][ss];
						lr[1][sb][ss] = ro[1][sb][ss];
					}
				} else if (i_stereo) {
					if (lsf) {
						lr[0][sb][ss] = ro[0][sb][ss] * io_v2[ mp3Instance->k[0][i] ];
						lr[1][sb][ss] = ro[0][sb][ss] * io_v2[ mp3Instance->k[1][i] ];
					} else {
						float tan = TAN12[mp3Instance->is_ratio[i]];
						lr[1][sb][ss] = ro[0][sb][ss] / (float) (1 + tan);
						lr[0][sb][ss] = lr[1][sb][ss] * tan;
					}
				}
				i++;
			}
		}
	}
}

void antialias(int ch, int gr, gr_info_s* gr_info, float* out_1d) {
	int sb18, ss, sb18lim;
	// 31 alias-reduction operations between each pair of sub-bands
	// with 8 butterflies between each pair
	bool a = (gr_info->window_switching_flag != 0) & (gr_info->block_type == 2);
	bool c = gr_info->mixed_block_flag != 0;
	if (a && !c)
		return;

	if (a && c) {
		sb18lim = 18;
	} else {
		sb18lim = 558;
	}

	for (sb18 = 0; sb18 < sb18lim; sb18 += 18) {
		for (ss = 0; ss < 8; ss++) {
			int src_idx1 = sb18 + 17 - ss;
			int src_idx2 = sb18 + 18 + ss;
			float bu = out_1d[src_idx1];
			float bd = out_1d[src_idx2];
			out_1d[src_idx1] = (bu * cs[ss]) - (bd * ca[ss]);
			out_1d[src_idx2] = (bd * cs[ss]) + (bu * ca[ss]);
		}
	}
}

/**
 * Fast INV_MDCT.
 */
void inv_mdct(float* in, float* out, int block_type) {
	int i;

	float tmpf_0, tmpf_1, tmpf_2, tmpf_3, tmpf_4, tmpf_5, tmpf_6, tmpf_7, tmpf_8, tmpf_9;
	float tmpf_10, tmpf_11, tmpf_12, tmpf_13, tmpf_14, tmpf_15, tmpf_16, tmpf_17;

	tmpf_0 = tmpf_1 = tmpf_2 = tmpf_3 = tmpf_4 = tmpf_5 = tmpf_6 = tmpf_7 = tmpf_8 = tmpf_9 = tmpf_10 = tmpf_11 = tmpf_12 = tmpf_13 = tmpf_14 = tmpf_15 = tmpf_16 = tmpf_17 = 0.0f;

	if (block_type == 2) {

		for (int ii = 0; ii < 36; ii++) {
			out[ii] = 0;
		}

		int six_i = 0;

		for (i = 0; i < 3; i++) {
			// 12 point IMDCT
			// Begin 12 point IDCT
			// Input aliasing for 12 pt IDCT
			in[15 + i] += in[12 + i];
			in[12 + i] += in[9 + i];
			in[9 + i] += in[6 + i];
			in[6 + i] += in[3 + i];
			in[3 + i] += in[0 + i];

			// Input aliasing on odd indices (for 6 point IDCT)
			in[15 + i] += in[9 + i];
			in[9 + i] += in[3 + i];

			// 3 point IDCT on even indices
			float pp1, pp2, sum;
			pp2 = in[12 + i] * 0.500000000f;
			pp1 = in[6 + i] * 0.866025403f;
			sum = in[0 + i] + pp2;
			tmpf_1 = in[0 + i] - in[12 + i];
			tmpf_0 = sum + pp1;
			tmpf_2 = sum - pp1;

			// End 3 point IDCT on even indices
			// 3 point IDCT on odd indices (for 6 point IDCT)
			pp2 = in[15 + i] * 0.500000000f;
			pp1 = in[9 + i] * 0.866025403f;
			sum = in[3 + i] + pp2;
			tmpf_4 = in[3 + i] - in[15 + i];
			tmpf_5 = sum + pp1;
			tmpf_3 = sum - pp1;
			// End 3 point IDCT on odd indices
			// Twiddle factors on odd indices (for 6 point IDCT)

			tmpf_3 *= 1.931851653f;
			tmpf_4 *= 0.707106781f;
			tmpf_5 *= 0.517638090f;

			// Output butterflies on 2 3 point IDCT's (for 6 point IDCT)
			float save = tmpf_0;
			tmpf_0 += tmpf_5;
			tmpf_5 = save - tmpf_5;
			save = tmpf_1;
			tmpf_1 += tmpf_4;
			tmpf_4 = save - tmpf_4;
			save = tmpf_2;
			tmpf_2 += tmpf_3;
			tmpf_3 = save - tmpf_3;

			// End 6 point IDCT
			// Twiddle factors on indices (for 12 point IDCT)

			tmpf_0 *= 0.504314480f;
			tmpf_1 *= 0.541196100f;
			tmpf_2 *= 0.630236207f;
			tmpf_3 *= 0.821339815f;
			tmpf_4 *= 1.306562965f;
			tmpf_5 *= 3.830648788f;

			// End 12 point IDCT

			// Shift to 12 point modified IDCT, multiply by window type 2
			tmpf_8 = -tmpf_0 * 0.793353340f;
			tmpf_9 = -tmpf_0 * 0.608761429f;
			tmpf_7 = -tmpf_1 * 0.923879532f;
			tmpf_10 = -tmpf_1 * 0.382683432f;
			tmpf_6 = -tmpf_2 * 0.991444861f;
			tmpf_11 = -tmpf_2 * 0.130526192f;

			tmpf_0 = tmpf_3;
			tmpf_1 = tmpf_4 * 0.382683432f;
			tmpf_2 = tmpf_5 * 0.608761429f;

			tmpf_3 = -tmpf_5 * 0.793353340f;
			tmpf_4 = -tmpf_4 * 0.923879532f;
			tmpf_5 = -tmpf_0 * 0.991444861f;

			tmpf_0 *= 0.130526192f;

			out[six_i + 6] += tmpf_0;
			out[six_i + 7] += tmpf_1;
			out[six_i + 8] += tmpf_2;
			out[six_i + 9] += tmpf_3;
			out[six_i + 10] += tmpf_4;
			out[six_i + 11] += tmpf_5;
			out[six_i + 12] += tmpf_6;
			out[six_i + 13] += tmpf_7;
			out[six_i + 14] += tmpf_8;
			out[six_i + 15] += tmpf_9;
			out[six_i + 16] += tmpf_10;
			out[six_i + 17] += tmpf_11;

			six_i += 6;
		}
	} else {
		// 36 point IDCT
		// input aliasing for 36 point IDCT
		in[17] += in[16];
		in[16] += in[15];
		in[15] += in[14];
		in[14] += in[13];
		in[13] += in[12];
		in[12] += in[11];
		in[11] += in[10];
		in[10] += in[9];
		in[9] += in[8];
		in[8] += in[7];
		in[7] += in[6];
		in[6] += in[5];
		in[5] += in[4];
		in[4] += in[3];
		in[3] += in[2];
		in[2] += in[1];
		in[1] += in[0];

		// 18 point IDCT for odd indices
		// input aliasing for 18 point IDCT
		in[17] += in[15];
		in[15] += in[13];
		in[13] += in[11];
		in[11] += in[9];
		in[9] += in[7];
		in[7] += in[5];
		in[5] += in[3];
		in[3] += in[1];

		float tmp0, tmp1, tmp2, tmp3, tmp4, tmp0_, tmp1_, tmp2_, tmp3_;
		float tmp0o, tmp1o, tmp2o, tmp3o, tmp4o, tmp0_o, tmp1_o, tmp2_o, tmp3_o;

		// Fast 9 Point Inverse Discrete Cosine Transform
		//
		// By Francois-Raymond Boyer
		// mailto:boyerf@iro.umontreal.ca
		// http://www.iro.umontreal.ca/~boyerf
		//
		// The code has been optimized for Intel processors
		// (takes a lot of time to convert float to and from iternal FPU
		// representation)
		//
		// It is a simple "factorization" of the IDCT matrix.

		// 9 point IDCT on even indices

		// 5 points on odd indices (not realy an IDCT)
		float i00 = in[0] + in[0];
		float iip12 = i00 + in[12];

		tmp0 = iip12 + in[4] * 1.8793852415718f + in[8] * 1.532088886238f + in[16] * 0.34729635533386f;
		tmp1 = i00 + in[4] - in[8] - in[12] - in[12] - in[16];
		tmp2 = iip12 - in[4] * 0.34729635533386f - in[8] * 1.8793852415718f + in[16] * 1.532088886238f;
		tmp3 = iip12 - in[4] * 1.532088886238f + in[8] * 0.34729635533386f - in[16] * 1.8793852415718f;
		tmp4 = in[0] - in[4] + in[8] - in[12] + in[16];

		// 4 points on even indices
		float i66_ = in[6] * 1.732050808f; // Sqrt[3]

		tmp0_ = in[2] * 1.9696155060244f + i66_ + in[10] * 1.2855752193731f + in[14] * 0.68404028665134f;
		tmp1_ = (in[2] - in[10] - in[14]) * 1.732050808f;
		tmp2_ = in[2] * 1.2855752193731f - i66_ - in[10] * 0.68404028665134f + in[14] * 1.9696155060244f;
		tmp3_ = in[2] * 0.68404028665134f - i66_ + in[10] * 1.9696155060244f - in[14] * 1.2855752193731f;

		// 9 point IDCT on odd indices
		// 5 points on odd indices (not realy an IDCT)
		float i0 = in[0 + 1] + in[0 + 1];
		float i0p12 = i0 + in[12 + 1];

		tmp0o = i0p12 + in[4 + 1] * 1.8793852415718f + in[8 + 1] * 1.532088886238f + in[16 + 1] * 0.34729635533386f;
		tmp1o = i0 + in[4 + 1] - in[8 + 1] - in[12 + 1] - in[12 + 1] - in[16 + 1];
		tmp2o = i0p12 - in[4 + 1] * 0.34729635533386f - in[8 + 1] * 1.8793852415718f + in[16 + 1] * 1.532088886238f;
		tmp3o = i0p12 - in[4 + 1] * 1.532088886238f + in[8 + 1] * 0.34729635533386f - in[16 + 1] * 1.8793852415718f;
		tmp4o = (in[0 + 1] - in[4 + 1] + in[8 + 1] - in[12 + 1] + in[16 + 1]) * 0.707106781f; // Twiddled

		// 4 points on even indices
		float i6_ = in[6 + 1] * 1.732050808f; // Sqrt[3]

		tmp0_o = in[2 + 1] * 1.9696155060244f + i6_ + in[10 + 1] * 1.2855752193731f
				+ in[14 + 1] * 0.68404028665134f;
		tmp1_o = (in[2 + 1] - in[10 + 1] - in[14 + 1]) * 1.732050808f;
		tmp2_o = in[2 + 1] * 1.2855752193731f - i6_ - in[10 + 1] * 0.68404028665134f
				+ in[14 + 1] * 1.9696155060244f;
		tmp3_o = in[2 + 1] * 0.68404028665134f - i6_ + in[10 + 1] * 1.9696155060244f
				- in[14 + 1] * 1.2855752193731f;

		// Twiddle factors on odd indices
		// and
		// Butterflies on 9 point IDCT's
		// and
		// twiddle factors for 36 point IDCT

		float e, o;
		e = tmp0 + tmp0_;
		o = (tmp0o + tmp0_o) * 0.501909918f;
		tmpf_0 = e + o;
		tmpf_17 = e - o;
		e = tmp1 + tmp1_;
		o = (tmp1o + tmp1_o) * 0.517638090f;
		tmpf_1 = e + o;
		tmpf_16 = e - o;
		e = tmp2 + tmp2_;
		o = (tmp2o + tmp2_o) * 0.551688959f;
		tmpf_2 = e + o;
		tmpf_15 = e - o;
		e = tmp3 + tmp3_;
		o = (tmp3o + tmp3_o) * 0.610387294f;
		tmpf_3 = e + o;
		tmpf_14 = e - o;
		tmpf_4 = tmp4 + tmp4o;
		tmpf_13 = tmp4 - tmp4o;
		e = tmp3 - tmp3_;
		o = (tmp3o - tmp3_o) * 0.871723397f;
		tmpf_5 = e + o;
		tmpf_12 = e - o;
		e = tmp2 - tmp2_;
		o = (tmp2o - tmp2_o) * 1.183100792f;
		tmpf_6 = e + o;
		tmpf_11 = e - o;
		e = tmp1 - tmp1_;
		o = (tmp1o - tmp1_o) * 1.931851653f;
		tmpf_7 = e + o;
		tmpf_10 = e - o;
		e = tmp0 - tmp0_;
		o = (tmp0o - tmp0_o) * 5.736856623f;
		tmpf_8 = e + o;
		tmpf_9 = e - o;

		// end 36 point IDCT */
		// shift to modified IDCT
		const float* win_bt = &(win[block_type][0]);

		out[0] = -tmpf_9 * win_bt[0];
		out[1] = -tmpf_10 * win_bt[1];
		out[2] = -tmpf_11 * win_bt[2];
		out[3] = -tmpf_12 * win_bt[3];
		out[4] = -tmpf_13 * win_bt[4];
		out[5] = -tmpf_14 * win_bt[5];
		out[6] = -tmpf_15 * win_bt[6];
		out[7] = -tmpf_16 * win_bt[7];
		out[8] = -tmpf_17 * win_bt[8];
		out[9] = tmpf_17 * win_bt[9];
		out[10] = tmpf_16 * win_bt[10];
		out[11] = tmpf_15 * win_bt[11];
		out[12] = tmpf_14 * win_bt[12];
		out[13] = tmpf_13 * win_bt[13];
		out[14] = tmpf_12 * win_bt[14];
		out[15] = tmpf_11 * win_bt[15];
		out[16] = tmpf_10 * win_bt[16];
		out[17] = tmpf_9 * win_bt[17];
		out[18] = tmpf_8 * win_bt[18];
		out[19] = tmpf_7 * win_bt[19];
		out[20] = tmpf_6 * win_bt[20];
		out[21] = tmpf_5 * win_bt[21];
		out[22] = tmpf_4 * win_bt[22];
		out[23] = tmpf_3 * win_bt[23];
		out[24] = tmpf_2 * win_bt[24];
		out[25] = tmpf_1 * win_bt[25];
		out[26] = tmpf_0 * win_bt[26];
		out[27] = tmpf_0 * win_bt[27];
		out[28] = tmpf_1 * win_bt[28];
		out[29] = tmpf_2 * win_bt[29];
		out[30] = tmpf_3 * win_bt[30];
		out[31] = tmpf_4 * win_bt[31];
		out[32] = tmpf_5 * win_bt[32];
		out[33] = tmpf_6 * win_bt[33];
		out[34] = tmpf_7 * win_bt[34];
		out[35] = tmpf_8 * win_bt[35];
	}
}

void hybrid(int ch, int gr, gr_info_s* gr_info, float(*prvblk)[SB_LIMIT*SS_LIMIT], float* tsOut) {
	float tsOutCopy[18];
	float rawout[36];
	int bt;
	int sb18;

	bool boolExpr = (gr_info->window_switching_flag != 0) & (gr_info->mixed_block_flag != 0);
	for (sb18 = 0; sb18 < 576; sb18 += 18) {
		bt = (boolExpr && (sb18 < 36)) ? 0 : gr_info->block_type;

		// Modif E.B 02/22/99
		for (int cc = 0; cc < 18; cc++){
			tsOutCopy[cc] = tsOut[cc + sb18];
		}

		inv_mdct(tsOutCopy, rawout, bt);

		for (int cc = 0; cc < 18; cc++){
			tsOut[cc + sb18] = tsOutCopy[cc];
		}
		// Fin Modif

		// overlap addition
		tsOut[0 + sb18] = rawout[0] + prvblk[ch][sb18 + 0];
		prvblk[ch][sb18 + 0] = rawout[18];
		tsOut[1 + sb18] = rawout[1] + prvblk[ch][sb18 + 1];
		prvblk[ch][sb18 + 1] = rawout[19];
		tsOut[2 + sb18] = rawout[2] + prvblk[ch][sb18 + 2];
		prvblk[ch][sb18 + 2] = rawout[20];
		tsOut[3 + sb18] = rawout[3] + prvblk[ch][sb18 + 3];
		prvblk[ch][sb18 + 3] = rawout[21];
		tsOut[4 + sb18] = rawout[4] + prvblk[ch][sb18 + 4];
		prvblk[ch][sb18 + 4] = rawout[22];
		tsOut[5 + sb18] = rawout[5] + prvblk[ch][sb18 + 5];
		prvblk[ch][sb18 + 5] = rawout[23];
		tsOut[6 + sb18] = rawout[6] + prvblk[ch][sb18 + 6];
		prvblk[ch][sb18 + 6] = rawout[24];
		tsOut[7 + sb18] = rawout[7] + prvblk[ch][sb18 + 7];
		prvblk[ch][sb18 + 7] = rawout[25];
		tsOut[8 + sb18] = rawout[8] + prvblk[ch][sb18 + 8];
		prvblk[ch][sb18 + 8] = rawout[26];
		tsOut[9 + sb18] = rawout[9] + prvblk[ch][sb18 + 9];
		prvblk[ch][sb18 + 9] = rawout[27];
		tsOut[10 + sb18] = rawout[10] + prvblk[ch][sb18 + 10];
		prvblk[ch][sb18 + 10] = rawout[28];
		tsOut[11 + sb18] = rawout[11] + prvblk[ch][sb18 + 11];
		prvblk[ch][sb18 + 11] = rawout[29];
		tsOut[12 + sb18] = rawout[12] + prvblk[ch][sb18 + 12];
		prvblk[ch][sb18 + 12] = rawout[30];
		tsOut[13 + sb18] = rawout[13] + prvblk[ch][sb18 + 13];
		prvblk[ch][sb18 + 13] = rawout[31];
		tsOut[14 + sb18] = rawout[14] + prvblk[ch][sb18 + 14];
		prvblk[ch][sb18 + 14] = rawout[32];
		tsOut[15 + sb18] = rawout[15] + prvblk[ch][sb18 + 15];
		prvblk[ch][sb18 + 15] = rawout[33];
		tsOut[16 + sb18] = rawout[16] + prvblk[ch][sb18 + 16];
		prvblk[ch][sb18 + 16] = rawout[34];
		tsOut[17 + sb18] = rawout[17] + prvblk[ch][sb18 + 17];
		prvblk[ch][sb18 + 17] = rawout[35];
	}
}









void SynthesisFilter_compute_pcm_samples4_universal(float* v, int offset, int actual_write_pos, float* tmpOut) {
	//offset = 0 or 512 only
	//actual_write_pos = 0..15
	
	// Compute PCM Samples.
	for (int i = 0; i < 32; i++) {
		//float* dp = &(SynthesisFilter_d16[i]);
		float term = 0;
		for (int ii = 0; ii < 16; ii++) {
			term += ( v[ ((actual_write_pos & 0x0F) | (i << 4)) + offset ] * /*dp*/SynthesisFilter_d16[i][ii] );
			actual_write_pos--;
		}
		tmpOut[i] = term;
	}
}

void SynthesisFilter_compute_subroutine1(float* src, int iteratorLevel, float* output) {
	float* samples_buffer1 = src;
	float* samples_buffer2 = output;

	int i;
	bool needsFlip = (iteratorLevel & 1) == 1;
	while (iteratorLevel >= 0) {
		const float* cosTable = &(SynthesisFilter_cosCacheTable[iteratorLevel][0]);

		int fullCycleSize = ((1 << iteratorLevel) * 2);
		int filterMask = ~(fullCycleSize - 1);
		int filterMask2 = ((1 << iteratorLevel) - 1);
		int intervalBit;
		int ii;
		const int src_length = 32; //src.length = 32
		for (i = 0; i < src_length; i++) { // sorry, but i love bit-hex-hex
			ii = i & filterMask2;
			intervalBit = ((i >> iteratorLevel) & 1) ^ 1;
			samples_buffer2[i] = (samples_buffer1[(i & filterMask) + ii]
					+ (samples_buffer1[(i & filterMask) + fullCycleSize - 1 - ii] * ((intervalBit << 1) - 1)))
					* ((cosTable[ii] * ((i >> iteratorLevel) & 1)) + intervalBit);
		}
		iteratorLevel--;
		// swap arrays because they share the same size ; to produce less
		// heap garbage
		float* tmp_out = samples_buffer2;
		samples_buffer2 = samples_buffer1;
		samples_buffer1 = tmp_out;
	}
	if (needsFlip) {
		for (int ii = 0; ii < 32; ii++) {
			output[ii] = samples_buffer1[ii];
		}
	}
}

void SynthesisFilter_compute(float* s, int pos, float* v) {
	// Compute new values via a fast cosine transform.
	float p1[32];
	float p[32];
	float new_v[32];

	for (int i = 0; i < 32; i++)
		p[i] = s[i];

	SynthesisFilter_compute_subroutine1(p, 4, p1);

	float tmp1;
	new_v[19] = -(new_v[4] = (new_v[12] = p1[7]) + p1[5]) - p1[6];
	new_v[27] = -p1[6] - p1[7] - p1[4];
	new_v[6] = (new_v[10] = (new_v[14] = p1[15]) + p1[11]) + p1[13];
	new_v[17] = -(new_v[2] = p1[15] + p1[13] + p1[9]) - p1[14];
	new_v[21] = (tmp1 = -p1[14] - p1[15] - p1[10] - p1[11]) - p1[13];
	new_v[29] = -p1[14] - p1[15] - p1[12] - p1[8];
	new_v[25] = tmp1 - p1[12];
	new_v[31] = -p1[0];
	new_v[0] = p1[1];
	new_v[23] = -(new_v[8] = p1[3]) - p1[2];
	{
		const float* cosTable = &(SynthesisFilter_cosCacheTable[4][0]);
		for (int i = 0; i < 16; i++) {
			p1[i] = (s[i] - s[31 - i]) * cosTable[i];
		}
	}
	SynthesisFilter_compute_subroutine1(p1, 3, p);

	// manually doing something that a compiler should handle sucks
	// coding like this is hard to read
	float tmp2;
	new_v[5] = (new_v[11] = (new_v[13] = (new_v[15] = p[15]) + p[7]) + p[11]) + p[5] + p[13];
	new_v[7] = (new_v[9] = p[15] + p[11] + p[3]) + p[13];
	new_v[16] = -(new_v[1] = (tmp1 = p[13] + p[15] + p[9]) + p[1]) - p[14];
	new_v[18] = -(new_v[3] = tmp1 + p[5] + p[7]) - p[6] - p[14];

	new_v[22] = (tmp1 = -p[10] - p[11] - p[14] - p[15]) - p[13] - p[2] - p[3];
	new_v[20] = tmp1 - p[13] - p[5] - p[6] - p[7];
	new_v[24] = tmp1 - p[12] - p[2] - p[3];
	new_v[26] = tmp1 - p[12] - (tmp2 = p[4] + p[6] + p[7]);
	new_v[30] = (tmp1 = -p[8] - p[12] - p[14] - p[15]) - p[0];
	new_v[28] = tmp1 - tmp2;

	// insert V[0-15] (== new_v[0-15]) into actual v:
	// float[] x2 = actual_v + actual_write_pos;

	//pos = 0..15
	
	int off1 = (pos & 1) * 512;
	int off2 = 512 - off1;

	for (int i = 0; i < 16; i++) v[(i * 16) + pos + off1] = new_v[i];
	v[256 + pos + off1] = 0.0f;
	for (int i = 17; i < 32; i++) v[(i * 16) + pos + off1] = -new_v[32 - i];

	// dest = ((pos & 1) == 1) ? v1 : v2;

	v[0 + pos + off2] = -new_v[0];
	for (int i = 1; i < 17; i++) v[(i * 16) + pos + off2] = new_v[i + 15];
	for (int i = 17; i < 32; i++) v[(i * 16) + pos + off2] = new_v[47 - i];
}

void SynthesisFilter_input_and_compute(float* s, int pos, float* output, float* v) {
	pos &= 0x0F;
	SynthesisFilter_compute(s, pos, v);
	SynthesisFilter_compute_pcm_samples4_universal(v, (pos & 1) * 512, pos, output);
}	



int decodeFrame(int header_sample_frequency, Mp3Instance* mp3Instance, char* frameBuffer, 
		int frameSize, int header_mode, int header_version, int header_mode_extension, short* samplesBuffer,
		int header_bitrate_index, int header_padding_bit, int header_protection_bit){
	int sfreq = header_sample_frequency + ((header_version == MPEG1) ? 3 : (header_version == MPEG25_LSF) ? 6 : 0);
	int channels = (header_mode == SINGLE_CHANNEL) ? 1 : 2;
	
	{
		int frameBufferReadPos[1];
		frameBufferReadPos[0] = 0;
		int mainDataBegin = readBitsFromBuffer(frameBuffer, frameSize, &(frameBufferReadPos[0]), header_version == MPEG1 ? 9 : 8);
		
		if(!get_side_info(header_version, mp3Instance, channels, frameBuffer, frameSize, &(frameBufferReadPos[0]))){
			return MP3_ERRORCODE_MISSING_HEADER;
		}

		int framesize;
		framesize = (144 * MP3_bitrates[header_version & 1][header_bitrate_index]) / MP3_frequencies[header_version][header_sample_frequency];
		if (header_version == MPEG2_LSF || header_version == MPEG25_LSF) {
			framesize >>= 1;
		}
		if (header_padding_bit != 0) {
			framesize++;
		}
		
		int crcSize = ((header_protection_bit != 0) ? 0 : 2);
		int nSlots;
		if (header_version == MPEG1) {
			nSlots = framesize - ((header_mode == SINGLE_CHANNEL) ? 17 : 32) - crcSize - MP3_HEADERFRAME_SIZE;
		} else { // MPEG-2 LSF, SZD: MPEG-2.5 LSF
			nSlots = framesize - ((header_mode == SINGLE_CHANNEL) ? 9 : 17) - crcSize - MP3_HEADERFRAME_SIZE;
		}
		
		if(mainDataBegin == 0){
			mp3Instance->frame_start = 0;
			//mp3Instance->bufReadPointer = 0;
			mp3Instance->bufWritePointer = 0;
			//mp3Instance->totalBitsRead = 0;
		} else {
			{
				int num = mp3Instance->bufReadPointer & 7;
				if(num != 0){
					num = 8 - num;
					//mp3Instance->totalBitsRead += num;
					mp3Instance->bufReadPointer += num;
				}
			}
			int main_data_end = mp3Instance->frame_start - (mp3Instance->bufReadPointer >> 3); // of previous frame
			int bytes_to_discard = main_data_end - mainDataBegin;

			if (bytes_to_discard < 0) {//if data buffers arent filled correctly
				mp3Instance->bufReadPointer = 0;
				mp3Instance->frame_start += nSlots;
				hputbuf_array(mp3Instance, frameBuffer, mp3Instance->isStereo ? 32 : 17, nSlots);//put new bytes into buffer
				for(int i=0; i<2 * 1152; i++) samplesBuffer[i] = 0;//bring output from random-state to 0-state.
				return 0;//return as like decoding was not a probem.
				//return MP3_ERRORCODE_NEGATIVE_DISCARD;
			}
			int alreadyReadBytes = mp3Instance->bufReadPointer / 8;
			int numBytesToDelete = alreadyReadBytes + bytes_to_discard;
			for(int i=numBytesToDelete; i<MAX_DATAPOOL_SIZE; i++){
				mp3Instance->buf[i-numBytesToDelete] = mp3Instance->buf[i];
			}
			mp3Instance->bufWritePointer -= numBytesToDelete;
			mp3Instance->frame_start -= bytes_to_discard;
			//mp3Instance->bufReadPointer -= alreadyReadBytes * 8;
			mp3Instance->frame_start -= alreadyReadBytes;
		}
		mp3Instance->bufReadPointer = 0;
		mp3Instance->frame_start += nSlots;
		hputbuf_array(mp3Instance, frameBuffer, mp3Instance->isStereo ? 32 : 17, nSlots);
// https://datatracker.ietf.org/doc/html/rfc3119
// https://github.com/FlorisCreyf/mp3-decoder/blob/master/mp3.cpp#L422
		
	}
	
	float samples[32];
	float outputBuffer[32];
	int max_gr = (header_version == MPEG1) ? 2 : 1;
	
	for (int gr = 0; gr < max_gr; gr++) {
		
		for (int ch = 0; ch < channels; ch++) {
			//int[][] scalefac_s = ch == 0 ? mp3Instance->scalefac0_s : mp3Instance->scalefac1_s;
			//int[] scalefac_l = ch == 0 ? mp3Instance->scalefac0_l : mp3Instance->scalefac1_l;
			int(*scalefac_s)[13] = ch == 0 ? &(mp3Instance->scalefac0_s[0]) : &(mp3Instance->scalefac1_s[0]);
			int* scalefac_l = ch == 0 ? &(mp3Instance->scalefac0_l[0]) : &(mp3Instance->scalefac1_l[0]);
			gr_info_s* info = &(mp3Instance->si_ch_gr[ch*2+gr]);
			
			//int part2_start = frameBufferReadPos[0];//br.hsstell();
			//mp3Instance->totalbits = 0;
			int localLen;
			if (header_version == MPEG1) {
				localLen = get_scale_factors(ch, gr, scalefac_s, scalefac_l, mp3Instance);
			} else { // MPEG-2 LSF, SZD: MPEG-2.5 LSF
				localLen = get_LSF_scale_factors(ch, gr, header_mode_extension, info, scalefac_s, scalefac_l, mp3Instance);
			}
			
			int is_1d[SB_LIMIT*SS_LIMIT+4];
			int iterationCount = huffman_decode(ch, gr, info, sfreq, localLen, is_1d, mp3Instance);
			
			// System.out.println("CheckSum HuffMan = " + CheckSumHuff);
			dequantize_sample(&(mp3Instance->ro[ch][0]), ch, gr, scalefac_s, scalefac_l, info, sfreq, is_1d, iterationCount);
			
		}

		//float[][][] lr = new float[2][SB_LIMIT][SS_LIMIT];
		
		
		stereo(gr, header_version, header_mode_extension, &(mp3Instance->si_ch_gr[gr]), mp3Instance, sfreq, channels, header_mode, &(mp3Instance->ro[0]), &(mp3Instance->lr[0]));
		
		
		
		//int last_channel = channels == 2 ? 1  : 0;
		bool isMono = channels == 1;
		//float[][] prevblck = mp3Instance->prevblck;
		float(*prevblck)[SB_LIMIT*SS_LIMIT] = &(mp3Instance->prevblck[0]);
		//float[]	out_1d = new float[SB_LIMIT*SS_LIMIT];
		float out_1d[SB_LIMIT*SS_LIMIT];
		
		if(isMono){
			int ch = 0;
			gr_info_s* info = &(mp3Instance->si_ch_gr[ch*2+gr]);
			reorder(&(mp3Instance->lr[ch][0]), ch, gr, info, sfreq, &(out_1d[0]));
			antialias(ch, gr, info, &(out_1d[0]));
			hybrid(ch, gr, info, prevblck, &(out_1d[0]));
			/*for (int sb18 = 18; sb18 < 576; sb18 += 36){ // Frequency inversion
				for (int ss = 1; ss < SS_LIMIT; ss += 2){
					out_1d[sb18 + ss] *= -1;
				}
			}*/
			for (int ss = 0; ss < SS_LIMIT; ss++) {
				if(ss & 1){
					for (int sb18 = 18; sb18 < 576; sb18 += 36){ // Frequency inversion
						out_1d[sb18 + ss] *= -1;
					}
				}
				int sb = 0;
				for (int sb18 = 0; sb18 < 576; sb18 += 18) {
					samples[sb] = out_1d[sb18 + ss];
					sb++;
				}
				SynthesisFilter_input_and_compute(samples, mp3Instance->sample_wp1, &(outputBuffer[0]), &(mp3Instance->v1[0]));
				mp3Instance->sample_wp1++;
				
				int writePos = gr * 64 * 18 + ss * 64;
				for (int i = 0; i < 32; i++) {
					int fs = (int) outputBuffer[i];
					samplesBuffer[writePos + i * 2] = (short) (fs > 32767 ? 32767 : (fs < -32767 ? -32767 : fs));
					samplesBuffer[writePos + i * 2 + 1] = samplesBuffer[writePos + i * 2];
				}
			}
		} else {
			for(int ch=0; ch<2; ch++){
				gr_info_s* info = &(mp3Instance->si_ch_gr[ch*2+gr]);
				reorder(&(mp3Instance->lr[ch][0]), ch, gr, info, sfreq, &(out_1d[0]));
				antialias(ch, gr, info, &(out_1d[0]));
				hybrid(ch, gr, info, prevblck, &(out_1d[0]));
				/*
				for (int ss = 1; ss < SS_LIMIT; ss += 2){
					for (int sb18 = 18; sb18 < 576; sb18 += 36){ // Frequency inversion
						out_1d[sb18 + ss] *= -1;
					}
				}
				*/
				for (int ss = 0; ss < SS_LIMIT; ss++) {
					if(ss & 1){
						for (int sb18 = 18; sb18 < 576; sb18 += 36){ // Frequency inversion
							out_1d[sb18 + ss] *= -1;
						}
					}
					int sb = 0;
					for (int sb18 = 0; sb18 < 576; sb18 += 18) {
						samples[sb] = out_1d[sb18 + ss];
						sb++;
					}
					int sample_wp;
					if(ch == 0){
						sample_wp = mp3Instance->sample_wp1;
						mp3Instance->sample_wp1++;
					} else {
						sample_wp = mp3Instance->sample_wp2;
						mp3Instance->sample_wp2++;
					}
					SynthesisFilter_input_and_compute(samples, sample_wp, &(outputBuffer[0]), ch == 0 ? &(mp3Instance->v1[0]) : &(mp3Instance->v2[0]));
					
					int writePos = gr * 64 * 18 + ss * 64 + ch;
					for (int i = 0; i < 32; i++) {
						int fs = (int) outputBuffer[i];
						samplesBuffer[writePos + i * 2] = (short) (fs > 32767 ? 32767 : (fs < -32767 ? -32767 : fs));
					}
				}
			}
		}
	} // granule
	return 0;//success
}

int DataReader_read(LinkedList* list){
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

bool DataReader_readArr(LinkedList* list, char* array, int len){
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

char* MP3Parser::runDecode(LinkedList* dataBlocks, int* errorIdOutput){
	errorIdOutput[0] = MP3_ERRORCODE_UNKNOWN;
	if(!instance){
		return 0;
	}
	if(dataBlocks->size == 0){
		errorIdOutput[0] = MP3_ERRORCODE_END_OF_INPUT_BUFFER_REACHED;
		return 0;
	}
	
	Mp3Instance* mp3Instance = instance;
	//step 1: find offset of next frame
	int read;
	while(1){
		read = DataReader_read(dataBlocks);
		if(read == -1) {
			errorIdOutput[0] = MP3_ERRORCODE_BUFFER_HAS_NO_STARTSEQUENCE;
			return 0;
		}
		if(read != 255) continue;
		read = DataReader_read(dataBlocks);
		if(read == -1) {
			errorIdOutput[0] = MP3_ERRORCODE_END_OF_INPUT_BUFFER_REACHED;
			return 0;
		}
		if((read & 0xF0) == 0xF0) break;
	}
	
	int read2 = DataReader_read(dataBlocks);
	int read3 = DataReader_read(dataBlocks);
	if((read2 | read3) == -1) {
		errorIdOutput[0] = MP3_ERRORCODE_END_OF_INPUT_BUFFER_REACHED;
		return 0;
	}
	
	if(mp3Instance->cachedHeader == 0){
		mp3Instance->cachedHeader = 0xFF000000 | (read << 16) | (read2 << 8) | read3;
	} else {
		if( ((read2 >> 2) & 3) == 3 ){
			read = (mp3Instance->cachedHeader >> 16) & 255;
			read2 = (mp3Instance->cachedHeader >> 8) & 255;
			read3 = mp3Instance->cachedHeader & 255;
		}
	}
	
	//step 2: extract metadata
	int version = ((read >> 3) & 1);
	if (((read >> 4) & 1) == 0){
		if (version == MPEG2_LSF) {
			version = MPEG25_LSF;
		} else {
			//print("expected MPEG2_LSF but got ");
			//nprintln(version);
			errorIdOutput[0] = MP3_ERRORCODE_INVALID_MPEG_VERSION_CONFIG;
			return 0;
		}
	}
	int layer = ((read >> 1) & 3);
	int protectionBit = ((read >> 0) & 1);
	
	
	
	int bitrateIndex = ((read2 >> 4) & 15);
	int samplingFrequency = ((read2 >> 2) & 3);
	if(samplingFrequency == 3){
		//println("invalid samplingFrequency!");
		errorIdOutput[0] = MP3_ERRORCODE_INVALID_SAMPLEINGFREQUENCY_CONFIG;
		return 0;
	}
	int paddingBit = ((read2 >> 1) & 1);

	
	int mode = ((read3 >> 6) & 3);
	int modeExtension = ((read3 >> 4) & 3);
	
	instance->isStereo = isStereo = mode != 0b11;
	if (protectionBit == 0) {
		int read_3 = DataReader_read(dataBlocks);
		int read_2 = DataReader_read(dataBlocks);
		if((read_3 | read_2) == -1) {
			errorIdOutput[0] = MP3_ERRORCODE_END_OF_INPUT_BUFFER_REACHED;
			return 0;
		}
	}
	
	
	if(layer != 0b01){
		println("unsupported codec! must be layer III mp3!");
		
		short* samplesBuffer = (short*)malloc(2 * 1152 * 2);
		if(!samplesBuffer) {
			errorIdOutput[0] = MP3_ERRORCODE_INSUFFICENT_MEMORY;
			return 0;
		}
		for(int i=0; i<2 * 1152; i++) samplesBuffer[i] = 0;//bring output from random-state to 0-state.
		
		MP3Parser::samplingFrequency = SAMPLING_FREQUENCY[samplingFrequency];

		errorIdOutput[0] = MP3_ERRORCODE_NONE;
		lastFrameSampleCount = 2 * 1152 *2;
		return (char*)samplesBuffer;
	
	} //else: do layer III decoding...
	
	
	
	
	int frameSize = (144 * BITRATE_LAYER_III[bitrateIndex]) / SAMPLING_FREQUENCY[samplingFrequency] + paddingBit - 4;
	
	char* frameBuffer = (char*)malloc(frameSize);
	if(!frameBuffer) {
		errorIdOutput[0] = MP3_ERRORCODE_INSUFFICENT_MEMORY;
		return 0;
	}
	if(!DataReader_readArr(dataBlocks, frameBuffer, frameSize)){
		errorIdOutput[0] = MP3_ERRORCODE_END_OF_INPUT_BUFFER_REACHED;
		//println("DataReader_readArr() -> too less data in buffer!");
		free(frameBuffer);
		return 0;
	}
	//byte[] samplesBuffer = new byte[18 * 32 * 2 * ((state.stereo ? 2 : 1) * 2)];
	short* samplesBuffer = (short*)malloc(2 * 1152 * 2);
	if(!samplesBuffer) {
		errorIdOutput[0] = MP3_ERRORCODE_INSUFFICENT_MEMORY;
		free(frameBuffer);
		return 0;
	}
	
	int errorcode = decodeFrame(samplingFrequency, mp3Instance, frameBuffer, frameSize, mode, version, modeExtension, samplesBuffer, bitrateIndex, paddingBit, protectionBit);
	
	free(frameBuffer);
	
	if(errorcode == 0){
		
		MP3Parser::samplingFrequency = SAMPLING_FREQUENCY[samplingFrequency];

		errorIdOutput[0] = MP3_ERRORCODE_NONE;
		lastFrameSampleCount = 2 * 1152 *2;
		return (char*)samplesBuffer;
	} else {
		free(samplesBuffer);
		errorIdOutput[0] = errorcode;
	}
	return 0;
}
