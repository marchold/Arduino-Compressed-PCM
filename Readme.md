This code is adapted from https://github.com/damellis/PCM and https://github.com/olleolleolle/wav2c

Therefore its got a GPL license

The idea for this code base is to create a ultra low complexity compressed audio format that allows for more than a few seconds of recorded audio out of an Arduino's program memory. Modern codecs like mp3 are too complex for a simple microcontroller. This format is simple:
  1, Take the difference of each value
  2, Store each difference with the least amount of bits possible. 

