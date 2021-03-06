#include "SuperpoweredExample.h"
#include "SuperpoweredSimple.h"
#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <android/log.h>
#include "lame_3.99.5_libmp3lame/lame.h"

static lame_global_flags *lame = NULL;
static FILE *mp3 = NULL;
static unsigned char mp3Buffer[MP3_SIZE];

static void playerEventCallbackA(void *clientData, SuperpoweredAdvancedAudioPlayerEvent event, void *value) {
    if (event == SuperpoweredAdvancedAudioPlayerEvent_LoadSuccess) {
    	SuperpoweredAdvancedAudioPlayer *playerA = *((SuperpoweredAdvancedAudioPlayer **)clientData);
        playerA->setBpm(126.0f);
        playerA->setFirstBeatMs(353);
        playerA->setPosition(playerA->firstBeatMs, false, false);
    };
}

static void playerEventCallbackB(void *clientData, SuperpoweredAdvancedAudioPlayerEvent event, void *value) {
     if (event == SuperpoweredAdvancedAudioPlayerEvent_LoadSuccess) {
     	SuperpoweredAdvancedAudioPlayer *playerB = *((SuperpoweredAdvancedAudioPlayer **)clientData);
         playerB->setBpm(123.0f);
         playerB->setFirstBeatMs(40);
         playerB->setPosition(playerB->firstBeatMs, false, false);
     };
}

static bool audioProcessing(void *clientdata, short int *audioIO, int numberOfSamples, int samplerate) {
	return ((SuperpoweredExample *)clientdata)->process(audioIO, numberOfSamples);
}

SuperpoweredExample::SuperpoweredExample(const char *path, int *params) : activeFx(0), crossValue(0.0f), volB(0.0f), volA(1.0f * headroom) {
    pthread_mutex_init(&mutex, NULL); // This will keep our player volumes and playback states in sync.
    unsigned int samplerate = params[4], buffersize = params[5];
    stereoBuffer = (float *)memalign(16, (buffersize + 16) * sizeof(float) * 2);
    recorderBuffer = (float *) memalign(16, (buffersize + 16) * sizeof(float) * 2); //J
    mp3Output = (short int *) memalign(16, (buffersize + 16) * sizeof(short int) * 2); //J

    playerA = new SuperpoweredAdvancedAudioPlayer(&playerA , playerEventCallbackA, samplerate, 0);
    playerA->open(path, params[0], params[1]);

    playerB = new SuperpoweredAdvancedAudioPlayer(&playerB, playerEventCallbackB, samplerate, 0);
    playerB->open(path, params[0], params[1]);

    playerA->syncMode = playerB->syncMode = SuperpoweredAdvancedAudioPlayerSyncMode_TempoAndBeat;

    recorder = new SuperpoweredRecorder("/sdcard/superpowered/recording.tmp", samplerate);   //J
    audioSystem = new SuperpoweredAndroidAudioIO(samplerate, buffersize, true, true, audioProcessing, this, buffersize * 2);

    //LAME
    if (lame != NULL) {
        lame_close(lame);
        lame = NULL;
    }
    lame = lame_init();
    lame_set_in_samplerate(lame, samplerate);
    lame_set_num_channels(lame, 2);//输入流的声道
    lame_set_out_samplerate(lame, samplerate);
    //lame_set_brate(lame, buffersize);
    lame_set_quality(lame, 7);
    lame_init_params(lame);

}

SuperpoweredExample::~SuperpoweredExample() {
    delete playerA;
    delete playerB;
    delete recorder;
    delete audioSystem;
    free(stereoBuffer);
    free(recorderBuffer);   //J
    pthread_mutex_destroy(&mutex);
}

void SuperpoweredExample::onPlayPause(bool play) {
    pthread_mutex_lock(&mutex);
    if (!play) {
        playerA->pause();
        playerB->pause();
        recorder->stop();
    } else {
        bool masterIsA = (crossValue <= 0.5f);
        playerA->play(!masterIsA);
        playerB->play(!masterIsA);
        recorder->start("/sdcard/superpowered/recording1.mp3");
    };
    pthread_mutex_unlock(&mutex);
}

void SuperpoweredExample::onCrossfader(int value) {
    pthread_mutex_lock(&mutex);
    crossValue = float(value) * 0.01f;
    if (crossValue < 0.01f) {
        volA = 1.0f * headroom;
        volB = 0.0f;
    } else if (crossValue > 0.99f) {
        volA = 0.0f;
        volB = 1.0f * headroom;
    } else { // constant power curve
        volA = cosf(M_PI_2 * crossValue) * headroom;
        volB = cosf(M_PI_2 * (1.0f - crossValue)) * headroom;
    };
    pthread_mutex_unlock(&mutex);
}

void SuperpoweredExample::onFxSelect(int value) {
	__android_log_print(ANDROID_LOG_VERBOSE, "SuperpoweredExample", "FXSEL %i", value);
	activeFx = value;
}

void SuperpoweredExample::onFxOff() {
    // filter->enable(false);
    // roll->enable(false);
    // flanger->enable(false);
}

#define MINFREQ 60.0f
#define MAXFREQ 20000.0f

