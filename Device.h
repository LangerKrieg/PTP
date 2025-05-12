#pragma once
#include <alsa/asoundlib.h>
#include <string>

class Device {
public:
    Device(const std::string& name, snd_pcm_stream_t streamType);
    ~Device();

    void SetHWParams(unsigned int sampleRate, snd_pcm_format_t format, unsigned int channels);
    snd_pcm_t* GetHandle();

private:
    snd_pcm_t* handle;
};
