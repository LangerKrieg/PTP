//
// Created by kuki on 5/8/25.
//

#ifndef BUFFER_H
#define BUFFER_H

#include <alsa/asoundlib.h>
#include <vector>

#include "Device.h"
#include "udpstreamer.h"

#include <mutex>

class Buffer {
private:
    std::vector<char> buffer_;

    const Device &captureDevice_;
    const Device &playbackDevice_;

    UdpStreamer &gateway_;

    snd_pcm_uframes_t framesPerPeriod_;

    size_t frameSize_;
    size_t bufferSize_;
    size_t fillFrames_;

    size_t writeInBufPosition_;
    size_t writeInAlsaPosition_;

    static int format_to_bytes(snd_pcm_format_t format);

    std::mutex bufferMutex_;
    size_t writeFromUdpPosition_;

    void ErrorHandler(int error, snd_pcm_stream_t mode) const;
    static void RecoverBuffer(const Device &device, int error);

public:
    explicit Buffer(const Device &capture_device, const Device &playback_device,
        UdpStreamer& udp);

    snd_pcm_sframes_t ReadFromDevice();
    snd_pcm_sframes_t WriteToDevice();

    snd_pcm_sframes_t ReceiveFromUDP();
    snd_pcm_sframes_t SendToUDP();

    unsigned int GetFillFrames() const { return fillFrames_; }
    unsigned int GetFramesPerPeriod() const { return framesPerPeriod_; }
};



#endif //BUFFER_H