static inline float floatToFrequency(float value) {
    if (value > 0.97f) return MAXFREQ;
    if (value < 0.03f) return MINFREQ;
    value = powf(10.0f, (value + ((0.4f - fabsf(value - 0.4f)) * 0.3f)) * log10f(MAXFREQ - MINFREQ)) + MINFREQ;
    return value < MAXFREQ ? value : MAXFREQ;
}

void SuperpoweredExample::onFxValue(int ivalue) {
    // float value = float(ivalue) * 0.01f;
    // switch (activeFx) {
    //     case 1:
    //         filter->setResonantParameters(floatToFrequency(1.0f - value), 0.2f);
    //         filter->enable(true);
    //         flanger->enable(false);
    //         roll->enable(false);
    //         break;
    //     case 2:
    //         if (value > 0.8f) roll->beats = 0.0625f;
    //         else if (value > 0.6f) roll->beats = 0.125f;
    //         else if (value > 0.4f) roll->beats = 0.25f;
    //         else if (value > 0.2f) roll->beats = 0.5f;
    //         else roll->beats = 1.0f;
    //         roll->enable(true);
    //         filter->enable(false);
    //         flanger->enable(false);
    //         break;
    //     default:
    //         flanger->setWet(value);
    //         flanger->enable(true);
    //         filter->enable(false);
    //         roll->enable(false);
    // };
}

bool SuperpoweredExample::process(short int *output, unsigned int numberOfSamples) {
    pthread_mutex_lock(&mutex);

    bool silence = !playerB->process(recorderBuffer, false, numberOfSamples);

    SuperpoweredShortIntToFloat(output, stereoBuffer, numberOfSamples);
    silence = !playerA->process(stereoBuffer, true, numberOfSamples);

    //silence = !playerA->process(stereoBuffer, false, numberOfSamples);
    pthread_mutex_unlock(&mutex);

    recorder->process(stereoBuffer, NULL, numberOfSamples);

    if (!silence) {


        memset(mp3Output, 0, sizeof(mp3Output));
        SuperpoweredFloatToShortInt(stereoBuffer, mp3Output, numberOfSamples);
        /* LAME */
        int result = lame_encode_buffer_interleaved(lame, mp3Output, numberOfSamples, mp3Buffer, MP3_SIZE);
        if (result > 0) {
            __android_log_print(ANDROID_LOG_ERROR, "TEST", "lame result = %d", result);
            fwrite(mp3Buffer, result, 1, mp3);
        }
        /* LAME */

        SuperpoweredFloatToShortInt(recorderBuffer, output, numberOfSamples);
    }

    return !silence;
}

extern "C" {
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_SuperpoweredExample(JNIEnv *javaEnvironment, jobject self, jstring apkPath, jlongArray offsetAndLength);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onPlayPause(JNIEnv *javaEnvironment, jobject self, jboolean play);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onCrossfader(JNIEnv *javaEnvironment, jobject self, jint value);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onFxSelect(JNIEnv *javaEnvironment, jobject self, jint value);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onFxOff(JNIEnv *javaEnvironment, jobject self);
	JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onFxValue(JNIEnv *javaEnvironment, jobject self, jint value);
}

static SuperpoweredExample *example = NULL;

// Android is not passing more than 2 custom parameters, so we had to pack file offsets and lengths into an array.
JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_SuperpoweredExample(JNIEnv *javaEnvironment, jobject self, jstring apkPath, jlongArray params) {
	// Convert the input jlong array to a regular int array.
    jlong *longParams = javaEnvironment->GetLongArrayElements(params, JNI_FALSE);
    int arr[6];
    for (int n = 0; n < 6; n++) arr[n] = longParams[n];
    javaEnvironment->ReleaseLongArrayElements(params, longParams, JNI_ABORT);

    const char *path = javaEnvironment->GetStringUTFChars(apkPath, JNI_FALSE);
    example = new SuperpoweredExample(path, arr);
    javaEnvironment->ReleaseStringUTFChars(apkPath, path);

}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onPlayPause(JNIEnv *javaEnvironment, jobject self, jboolean play) {
    if (play) {
        __android_log_print(ANDROID_LOG_ERROR, "TEST", "Play!");
        mp3 = fopen("/sdcard/superpowered/test.mp3", "wb");
    }else {
        __android_log_print(ANDROID_LOG_ERROR, "TEST", "STOP!");
        int result = lame_encode_flush(lame,mp3Buffer, MP3_SIZE);
        if(result > 0) {
            fwrite(mp3Buffer, result, 1, mp3);
        }
        lame_close(lame);
        fclose(mp3);
    }
	example->onPlayPause(play);
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onCrossfader(JNIEnv *javaEnvironment, jobject self, jint value) {
	example->onCrossfader(value);
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onFxSelect(JNIEnv *javaEnvironment, jobject self, jint value) {
	example->onFxSelect(value);
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onFxOff(JNIEnv *javaEnvironment, jobject self) {
	example->onFxOff();
}

JNIEXPORT void Java_com_superpowered_crossexample_MainActivity_onFxValue(JNIEnv *javaEnvironment, jobject self, jint value) {
	example->onFxValue(value);
}
