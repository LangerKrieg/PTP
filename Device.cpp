//
// Created by kuki on 5/6/25.
//

#include "Device.h"

#include <stdexcept>
#include <iostream>

// Receive format and mode by string and convert it by string_to* functions
Device::Device(const std::string &hw, const std::string &format, const std::string &mode,
    const unsigned int rate, const unsigned int channels)
    : hw_(hw), deviceHandle_(0), periodSizeInFrames_(0), rate_(rate), channels_(channels)
{
    format_ = string_to_format(format);
    mode_ = string_to_mode(mode);

    InitializeDevice();
}

Device::Device(const std::string &hw, const snd_pcm_format_t format, const snd_pcm_stream_t mode,
    const unsigned int rate, const unsigned int channels)
    : hw_(hw), deviceHandle_(0), format_(format), mode_(mode), periodSizeInFrames_(0),
        rate_(rate), channels_(channels)
{
    InitializeDevice();
}

Device::~Device()
{
    if(deviceHandle_) {
        snd_pcm_close(deviceHandle_);
        deviceHandle_ = 0;
    }
}

// Combines all stages of device preparations
void Device::InitializeDevice()
{
    try {
        OpenDevice();
        ConfigureDevice();
    }
    catch(std::runtime_error &err) {
        std::string new_err("Problem with device ");
        new_err += hw_;
        new_err += ". ";
        new_err += err.what();
        throw std::runtime_error(new_err);
    }
}


void Device::OpenDevice()
{
    int return_code = 0;

    if((return_code = snd_pcm_open(&deviceHandle_, hw_.c_str(), mode_, 0)) < 0) {
        throw std::runtime_error(std::string("Failed to open audio interface ") + hw_
            + ". " + snd_strerror(return_code));
    }
    std::cout << "Audio interface " << hw_ << " opened" << std::endl;
}

// All device parameters like access, format, rate, channels sets here
void Device::ConfigureDevice()
{
    snd_pcm_hw_params_t *paramsHandle;
    int return_code = 0;

    // Memory allocation for struct with all parameters
    if((return_code = snd_pcm_hw_params_malloc(&paramsHandle)) < 0) {
        throw std::runtime_error(std::string("Failed to allocate memory for parameters. ")
            + snd_strerror(return_code));
    }
    std::cout << "Memory for audio parameters allocated" << std::endl;

    try {
        // Setting all default parameters - params_any
        if((return_code = snd_pcm_hw_params_any(deviceHandle_, paramsHandle)) < 0) {

            throw std::runtime_error(std::string("Failed to set default parameters. ")
                + snd_strerror(return_code));
        }
        std::cout << "Default parameters is set" << std::endl;

        // Now setting custom parameters
        if((return_code = snd_pcm_hw_params_set_access(deviceHandle_, paramsHandle,
            SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
            throw std::runtime_error(std::string("Failed to set access parameter. ")
                + snd_strerror(return_code));
            }
        std::cout << "Access parameter is set" << std::endl;


        if((return_code = snd_pcm_hw_params_set_format(deviceHandle_, paramsHandle, format_)) < 0) {
            throw std::runtime_error(std::string("Failed to set format. ")
                + snd_strerror(return_code));
        }
        std::cout << "Format is set" << std::endl;


        if((return_code = snd_pcm_hw_params_set_rate_near(deviceHandle_, paramsHandle, &rate_, 0)) < 0) {
            throw std::runtime_error(std::string("Failed to set sample rate. ")
                + snd_strerror(return_code));
        }
        std::cout << "Sample rate is set " << std::endl;


        if((return_code = snd_pcm_hw_params_set_channels_near(deviceHandle_, paramsHandle, &channels_)) < 0) {
            throw std::runtime_error(std::string("Failed to set channels amount. ")
                + snd_strerror(return_code));
        }
        std::cout << "Channels amount is set" << std::endl;

        // Apply all the set parameters
        if((return_code = snd_pcm_hw_params(deviceHandle_, paramsHandle)) < 0) {
            throw std::runtime_error(std::string("Failed to confirm parameters. ")
                + snd_strerror(return_code));
        }
        std::cout << "All parameters is set" << std::endl;

        // Get some necessary parameters. It will be needed for buffer
        if((return_code = snd_pcm_hw_params_get_period_size(paramsHandle, &periodSizeInFrames_, 0)) < 0) {
            throw std::runtime_error(std::string("Failed to get period size from device")
                + snd_strerror(return_code));
        }
        std::cout << "Period size received" << std::endl;
    }
    catch(...) {
        snd_pcm_hw_params_free(paramsHandle);
        throw;
    }
    snd_pcm_hw_params_free(paramsHandle);
}


snd_pcm_format_t Device::string_to_format(const std::string &str) {
    if (str == "8") {
        return SND_PCM_FORMAT_S8;
    }
    if (str == "16") {
        return SND_PCM_FORMAT_S16_LE;
    }
    if (str == "24") {
        return SND_PCM_FORMAT_S24_LE;
    }
    throw std::runtime_error(std::string("Unknown format: ") + str);
}


snd_pcm_stream_t Device::string_to_mode(const std::string &str) {

    // We are receiving only one symbol
    if (str.length() != 1) {
        throw std::runtime_error(std::string("Unknown mode parameter: ") + str
            + ". Try to enter only C or P");
    }

    switch (str[0]) {
        case 'C':
            return SND_PCM_STREAM_CAPTURE;
        case 'P':
            return SND_PCM_STREAM_PLAYBACK;
        default:
            throw std::runtime_error(std::string("Unknown mode parameter: ") + str);
    }
}
