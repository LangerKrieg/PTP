//
// Created by kuki on 5/6/25.
//

#ifndef DEVICE_H
#define DEVICE_H

#include <string>
#include <alsa/asoundlib.h>

class Device {
private:
    std::string hw_;

    snd_pcm_t *deviceHandle_;

    snd_pcm_format_t format_;
    snd_pcm_stream_t mode_;
    snd_pcm_uframes_t periodSizeInFrames_;

    unsigned int rate_;
    unsigned int channels_;

    void OpenDevice();
    void ConfigureDevice();
    void InitializeDevice();

public:
    Device(const std::string &hw, snd_pcm_stream_t mode, snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE,
        unsigned int rate = 8000, unsigned int channels = 1);
    ~Device();

    std::string GetHw() const { return hw_; }

    snd_pcm_t *GetHandle() const { return deviceHandle_; }

    snd_pcm_format_t GetFormat() const { return format_; }
    snd_pcm_stream_t GetMode() const { return mode_; }
    snd_pcm_uframes_t GetPeriodSize() const { return periodSizeInFrames_;  }

    unsigned int GetRate() const { return rate_; }
    unsigned int GetChannels() const { return channels_; }

};


#endif //DEVICE_H
