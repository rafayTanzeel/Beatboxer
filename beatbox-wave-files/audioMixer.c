// Incomplete implementation of an audio mixer. Search for "REVISIT" to find things
// which are left as incomplete.
// Note: Generates low latency audio on BeagleBone Black; higher latency found on host.
#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#include <alloca.h> // needed for mixer
#include <time.h>
#include "audioMixer.h"

static snd_pcm_t *handle;
static int seq=0;
static _Bool externSeq=true;
static int externIndex = 0;
static _Bool nextSeq=true;

#define SOURCE_FILE_BD "beatbox-wav-files/wave-file/100051__menegass__gui-drum-bd-hard.wav" //Default Base Drum
#define SOURCE_FILE_SN "beatbox-wav-files/wave-file/100059__menegass__gui-drum-snare-soft.wav" //Default Snare
#define SOURCE_FILE_HH "beatbox-wav-files/wave-file/100053__menegass__gui-drum-cc.wav" //Default HitHat

#define MAX_TEMPO 300
#define MIN_TEMPO 40

#define DEFAULT_VOLUME 80

#define SAMPLE_RATE 44100
#define NUM_CHANNELS 1
#define SAMPLE_SIZE (sizeof(short)) 			// bytes per sample
// Sample size note: This works for mono files because each sample ("frame') is 1 value.
// If using stereo files then a frame would be two samples.

static unsigned long playbackBufferSize = 0;
static short *playbackBuffer = NULL;

char* beatbox_fileName[]={"beatbox-wav-files/wave-file/100051__menegass__gui-drum-bd-hard.wav","beatbox-wav-files/wave-file/100052__menegass__gui-drum-bd-soft.wav","beatbox-wav-files/wave-file/100053__menegass__gui-drum-cc.wav","beatbox-wav-files/wave-file/100054__menegass__gui-drum-ch.wav","beatbox-wav-files/wave-file/100055__menegass__gui-drum-co.wav","beatbox-wav-files/wave-file/100056__menegass__gui-drum-cyn-hard.wav","beatbox-wav-files/wave-file/100057__menegass__gui-drum-cyn-soft.wav","beatbox-wav-files/wave-file/100058__menegass__gui-drum-snare-hard.wav","beatbox-wav-files/wave-file/100059__menegass__gui-drum-snare-soft.wav","beatbox-wav-files/wave-file/100060__menegass__gui-drum-splash-hard.wav","beatbox-wav-files/wave-file/100061__menegass__gui-drum-splash-soft.wav","beatbox-wav-files/wave-file/100062__menegass__gui-drum-tom-hi-hard.wav","beatbox-wav-files/wave-file/100063__menegass__gui-drum-tom-hi-soft.wav","beatbox-wav-files/wave-file/100064__menegass__gui-drum-tom-lo-hard.wav","beatbox-wav-files/wave-file/100065__menegass__gui-drum-tom-lo-soft.wav","beatbox-wav-files/wave-file/100066__menegass__gui-drum-tom-mid-hard.wav","beatbox-wav-files/wave-file/100067__menegass__gui-drum-tom-mid-soft.wav"};


// Currently active (waiting to be played) sound bites
#define MAX_SOUND_BITES 30
typedef struct {
	// A pointer to a previously allocated sound bite (wavedata_t struct).
	// Note that many different sound-bite slots could share the same pointer
	// (overlapping cymbal crashes, for example)
	wavedata_t *pSound;

	// The offset into the pData of pSound. Indicates how much of the
	// sound has already been played (and hence where to start playing next).
	int location;
} playbackSound_t;
static playbackSound_t soundBites[MAX_SOUND_BITES];


// Playback threading
void* playbackThread(void* arg);
void* playbackSeqThread(void* arg);

static _Bool stopping = false;
static pthread_t playbackThreadId;
static pthread_t playbackSeqThreadId;

static pthread_mutex_t audioMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t seqMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t externSeqMutex = PTHREAD_MUTEX_INITIALIZER;


static int volume = 80;
static int beat_count=0;
static int BPM = 120;
static int mode = 0;

wavedata_t hithatFile;
wavedata_t snareFile;
wavedata_t basedrumFile;

void sep_player(int index);

int AudioMixer_getMode(void){

	return mode;

}

void AudioMixer_setMode(int mod){
	mode=mod%3;
}

