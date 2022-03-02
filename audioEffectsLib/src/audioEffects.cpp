#include "audioEffects.h"
#include "reverbLib.h"
#include <stdio.h>
#include <iostream>

int audioEffects::returnFour()
{
    return 4;
}

audioEffects::audioEffects()
{
    tremoloCounter = 0;
    for(int  i=0; i<TABLE_SIZE; i++ )
    {
        sine[i] = 0.2 * (float) sin( ((double)i/(double)TABLE_SIZE) * PI * 2. );
    }

    int channels = 1;
    int sampleRate = 44100;
    int test = verblib_initialize(&verb0, sampleRate, channels); 
    //std::cout << test << " verb init \n" << std::flush;//error check



    configQueue = new moodycamel::ReaderWriterQueue<audioEffectsConfig> (1);
    config.overDriveEnabled = false;
    config.tremoloEnabled = false;
    config.distortEnabled = false;
    config.reverbEnabled = false;
    config.bypassEnabled = false;

}

audioEffects::~audioEffects()
{

}


void audioEffects::process(float *inputBuffer, float *outputBuffer, size_t size)
{

    if(configQueue->size_approx() == 1)
    {
        configQueue->try_dequeue(config);
    }

    float *currInput = inputBuffer;
    if(config.bypassEnabled)
    {
        memcpy(outputBuffer, inputBuffer, size * sizeof(inputBuffer[0]));
        return;
    }
    if(config.preAmpEnabled)
    {
        this->preAmp(currInput, outputBuffer, size, config.preAmpGain);
        if(currInput == inputBuffer)
        {
            currInput = outputBuffer;
        }
    }

    for(int x = 1; x < 5; x++)
    {
        if(config.overDriveOrderNumber == x && config.overDriveEnabled)
        {
            this->overdriveEffect(currInput,outputBuffer,size,config.overDriveThresh);
            if(currInput == inputBuffer)
            {
                currInput = outputBuffer;
            }
        }
        if(config.tremoloOrderNumber == x && config.tremoloEnabled)
        {
            this->tremoloEffect_2(currInput,outputBuffer,size, config.tremoloFreq, config.tremoloDepth);
            if(currInput == inputBuffer)
            {
                currInput = outputBuffer;
            }
        }
        if(config.distortOrderNumber == x && config.distortEnabled)
        {
            this->distortEffect(currInput,outputBuffer,size,config.distortThresh);
            if(currInput == inputBuffer)
            {
                currInput = outputBuffer;
            }
        }
        if(config.reverbOrderNumber == x && config.reverbEnabled)
        {
            this->reverbEffect(currInput,outputBuffer,size, 44100, config.reverbWetLevel, config.reverbRoomSize, config.reverbDryLevel, config.reverbDampLevel, config.reverbWidth, config.reverbMode);
            if(currInput == inputBuffer)
            {          
                currInput = outputBuffer;
            }
        }        
        
    }
    if(currInput == inputBuffer)
    {
        memcpy(outputBuffer, inputBuffer, size * sizeof(inputBuffer[0]));
    }

}

void audioEffects::preAmp(float *inputBuffer, float *outputBuffer, size_t size, float gain)
{
    for(int i = 0; i < size; i++)
    {
        outputBuffer[i] = gain * inputBuffer[i];
    }  
}

