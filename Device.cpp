#include "Device.h"
#include <stdexcept>

Device::Device(const std::string& name, snd_pcm_stream_t streamType) {
    if (snd_pcm_open(&handle, name.c_str(), streamType, 0) < 0) {
        throw std::runtime_error("Failed to open device: " + name);
    }
}

Device::~Device() {
    if (handle) {
        snd_pcm_close(handle);
    }
}

void Device::SetHWParams(unsigned int sampleRate, snd_pcm_format_t format, unsigned int channels) {
    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, format);
    snd_pcm_hw_params_set_channels(handle, params, channels);
    snd_pcm_hw_params_set_rate_near(handle, params, &sampleRate, nullptr);
    snd_pcm_hw_params(handle, params);
}

snd_pcm_t* Device::GetHandle() {
    return handle;
}