void AudioMixer_init(void)
{
	AudioMixer_setVolume(DEFAULT_VOLUME);

	// Initialize the currently active sound-bites being played
	// REVISIT:- Implement this. Hint: set the pSound pointer to NULL for each
	//     sound bite.
	for(int i=0; i<MAX_SOUND_BITES; i++){
		soundBites[i].pSound=NULL;
		soundBites[i].location=0;
	}


	// Open the PCM output
	int err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	// Configure parameters of PCM output
	err = snd_pcm_set_params(handle,
			SND_PCM_FORMAT_S16_LE,
			SND_PCM_ACCESS_RW_INTERLEAVED,
			NUM_CHANNELS,
			SAMPLE_RATE,
			1,			// Allow software resampling
			50000);		// 0.05 seconds per buffer
	if (err < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	// Allocate this software's playback buffer to be the same size as the
	// the hardware's playback buffers for efficient data transfers.
	// ..get info on the hardware buffers:
 	unsigned long unusedBufferSize = 0;
	snd_pcm_get_params(handle, &unusedBufferSize, &playbackBufferSize);
	// ..allocate playback buffer:
	playbackBuffer = malloc(playbackBufferSize * sizeof(*playbackBuffer));

	standard_beats();//Use default standard beats


	// Launch playback thread:
	pthread_create(&playbackThreadId, NULL, playbackThread, NULL);
	pthread_create(&playbackSeqThreadId, NULL, playbackSeqThread, NULL);

}


int AudioMixer_getHalfBeatDelay(){

	return ((60.0/BPM)/2.0)*1000;
}



void AudioMixer_setBPM(int val){
	if(val>MAX_TEMPO){
		BPM=MAX_TEMPO;
	}
	else if(val<MIN_TEMPO){
		BPM=MIN_TEMPO;
	}
	else{
		BPM=val;
	}
}


int AudioMixer_getBPM(){
	return BPM;
}



// Client code must call AudioMixer_freeWaveFileData to free dynamically allocated data.
void AudioMixer_readWaveFileIntoMemory(char *fileName, wavedata_t *pSound)
{
	assert(pSound);

	// The PCM data in a wave file starts after the header:
	const int PCM_DATA_OFFSET = 44;

	// Open the wave file
	FILE *file = fopen(fileName, "r");
	if (file == NULL) {
		fprintf(stderr, "ERROR: Unable to open file %s.\n", fileName);
		exit(EXIT_FAILURE);
	}

	// Get file size
	fseek(file, 0, SEEK_END);
	int sizeInBytes = ftell(file) - PCM_DATA_OFFSET;
	pSound->numSamples = sizeInBytes / SAMPLE_SIZE;

	// Search to the start of the data in the file
	fseek(file, PCM_DATA_OFFSET, SEEK_SET);

	// Allocate space to hold all PCM data
	pSound->pData = malloc(sizeInBytes);
	if (pSound->pData == 0) {
		fprintf(stderr, "ERROR: Unable to allocate %d bytes for file %s.\n",
				sizeInBytes, fileName);
		exit(EXIT_FAILURE);
	}

	// Read PCM data from wave file into memory
	int samplesRead = fread(pSound->pData, SAMPLE_SIZE, pSound->numSamples, file);
	if (samplesRead != pSound->numSamples) {
		fprintf(stderr, "ERROR: Unable to read %d samples from file %s (read %d).\n",
				pSound->numSamples, fileName, samplesRead);
		exit(EXIT_FAILURE);
	}
}

void AudioMixer_freeWaveFileData(wavedata_t *pSound)
{
	pSound->numSamples = 0;
	free(pSound->pData);
	pSound->pData = NULL;
}

void AudioMixer_queueSound(wavedata_t *pSound)
{
	if(mode!=2){
		// Ensure we are only being asked to play "good" sounds:
		assert(pSound->numSamples > 0);
		assert(pSound->pData);


		pthread_mutex_lock(&audioMutex);


		_Bool freeSlotExit = false;

		for(int i=0; i<MAX_SOUND_BITES && !freeSlotExit; i++){
			if(soundBites[i].pSound==NULL){
				soundBites[i].pSound=pSound;
				soundBites[i].location=0;
				freeSlotExit=true;
				seq++;
			}
		}
		pthread_mutex_unlock(&audioMutex);


		if(!freeSlotExit){
			printf("ERROR: No free slot found in the sound queue.\n");
			return;
		}
	}

}

void AudioMixer_cleanup(void)
{
	printf("Stopping audio...\n");

	// Stop the PCM generation thread
	stopping = true;

	// Shutdown the PCM output, allowing any pending sound to play out (drain)
	snd_pcm_drain(handle);
	snd_pcm_close(handle);

	// Free playback buffer
	// (note that any wave files read into wavedata_t records must be freed
	//  in addition to this by calling AudioMixer_freeWaveFileData() on that struct.)
	free(playbackBuffer);
	playbackBuffer = NULL;

	printf("Done stopping audio...\n");
	fflush(stdout);
}


int AudioMixer_getVolume()
{
	// Return the cached volume; good enough unless someone is changing
	// the volume through other means and the cached value is out of date.
	return volume;
}

// Function copied from:
// http://stackoverflow.com/questions/6787318/set-alsa-master-volume-from-c-code
// Written by user "trenki".
void AudioMixer_setVolume(int newVolume)
{
	// Ensure volume is reasonable; If so, cache it for later getVolume() calls.
	if (newVolume < 0 || newVolume > AUDIOMIXER_MAX_VOLUME) {
		printf("ERROR: Volume must be between 0 and 100.\n");
		return;
	}
	volume = newVolume;

    long min, max;
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    const char *card = "default";
    const char *selem_name = "PCM";

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, card);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);
    snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);

    snd_mixer_close(handle);
}