void audioEffects::recieveConfig(void)
{
    std::cout << "TEST";
    httplib::Client cli("http://localhost:4996");
    auto res = cli.Get("/effectsProfile/selectedProfile/");
    if(res == nullptr)
    {
        printf("Null ptr");
        fflush(stdout);
        return;
    }
    if(res->status != 200)
    {
        std::cout << "GET RESPONSE: " << res->status;
        return;
    }
    std::cout << res->status;
    fflush(stdout);

    json j_complete = json::parse(res->body);

    std::cout << std::setw(4) << j_complete << "\n\n";
    std::cout << j_complete["author"] << "\n\n";

    audioEffectsConfig tempConfig = {};
    
    tempConfig.overDriveEnabled = j_complete["overDriveEnabled"];
    tempConfig.overDriveOrderNumber = j_complete["overDriveOrderNumber"];
    tempConfig.overDriveThresh = j_complete["overDriveThresh"];
    
    tempConfig.distortEnabled = j_complete["distortEnabled"];
    tempConfig.distortOrderNumber = j_complete["distortOrderNumber"];
    tempConfig.distortThresh = j_complete["distortThresh"];
    

    tempConfig.tremoloDepth = j_complete["tremoloDepth"];
    tempConfig.tremoloEnabled = j_complete["tremoloEnabled"];
    tempConfig.tremoloFreq = j_complete["tremoloFreq"];
    tempConfig.tremoloOrderNumber = j_complete["tremoloOrderNumber"];
    
    tempConfig.reverbWetLevel = j_complete["reverbWetLevel"];
    tempConfig.reverbRoomSize = j_complete["reverbRoomSize"];    
    tempConfig.reverbDryLevel = j_complete["reverbDryLevel"];    
    tempConfig.reverbDampLevel = j_complete["reverbDampLevel"];    
    tempConfig.reverbWidth = j_complete["reverbWidth"];    
    tempConfig.reverbMode = static_cast<float>(j_complete["reverbMode"]);    
    tempConfig.reverbEnabled = j_complete["reverbEnabled"];    
    tempConfig.reverbOrderNumber = j_complete["reverbOrderNumber"];    
    
    tempConfig.preAmpGain = j_complete["preAmpGain"];    
    tempConfig.preAmpEnabled = j_complete["preAmpEnabled"];    
    

    res = cli.Get("/byPass/");
    if(res == nullptr)
    {
        printf("Null ptr");
        fflush(stdout);
        return;
    }
    if(res->status != 200)
    {
        std::cout << "GET RESPONSE: " << res->status;
        return;
    }
    std::cout << res->status;
    fflush(stdout);

    j_complete = json::parse(res->body);

    std::cout << std::setw(4) << j_complete << "\n\n";
    std::cout << j_complete["author"] << "\n\n";

    
    tempConfig.bypassEnabled = j_complete["bypassEnabled"];

    while(configQueue->size_approx() == 1)
    {

    }

    if(configQueue->size_approx() != 1)
    {
        if(!configQueue->try_enqueue(tempConfig))
        {
            throw std::invalid_argument("FAILED TO PUSH FULL FRAME!");
        }
    }
}
void audioEffects::tremoloEffect(float *inputBuffer, float *outputBuffer, size_t size, float numOscPerSecond, unsigned int sampleRate)
{
    //TODO: VERIFY THAT NO DIVIDE BY ZERO ERRORS OCCUR!!!

    //In order for us to 'oscilate' the input signal n number of times per second, we need to 
    // divide the duration of desired oscilation (for example if we wanted 4 oscllations per second, this would be 0.25 seconds) by
    // the duration of the sine wave's period (our sine wave table size at this time is 512 samples, with a sample rate of 44100, this is 0.0116 seconds).
    // the division results in how much slower we need to traverse the sine wave table.
    float sineWaveDuration = (float)(1.0/sampleRate) * TABLE_SIZE;
    float oscDuration = 1.0/numOscPerSecond;

    int sineWaveDivisor =(int)(oscDuration/sineWaveDuration);
    for(int x = 0; x < size; x++)
    {
        //Multiply input buffer by sin wave, we also multiple 12.0 just for a volume increase, since it's fairly quiet without any extra multiplication
        outputBuffer[x] = 12.0 * inputBuffer[x] * sine[(tremoloCounter/sineWaveDivisor)%TABLE_SIZE]; //1/sineWaveDivisor = (freq * 512)/44100
        tremoloCounter += 1;
        //resetting our tremolo counter arbitrarily after 500000, to avoid overflows (technically we don't need this since tremoloCounter is an unsigned int...)
        if(tremoloCounter > 800000000)
        {
            tremoloCounter = 0;
        }
    }
}

void audioEffects::distortEffect(float *inputBuffer, float *outputBuffer, size_t size, float thresh)
{
    for(int i = 0; i < size; i++)
    {
        if(inputBuffer[i] > thresh)
            outputBuffer[i] = thresh;
        else if(inputBuffer[i] < -thresh)
            outputBuffer[i] = -thresh;
        else
            outputBuffer[i] = inputBuffer[i];
    }
}

void audioEffects::overdriveEffect(float *inputBuffer, float *outputBuffer, size_t size, float a)
{
    for(int i = 0; i < size; i++)
    {					//a maybe should range from 2-3
        outputBuffer[i] = (2./PI) * atan(inputBuffer[i] * a); // double atan(double x)
    }  
}

void audioEffects::reverbEffect(const float* inputBuffer, float* outputBuffer, unsigned long frames, unsigned int sampleRate, float reverbWetLevel, float reverbRoomSize, float reverbDryLevel, float reverbDampLevel, float reverbWidth, float reverbMode)
{
    //2.17: add verblib_init's set functions
    verblib_set_wet ( &verb0, reverbWetLevel );
    verblib_set_room_size ( &verb0, reverbRoomSize );
    verblib_set_dry ( &verb0, reverbDryLevel );
    verblib_set_damping ( &verb0, reverbDampLevel );
    verblib_set_width ( &verb0, reverbWidth );
    verblib_set_mode ( &verb0, reverbMode );
    //verblib_mute ( &verb0 );

    verblib_process(&verb0, inputBuffer, outputBuffer, frames);
    
}



void audioEffects::tremoloEffect_2(float *inputBuffer, float *outputBuffer, size_t size, float freq, int depth)
{   // freq: numOscPerSecond
    float a = depth/200.0;
    float offset = 1.0 - a; // bounds magnitude of output: [0, |input|]
    
    //on matlab, vector lfo is multiplied with input vector
    for(int i = 0; i < size; i++)
    {
        //outputBuffer[i] = inputBuffer[i] * (a * sin(2. * PI * freq * i) + offset); // 'i' in sin() is 't = (0:length(in)-1)/Fs;' where Fs = #ofSamples
        outputBuffer[i] = inputBuffer[i] * (a * sin(2.0 * PI * freq * (tremoloCounter/48000.0)) + offset);
        tremoloCounter++;
	if(tremoloCounter > 800000000)
	{
	    tremoloCounter = 0;
	}
    }

}


