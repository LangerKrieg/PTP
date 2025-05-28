#pragma once
#ifndef BUFFER_H
#define BUFFER_H

#include "Device.h"
#include "udpstreamer.h"
#include <vector>
#include <mutex>
#include <alsa/asoundlib.h>

/**
 * @class Buffer
 * @brief Ядро аудио-потока - управление циркулярным буфером
 *
 * 🔹 Чтение с микрофона -> Буфер -> Отправка по UDP
 * 🔹 Прием по UDP -> Буфер -> Воспроизведение
 *
 * Особенности:
 * - Потокобезопасный доступ (std::mutex)
 * - Автоматическое восстановление при ошибках ALSA
 * - Циркулярный буфер на 8 периодов
 */
class Buffer {
public:
    Buffer(const Device& capture_device,
           const Device& playback_device,
           UdpStreamer& udp);

    // Основные операции (возвращают количество обработанных фреймов)
    snd_pcm_sframes_t ReadFromDevice();    // 🎤 -> 📦
    snd_pcm_sframes_t SendToUDP();         // 📦 -> 🌍
    snd_pcm_sframes_t ReceiveFromUDP();    // 🌍 -> 📦
    snd_pcm_sframes_t WriteToDevice();     // 📦 -> 🔊

private:
    // Конфигурация устройств
    const Device& captureDevice_;   // Устройство захвата (микрофон)
    const Device& playbackDevice_;  // Устройство воспроизведения (динамик)
    UdpStreamer& gateway_;          // UDP транспортер

    // Циркулярный буфер
    std::vector<char> buffer_;      // 📦 Данные аудио
    size_t bufferSize_;             // Общий размер буфера (в байтах)
    size_t frameSize_;              // Размер фрейма (в байтах)
    size_t framesPerPeriod_;        // Фреймов в периоде ALSA

    // Позиции в буфере
    size_t writeInBufPosition_;     // 🎤 Позиция записи с микрофона
    size_t writeFromUdpPosition_;   // 🌍 Позиция записи с UDP
    size_t writeInAlsaPosition_;    // 🔊 Позиция чтения для ALSA

    std::mutex bufferMutex_;        // 🔒 Защита доступа к буферу

    // Вспомогательные методы
    int format_to_bytes(snd_pcm_format_t format);
    void ErrorHandler(int err, snd_pcm_stream_t stream_type);
};

#endif // BUFFER_H