// Fill the playbackBuffer array with new PCM values to output.
//    playbackBuffer: buffer to fill with new PCM data from sound bites.
//    size: the number of values to store into playbackBuffer
static void fillPlaybackBuffer(short *playbackBuffer, int size)
{

	memset(playbackBuffer,0,size * sizeof(*playbackBuffer));

	pthread_mutex_lock(&audioMutex);

	for(int i=0; i<MAX_SOUND_BITES; i++){
		wavedata_t *pSoundB=soundBites[i].pSound;
		wavedata_t *pSoundBSec=soundBites[(i+1)%MAX_SOUND_BITES].pSound;

		int location=soundBites[i].location;
		int newLocation=location;

		int num_sample=(pSoundB!=NULL)?pSoundB->numSamples:0;

		if(beat_count%2==0 && pSoundB!=NULL && pSoundBSec!=NULL){
			num_sample=(pSoundB->numSamples > pSoundBSec->numSamples)?pSoundB->numSamples:pSoundBSec->numSamples;
		}

		if(pSoundB!=NULL){
			for(int j=0; j<size && j+location<num_sample; j++){
				int bufferSec=0;
				if(pSoundBSec!=NULL && beat_count%2==0 && j+location<pSoundBSec->numSamples){
					bufferSec=(pSoundBSec->pData)[j+location];
				}
				int buffer=((pSoundB->pData)[j+location])*((j+location<pSoundB->numSamples)?1:0)+bufferSec;
				if(buffer>SHRT_MAX){
					playbackBuffer[j]=SHRT_MAX;
				}
				else if(SHRT_MIN>buffer){
					playbackBuffer[j]=SHRT_MIN;
				}
				else{
					playbackBuffer[j]=buffer;
				}


//				playbackBuffer[j]=(buffer>SHRT_MAX)?SHRT_MAX:buffer;
				newLocation++;
			}
			soundBites[i].location=newLocation;

//			printf("Sample: %d Location: %d\n",pSoundB->numSamples, soundBites[i].location);

			if(newLocation>=num_sample){
//				AudioMixer_freeWaveFileData(soundBites[i].pSound);
				soundBites[i].pSound=NULL;

				for(int j=i; j+1<MAX_SOUND_BITES; j++){
					soundBites[j].pSound=soundBites[j+1].pSound;
					soundBites[j].location=soundBites[j+1].location;

				}
				soundBites[MAX_SOUND_BITES-1].pSound=pSoundB;
				soundBites[MAX_SOUND_BITES-1].location=0;

				soundBites[MAX_SOUND_BITES-1].pSound=NULL;

				if(i+1<MAX_SOUND_BITES && beat_count%2==0){
					soundBites[i].pSound=NULL;
					for(int j=i; j+1<MAX_SOUND_BITES; j++){
						soundBites[j].pSound=soundBites[j+1].pSound;
						soundBites[j].location=soundBites[j+1].location;
					}
					soundBites[MAX_SOUND_BITES-1].pSound=pSoundBSec;
					soundBites[MAX_SOUND_BITES-1].location=0;

					soundBites[MAX_SOUND_BITES-1].pSound=NULL;

				}
				beat_count++;
				pthread_mutex_lock(&seqMutex);
				nextSeq=true;
				pthread_mutex_unlock(&seqMutex);

				//full sound
			}
			break;
		}
	}

	pthread_mutex_unlock(&audioMutex);

}


