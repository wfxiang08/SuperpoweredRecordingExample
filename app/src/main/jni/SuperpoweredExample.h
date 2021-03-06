#ifndef Header_SuperpoweredExample
#define Header_SuperpoweredExample

#include <math.h>
#include <pthread.h>

#include "SuperpoweredExample.h"
#include "../../../../../../Superpowered/SuperpoweredAdvancedAudioPlayer.h"
#include "../../../../../../Superpowered/SuperpoweredFilter.h"
#include "../../../../../../Superpowered/SuperpoweredRoll.h"
#include "../../../../../../Superpowered/SuperpoweredFlanger.h"
#include "../../../../../../Superpowered/SuperpoweredAndroidAudioIO.h"
#include "../../../../../../Superpowered/SuperpoweredRecorder.h"

#define NUM_BUFFERS 2
#define HEADROOM_DECIBEL 3.0f
static const float headroom = powf(10.0f, -HEADROOM_DECIBEL * 0.025);

const int MP3_SIZE = 8192;

class SuperpoweredExample {
public:

	SuperpoweredExample(const char *path, int *params);
	~SuperpoweredExample();

	bool process(short int *output, unsigned int numberOfSamples);
	void onPlayPause(bool play);
	void onCrossfader(int value);
	void onFxSelect(int value);
	void onFxOff();
	void onFxValue(int value);

    //Added function declaration
    void onRecord(bool record);

private:
    pthread_mutex_t mutex;
    SuperpoweredAndroidAudioIO *audioSystem;
    SuperpoweredAdvancedAudioPlayer *playerA, *playerB;
    SuperpoweredRoll *roll;
    SuperpoweredFilter *filter;
    SuperpoweredFlanger *flanger;
    float *stereoBuffer;
    float *recorderBuffer;  //J
    short int *mp3Output;
    unsigned char activeFx;
    float crossValue, volA, volB;

    SuperpoweredRecorder * recorder;
    //added object variables
    const char *tempPath;
    const char *destinationPath;

};

#endif
