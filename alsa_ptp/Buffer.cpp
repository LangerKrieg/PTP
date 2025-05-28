#include "Buffer.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

// Конструктор с проверкой совместимости устройств
Buffer::Buffer(const Device& capture_device,
               const Device& playback_device,
               UdpStreamer& udp)
    : captureDevice_(capture_device),
      playbackDevice_(playback_device),
      gateway_(udp),
      writeInBufPosition_(0),
      writeFromUdpPosition_(0),
      writeInAlsaPosition_(0)
{
    // Проверка совместимости параметров устройств
    if (capture_device.GetFormat() != playback_device.GetFormat() ||
        capture_device.GetChannels() != playback_device.GetChannels() ||
        capture_device.GetRate() != playback_device.GetRate() ||
        capture_device.GetPeriodSize() != playback_device.GetPeriodSize())
    {
        throw std::runtime_error("❌ Устройства захвата и воспроизведения несовместимы!");
    }

    // Расчет параметров буфера
    frameSize_ = format_to_bytes(captureDevice_.GetFormat()) * captureDevice_.GetChannels();
    framesPerPeriod_ = captureDevice_.GetPeriodSize();
    bufferSize_ = frameSize_ * framesPerPeriod_ * 8; // 8 периодов в буфере

    buffer_.resize(bufferSize_);

    // Подготовка ALSA устройств
    snd_pcm_prepare(captureDevice_.GetHandle());
    snd_pcm_prepare(playbackDevice_.GetHandle());

    snd_pcm_prepare(captureDevice_.GetHandle());
    snd_pcm_prepare(playbackDevice_.GetHandle());
    snd_pcm_drop(playbackDevice_.GetHandle()); // Сбросить буфер
    snd_pcm_prepare(playbackDevice_.GetHandle()); // Переподготовить

}

// Конвертация формата ALSA в размер в байтах
int Buffer::format_to_bytes(snd_pcm_format_t format) {
    switch (format) {
        case SND_PCM_FORMAT_S8: return 1;
        case SND_PCM_FORMAT_S16_LE: return 2;
        case SND_PCM_FORMAT_S24_LE: return 3;
        default: return -1;
    }
}

// Обработчик ошибок ALSA с автоматическим восстановлением
void Buffer::ErrorHandler(int err, snd_pcm_stream_t stream_type) {
    if (err == -EPIPE) {
        snd_pcm_t* handle = (stream_type == SND_PCM_STREAM_CAPTURE)
            ? captureDevice_.GetHandle()
            : playbackDevice_.GetHandle();
        snd_pcm_prepare(handle);
    }
}

// Чтение данных с устройства захвата
snd_pcm_sframes_t Buffer::ReadFromDevice() {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    // Расчет свободного места в буфере
    size_t free_bytes = (writeInBufPosition_ >= writeFromUdpPosition_)
        ? bufferSize_ - (writeInBufPosition_ - writeFromUdpPosition_)
        : writeFromUdpPosition_ - writeInBufPosition_;

    size_t free_frames = snd_pcm_bytes_to_frames(captureDevice_.GetHandle(), free_bytes);

    if (free_frames >= framesPerPeriod_) {
        for (int i = 0; i < 3; ++i) { // 3 попытки при ошибках
            auto* dest = buffer_.data() + writeInBufPosition_;
            snd_pcm_sframes_t read_frames = snd_pcm_readi(
                captureDevice_.GetHandle(),
                dest,
                framesPerPeriod_);

            if (read_frames >= 0) {
                writeInBufPosition_ = (writeInBufPosition_ +
                    snd_pcm_frames_to_bytes(captureDevice_.GetHandle(), read_frames))
                    % bufferSize_;
                return read_frames;
            }

            ErrorHandler(read_frames, SND_PCM_STREAM_CAPTURE);
            usleep(100000); // Пауза 100ms при ошибке
        }
    }
    return 0;
}

// Отправка данных по UDP
snd_pcm_sframes_t Buffer::SendToUDP() {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    size_t bytes_to_send = snd_pcm_frames_to_bytes(captureDevice_.GetHandle(), framesPerPeriod_);
    std::vector<char> temp(bytes_to_send);

    // Копирование из циркулярного буфера в линейный
    size_t start = (writeInBufPosition_ + bufferSize_ - bytes_to_send) % bufferSize_;
    if (start + bytes_to_send <= bufferSize_) {
        std::memcpy(temp.data(), &buffer_[start], bytes_to_send);
    } else {
        size_t first_part = bufferSize_ - start;
        std::memcpy(temp.data(), &buffer_[start], first_part);
        std::memcpy(temp.data() + first_part, &buffer_[0], bytes_to_send - first_part);
    }

    try {
        gateway_.Send(temp.data(), bytes_to_send);
        return snd_pcm_bytes_to_frames(captureDevice_.GetHandle(), bytes_to_send);
    } catch (...) {
        return 0;
    }
}

// Прием данных по UDP
snd_pcm_sframes_t Buffer::ReceiveFromUDP() {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    size_t bytes = snd_pcm_frames_to_bytes(playbackDevice_.GetHandle(), framesPerPeriod_);
    std::vector<char> temp(bytes);

    size_t received = gateway_.Receive(temp.data(), bytes);
    std::cout << "Received from UDP: " << received << " bytes" << std::endl; // Debug

    if (received == 0) {
        std::memset(temp.data(), 0, bytes);
    }

    // Запись в циркулярный буфер
    if (writeFromUdpPosition_ + bytes <= bufferSize_) {
        std::memcpy(&buffer_[writeFromUdpPosition_], temp.data(), bytes);
    } else {
        size_t first_part = bufferSize_ - writeFromUdpPosition_;
        std::memcpy(&buffer_[writeFromUdpPosition_], temp.data(), first_part);
        std::memcpy(&buffer_[0], temp.data() + first_part, bytes - first_part);
    }

    writeFromUdpPosition_ = (writeFromUdpPosition_ + bytes) % bufferSize_;
    return snd_pcm_bytes_to_frames(playbackDevice_.GetHandle(), bytes);
}

// Воспроизведение данных на устройстве
snd_pcm_sframes_t Buffer::WriteToDevice() {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    size_t available_bytes = (writeFromUdpPosition_ >= writeInAlsaPosition_)
        ? writeFromUdpPosition_ - writeInAlsaPosition_
        : bufferSize_ - (writeInAlsaPosition_ - writeFromUdpPosition_);

    size_t available_frames = snd_pcm_bytes_to_frames(playbackDevice_.GetHandle(), available_bytes);

    std::cout << "Available frames for playback: " << available_frames << std::endl; // Debug

    if (available_frames >= framesPerPeriod_) {
        for (int i = 0; i < 3; ++i) {
            auto* src = &buffer_[writeInAlsaPosition_];
            snd_pcm_sframes_t written = snd_pcm_writei(
                playbackDevice_.GetHandle(),
                src,
                framesPerPeriod_);

            if (written >= 0) {
                writeInAlsaPosition_ = (writeInAlsaPosition_ +
                    snd_pcm_frames_to_bytes(playbackDevice_.GetHandle(), written))
                    % bufferSize_;
                std::cout << "Frames written to device: " << written << std::endl; // Debug
                return written;
            }

            ErrorHandler(written, SND_PCM_STREAM_PLAYBACK);
            usleep(100000);
        }
    }
    return 0;
}