void* playbackSeqThread(void* arg){
		while(true){
			pthread_mutex_lock(&seqMutex);
			if(nextSeq && externSeq){
				if(seq==0){
					//Hi-hat, Base
					AudioMixer_queueSound(&hithatFile);
					AudioMixer_queueSound(&basedrumFile);
					customsleep();

					nextSeq=false;
				}
				else if(seq==2){
					//Hi-hat
					AudioMixer_queueSound(&hithatFile);
					customsleep();
					nextSeq=false;
				}
				else if(seq==3){
					//Hi-hat, Snare
					AudioMixer_queueSound(&hithatFile);
					AudioMixer_queueSound(&snareFile);
					customsleep();
					nextSeq=false;
				}
				else if(seq==5){
					//Hi-hat
					AudioMixer_queueSound(&hithatFile);
					customsleep();
					seq=0;
					nextSeq=false;
				}

			}
			pthread_mutex_unlock(&seqMutex);

		}
		return NULL;

}


void customsleep(){

	long milliseconds=AudioMixer_getHalfBeatDelay();
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);

}


void* playbackThread(void* arg)
{

	while (!stopping) {
		if(externSeq){
			if(mode!=2){

			// Generate next block of audio
			fillPlaybackBuffer(playbackBuffer, playbackBufferSize);
//			snd_pcm_reset(handle);

			// Output the audio
			snd_pcm_sframes_t frames = snd_pcm_writei(handle,
					playbackBuffer, playbackBufferSize);


			// Check for (and handle) possible error conditions on output
			if (frames < 0) {
				fprintf(stderr, "AudioMixer: writei() returned %li\n", frames);
				frames = snd_pcm_recover(handle, frames, 1);
			}
			if (frames < 0) {
				fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %li\n",frames);
				exit(EXIT_FAILURE);
			}
			if (frames > 0 && frames < playbackBufferSize) {
				printf("Short write (expected %li, wrote %li)\n",
						playbackBufferSize, frames);
			}

			fflush(stdout);
			}
		}else{
			sep_player(externIndex);
			pthread_mutex_lock(&externSeqMutex);
			externSeq=true;
			pthread_mutex_unlock(&externSeqMutex);

		}
	}
	return NULL;
}


void standard_beats(){
	AudioMixer_readWaveFileIntoMemory(SOURCE_FILE_HH, &hithatFile);
	AudioMixer_readWaveFileIntoMemory(SOURCE_FILE_SN, &snareFile);
	AudioMixer_readWaveFileIntoMemory(SOURCE_FILE_BD, &basedrumFile);
}



void custom_beats(){
	AudioMixer_readWaveFileIntoMemory(beatbox_fileName[8], &hithatFile);
	AudioMixer_readWaveFileIntoMemory(beatbox_fileName[13], &snareFile);
	AudioMixer_readWaveFileIntoMemory(beatbox_fileName[7], &basedrumFile);
}




void AudioMixer_freeFileDatas(void){
	AudioMixer_freeWaveFileData(&hithatFile);
	AudioMixer_freeWaveFileData(&snareFile);
	AudioMixer_freeWaveFileData(&basedrumFile);
}


void Audio_playFile(int index)
{
	externIndex=index;
	pthread_mutex_lock(&externSeqMutex);
	externSeq=false;
	pthread_mutex_unlock(&externSeqMutex);

}

void sep_player(int index){

	wavedata_t pWaveData;
	AudioMixer_readWaveFileIntoMemory(beatbox_fileName[index], &pWaveData);

	// If anything is waiting to be written to screen, can be delayed unless flushed.
	fflush(stdout);

	// Write data and play sound (blocking)
	snd_pcm_sframes_t frames = snd_pcm_writei(handle, pWaveData.pData, pWaveData.numSamples);

	// Check for errors
	if (frames < 0)
		frames = snd_pcm_recover(handle, frames, 0);
	if (frames < 0) {
		fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %li\n", frames);
		exit(EXIT_FAILURE);
	}
	if (frames > 0 && frames < pWaveData.numSamples)
		printf("Short write (expected %d, wrote %li)\n", pWaveData.numSamples, frames);

}
