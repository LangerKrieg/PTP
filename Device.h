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

    static snd_pcm_format_t string_to_format(const std::string &str);
    static snd_pcm_stream_t string_to_mode(const std::string &str);

public:
    // There is two types of Constructor. One is needed for receive strings.
    Device(const std::string &hw, const std::string &format, const std::string &mode,
        unsigned int rate = 8000, unsigned int channels = 1);
    Device(const std::string &hw, snd_pcm_format_t format, snd_pcm_stream_t mode,
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
