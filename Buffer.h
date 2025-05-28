#pragma once
#ifndef BUFFER_H
#define BUFFER_H

#include "Device.h"
#include "udpstreamer.h"
#include <vector>
#include <mutex>
#include <alsa/asoundlib.h>

class Buffer {
public:
    Buffer(const Device& capture_device,
           const Device& playback_device,
           UdpStreamer& udp);

    // Основные операции (возвращают количество обработанных фреймов)
    snd_pcm_sframes_t ReadFromDevice();
    snd_pcm_sframes_t SendToUDP();
    snd_pcm_sframes_t ReceiveFromUDP();
    snd_pcm_sframes_t WriteToDevice();

private:
    // Конфигурация устройств
    const Device& captureDevice_;
    const Device& playbackDevice_;
    UdpStreamer& gateway_;

    // Циркулярный буфер
    std::vector<char> buffer_;
    size_t bufferSize_;
    size_t frameSize_;
    size_t framesPerPeriod_;

    // Позиции в буфере
    size_t writeInBufPosition_;
    size_t writeFromUdpPosition_;
    size_t writeInAlsaPosition_;

    std::mutex bufferMutex_;

    // Вспомогательные методы
    int format_to_bytes(snd_pcm_format_t format);
    void ErrorHandler(int err, snd_pcm_stream_t stream_type);
};

#endif // BUFFER_H
