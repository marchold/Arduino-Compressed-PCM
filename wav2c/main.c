/***************************************************************
 * Name:      wave2c, a WAV file to GBA C source converter.
 * Purpose:   translate audio binaries into AVR memory data
 * Author:    Ino Schlaucher (ino@blushingboy.org)
 * Created:   2008-07-28
 * Copyright: Ino Schlaucher (http://blushingboy.org)
 * License:   GPLv3 (upgrading previous version)
 **************************************************************
 *
 * Based on an original piece of code by Mathieu Brethes.
 *
 * Copyright (c) 2003 by Mathieu Brethes.
 *
 * Contact : thieumsweb@free.fr
 * Website : http://thieumsweb.free.fr/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "wavdata.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


static int bitcount;
static int *bufPtr;


static int getBits(int numberOfBits)
{
    int shift = bitcount&0xF;
    int word  = bitcount>>4;
    
    bitcount += numberOfBits;
    int result = bufPtr[word];
    result >>= shift;
    int bitsUsed = (16-shift);
    if (numberOfBits-bitsUsed > 0)
    {
        unsigned int mask = (unsigned int)-1;
        mask >>= (16-(bitsUsed));
        result &= mask;
        mask = (unsigned int)-1;
        mask >>= (16-(numberOfBits-bitsUsed));
        int rr = (((int*)bufPtr)[word+1]& mask);
        int rrr = rr  << bitsUsed;
        result = result | rrr;
    }
    result <<= (16-numberOfBits);
    result >>= (16-numberOfBits);
    return result;
}

typedef struct  { 
    int8_t number;
    int difference;
    int minBits;
    int bits;
} Sample;

#define MAX_COMPRESSED_WORDS 10000



uint16_t compressed[MAX_COMPRESSED_WORDS] = {0};
Sample numbers[MAX_COMPRESSED_WORDS] = {0};

//Uncompressed size
int samples = 0;

#define MAX_SAMPLES 50000
int8_t uncompressed[MAX_SAMPLES] = {0};

int bitCount;

//Function to calculate the minimum bits we need to store a value in our scheme
int minBitsForValue(int value){
    
    if (value == 0) return 2;
    int mask = 0x4000;
    int minBits = 16;
    bool isSigned = false;
    if (value<0){
        value = -value;
        isSigned = true;
    }
    while (((value & mask)!=mask) && (minBits>0))
    {
        minBits--;
        mask>>=1;
    }
    return minBits;
}

void putBits(int _value, int numberOfBits)
{
    //Mask out the bits we are not using
    uint16_t mask = ((uint16_t)0xFFFF) >> (16-numberOfBits);
    int value = _value;
    value &= mask;
    
    //Calculate the byte and bit that we will be writing to
    int shift = bitCount&0xF;
    int word  = bitCount>>4;
    
    //Bump the bitcount for next time
    bitCount += numberOfBits;
    
    //Write the bits to the compressed buffer at the current 32 bit word
    uint16_t curWord = compressed[word];
    compressed[word] = curWord | (value << shift);
    
    //Decrement the number of bits by the number we already added to the compressed[] buffer
    //And if we have bits that overflow the current word write then to the next word
    numberOfBits -= (16-shift);
    if (numberOfBits > 0)
    {
        value = (value >> (16-shift));
        word++;
        compressed[word] = value;
    }
}


void encode(int8_t *array, int numbersCount)
{
    
    
#define WINDOW_SIZE 8
    
    
    //Build the Sample objects for each number
    for (int i = 0; i < numbersCount; i++)
    {
        numbers[i].number = array[i];
    }

    
    //1st, Set the whole sequence to difference value except for the first which retain the original value
    for (int i = numbersCount-1; i > 0 ;i--)
    {
        numbers[i].difference = numbers[i-1].number-numbers[i].number;
        numbers[i].minBits    = minBitsForValue(numbers[i].difference);
    }
        
    //2nd, Look at 8 value windows to and find the least number of bits for that window
    //     and set the number of bits for that window to that minimum
    for (int i = 1; i < numbersCount ;)
    {
        int minBitsForWindow=0;
        int ii = i;
        for (int j = 0; j < WINDOW_SIZE && i < numbersCount; j++,i++)
        {
            if (numbers[i].minBits > minBitsForWindow)
            {
                minBitsForWindow = numbers[i].minBits;
            }
        }
        for (int j = 0; j < WINDOW_SIZE && ii < numbersCount; j++,ii++)
        {
            numbers[ii].bits = minBitsForWindow;
        }
    }
    
    //In some cases we can move small numbers from the one window in to another, in an attempt to make the small bits windows bigger.
    int lastBits = 1000;
    int curBits = numbers[1].bits;
    for (int i = 1; i < numbersCount; i++)
    {
        if (numbers[i].minBits < curBits)
        {
            lastBits = curBits;
            curBits = numbers[i].bits;
        }
        if (lastBits<curBits)
        {
            if (numbers[i].minBits < lastBits)
            {
                numbers[i].bits = lastBits;
            }
            else {
                lastBits = 1000;
            }
        }
    }
    
    putBits(numbers[0].number,8);
    
    //Put 3 bits defining how many bits per sample
    putBits(numbers[1].bits-2,3);
        
    //Set the bits per sample
    curBits = numbers[1].bits;
    for (int i = 1; i < numbersCount;i++)
    {
      //  NSLog(@"Adding %d",((Sample*)numbers[i]).number);
        //If the bitsPer sample we are using is not equal to the current bits per sample
        if (numbers[i].bits != curBits)
        {
            //output an escape code
            int escapeCode = 1 << (curBits-1);
            putBits(escapeCode,curBits);

            //And write the number of bits per sample minus 2 in the next 3 bits
            putBits(numbers[i].bits-2,3);

            //Set the bits per sample
            curBits = numbers[i].bits;
        }
        putBits(numbers[i].difference,curBits);
    }
    
}


int main(int argc, char **argv) {
	wavSound*s;
	FILE *fin;
	FILE *fout;

	if (argc < 4 || argc > 6) {
		printf("Usage 1: %s <file.wav> <output.c> <soundname>\n", argv[0]);
		printf("Usage 2: %s <file.wav> <output.c> <soundname> <amount of samples>\n", argv[0]);
		exit(0);
	}

    char *fn = argv[1];
	fin = fopen(fn, "r");

	s = loadWaveHeader(fin);

	if (s == NULL) {
		printf("Invalid wave !\n");
		exit(0);
	}

    char tempFile[255];
    
    tempFile[0] = 0;
    strcat(tempFile, argv[2]);
    strcat(tempFile, ".temp");
    
	fout = fopen(tempFile, "w");

    switch (argc) {
        case 4:
            saveWave_(fin, s, fout, argv[3], MAX_SAMPLES);
            break;
        case 5:
            saveWave_(fin, s, fout, argv[3], atoi(argv[4]));
            break;
        default:
            break;
    }
    fclose(fout);
    
    fout = fopen(argv[2], "w");
    //Read in the temp file and create a compressed version
    encode(uncompressed,samples);
    
  
    int size  = (bitCount>>4)+1;
    fprintf(fout, "const int %s_length = %d;\n\n", argv[3], size);
    fprintf(fout, "const int %s_data[] PROGMEM ={\n", argv[3]);

    for (int i = 0; i < size; i++){
        fprintf(fout, "0x%X,",compressed[i]);
        if (i%20 == 0) fprintf(fout , "\n" );
    }
    fprintf(fout, "};\n");
    
	return 0;
}
