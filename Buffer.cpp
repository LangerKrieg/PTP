#include "Buffer.h"
#include <iostream>

Buffer::Buffer(Device& input, Device& output)
    : inputDevice(input), outputDevice(output), data(8000) {} // 1 second buffer at 44100 Hz

void Buffer::ReadFromDevice() {
    int frames = snd_pcm_readi(inputDevice.GetHandle(), data.data(), data.size());
    if (frames < 0) {
        snd_pcm_prepare(inputDevice.GetHandle());
    }
}

void Buffer::WriteToDevice() {
    int frames = snd_pcm_writei(outputDevice.GetHandle(), data.data(), data.size());
    if (frames < 0) {
        snd_pcm_prepare(outputDevice.GetHandle());
    }
}
