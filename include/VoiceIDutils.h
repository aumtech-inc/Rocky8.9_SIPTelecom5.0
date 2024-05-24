#ifndef VOICEIDUTILS_H_INCLUDED
#define VOICEIDUTILS_H_INCLUDED

/* VoiceIDutils.h */
/*
 * $URL: https://subversion.assembla.com/svn/voiceid/trunk/AVB14/VoiceIDutils.h $
 * $Rev: 145 $
 * $Date: 2014-06-08 16:50:57 -0700 (Sun, 08 Jun 2014) $
 * $Author: voiceid $
 */

/*****************************************************************/
/*
	These utility routines are provided for convenience only,
	to assist with file I/O.
	They are not part of the VoiceID library or product, nor do
	they have any depencencies on the VoiceID library.  
	The example programs are file-based, and use these utilities.
	Production applications that use database storage would provide
	their own suitable replacements.
*/
/*****************************************************************/

/* 8000 Samples = 1 Sec = 16000 bytes  (2 bytes/sample PCM)
   Work in units of 1024 bytes (=1K)
   2 seconds = 2 x 8000 = 16000 Samples = 32000 bytes
   Get close to this in even units:
   ~2 sec = 2 x (8 x 1024 samples) = 16384 samples = 32786 bytes
   Frame shift is 128 samples.  The frame length is 256 samples.
   The number of frames in 2 seconds is
   (16384 / 128) = 128-1 = 127 because the first half-frame cannot be used.
   
   For development purposes, we will use ~0.5 second, which is 1/4 these
   numbers: 4096 Samples.  32-1=31 frames.

   It is also necessary for buffer size to be a multiple of 1K=1024,
   which this is:  4096 = 1024 * 4
*/
//#define VoiceID_AUDIO_BUFFER_N_SAMPLES 4098
//#define VoiceID_AUDIO_BUFFER_N_SAMPLES 200000
#define VoiceID_AUDIO_BUFFER_N_SAMPLES 500000


#define VoiceID_SUCCESS 0

#define VoiceID_FILE_ERROR_GENERIC 1
#define VoiceID_FILE_OPEN_R_ERROR 2
#define VoiceID_FILE_READ_ERROR 3
#define VoiceID_FILE_WRITE_ERROR 4
#define VoiceID_MALLOC_ERROR 5

#define VoiceID_VOICEPRINT_NOT_FOUND 1001
#define VoiceID_FILE_ACCESS_ERROR 1002

#define VoiceID_ERROR_GENERIC 999

// WAVE PCM soundfile format (you can find more in 
// https://ccrma.stanford.edu/courses/422/projects/WaveFormat/ )
typedef struct _header_wav
{
  char chunk_id[4];
  int chunk_size;
  char format[4];
  char subchunk1_id[4];
  int subchunk1_size;
  short int audio_format;
  short int num_channels;
  int sample_rate;			// sample_rate denotes the sampling rate.
  int byte_rate;
  short int block_align;
  short int bits_per_sample;
  char subchunk2_id[4];
  int subchunk2_size;			// subchunk2_size denotes the number of samples.
} Header_wav_t;


/******** START Implementations in VoiceIDutilities.c: *****************/
int VIDPub_FileSize2End(FILE *pF_audio, unsigned long* pul_FileSize, 
												char* pc_ErrMsg);

short* VIDPub_alloc_audio(char* pc_ErrMsg);
int VIDPub_ReadSpeechFile_withHeader(char cp_FileName[], short *s_Samples,
																		 unsigned long* pul_NumSamplesRead, 
																		 char* pc_ErrMsg);
void VIDPub_release_audio(short* ps_Samples);

FILE* VIDPub_openWavFile_withHeader(char pc_FileName[],	char *ErrMsg);

unsigned long read_openWavFile(FILE* pF_audio, short* ps_Samples, 
															 unsigned long ul_nSamplesToRead);

/*^^^^^^^^^ END Implementations in VoiceIDutilities.c  ^^^^^^^^^^^^^^*/

#endif // VOICEIDUTILS_H_INCLUDED

