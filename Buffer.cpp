//
// Created by kuki on 5/8/25.
//

#include "Buffer.h"

#include <cstring>
#include <stdexcept>
#include <string>
#include <iostream>

Buffer::Buffer(const Device &capture_device, const Device &playback_device, UdpStreamer& udp)
    : captureDevice_(capture_device), playbackDevice_(playback_device), gateway_(udp), fillFrames_(0)
{
    if (capture_device.GetFormat() != playback_device.GetFormat() ||
        capture_device.GetChannels() != playback_device.GetChannels() ||
        capture_device.GetRate() != playback_device.GetRate() ||
        capture_device.GetPeriodSize() != playback_device.GetPeriodSize()) {

        throw std::runtime_error("Capture and playback devices have incompatible parameters");
    }

    // Configure the buffer using the recording device parameters
    frameSize_ = format_to_bytes(captureDevice_.GetFormat()) * captureDevice_.GetChannels();
    framesPerPeriod_ = captureDevice_.GetPeriodSize();
    bufferSize_ = frameSize_ * framesPerPeriod_ * 8;

    // Pointer to write to this buffer
    writeInBufPosition_ = 0;

    // Pointer to write to Alsa buffer
    writeInAlsaPosition_ = 0;

    // Setting buffer size
    buffer_.resize(bufferSize_);

    // Preparing devices to work
    snd_pcm_prepare(captureDevice_.GetHandle());
    snd_pcm_prepare(playbackDevice_.GetHandle());

    writeFromUdpPosition_ = 0;
}


int Buffer::format_to_bytes(const snd_pcm_format_t format)
{
    if (format == SND_PCM_FORMAT_S8) return 1;
    if (format == SND_PCM_FORMAT_S16_LE) return 2;
    if (format == SND_PCM_FORMAT_S24_LE) return 3;

    return -1;
}

// Capture from device
//snd_pcm_sframes_t Buffer::ReadFromDevice()
//{
//    std::lock_guard<std::mutex> lock(bufferMutex_);
//    // How much space is already filled
//    const size_t fill_bytes = (writeInBufPosition_ < writeInAlsaPosition_) ?
//        (writeInBufPosition_ + bufferSize_ - writeInAlsaPosition_) :
//        (writeInBufPosition_ - writeInAlsaPosition_);

//    const size_t fill_frames = snd_pcm_bytes_to_frames(captureDevice_.GetHandle(),
//        static_cast<ssize_t>(fill_bytes));

//    const size_t available_frames = (bufferSize_ / frameSize_) - fill_frames;

//    // If we have more free space(frames) than need for write one period.
//    if (available_frames >= framesPerPeriod_) {

//        // A loop to try again if an error occurs
//        for (int i = 0; i < 3; i++) {

//            // Reading from device from the InBuf pointer position
//            const snd_pcm_sframes_t read_frames = snd_pcm_readi(captureDevice_.GetHandle(),
//                buffer_.data() + writeInBufPosition_, framesPerPeriod_);

//            // Processing the errors
//            if (read_frames < 0) {
//                ErrorHandler(static_cast<int>(read_frames), SND_PCM_STREAM_CAPTURE);
//                if (i < 2) {
//                    std::cerr << "Wait 100ms then try again" << std::endl;
//                    usleep(100000);
//                }
//            }
//            else {
//                std::cout << "Read " << read_frames << " from " << captureDevice_.GetHw() <<
//                    ". Free space: " << available_frames - read_frames << '/' << bufferSize_ << std::endl;

//                // Move the pointer. If it goes beyond the buffer boundary, it will start again from 0
//                writeInBufPosition_ = (writeInBufPosition_ +
//                    snd_pcm_frames_to_bytes(captureDevice_.GetHandle(), read_frames)) % bufferSize_;

//                return read_frames;
//            }
//        }
//        throw std::runtime_error(std::string("Failed to read from device after 3 attempts"));
//    }
//    std::cerr << "No free space in the buffer at this time" << std::endl;
//    return 0;
//}

snd_pcm_sframes_t Buffer::ReadFromDevice() {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    // Проверяем, сколько места свободно перед записью с микрофона
    const size_t fill_bytes = (writeInBufPosition_ < writeFromUdpPosition_) ?
        (writeInBufPosition_ + bufferSize_ - writeFromUdpPosition_) :
        (writeInBufPosition_ - writeFromUdpPosition_);

    const size_t available_bytes = bufferSize_ - fill_bytes;
    const size_t available_frames = snd_pcm_bytes_to_frames(captureDevice_.GetHandle(), available_bytes);

    if (available_frames >= framesPerPeriod_) {
        for (int i = 0; i < 3; i++) {
            const snd_pcm_sframes_t read_frames = snd_pcm_readi(
                captureDevice_.GetHandle(),
                buffer_.data() + writeInBufPosition_,
                framesPerPeriod_
            );

            if (read_frames < 0) {
                ErrorHandler(static_cast<int>(read_frames), SND_PCM_STREAM_CAPTURE);
                if (i < 2) usleep(100000);
            } else {
                writeInBufPosition_ = (writeInBufPosition_ +
                    snd_pcm_frames_to_bytes(captureDevice_.GetHandle(), read_frames)) % bufferSize_;
                return read_frames;
            }
        }
        throw std::runtime_error("Failed to read from device after 3 attempts");
    }
    std::cerr << "No free space for microphone data" << std::endl;
    return 0;
}

// Playback
//snd_pcm_sframes_t Buffer::WriteToDevice()
//{
//    std::lock_guard<std::mutex> lock(bufferMutex_);
//    const size_t fill_bytes = (writeInBufPosition_ < writeInAlsaPosition_) ?
//        (writeInBufPosition_ + bufferSize_ - writeInAlsaPosition_) :
//        (writeInBufPosition_ - writeInAlsaPosition_);

//    fillFrames_ = snd_pcm_bytes_to_frames(playbackDevice_.GetHandle(), static_cast<ssize_t>(fill_bytes));

//    if (fillFrames_ >= framesPerPeriod_) {
//        for (int i = 0; i < 3; i++) {

//            // Using writei instead of readi
//            const snd_pcm_sframes_t written_frames = snd_pcm_writei(playbackDevice_.GetHandle(),
//                buffer_.data() + writeInAlsaPosition_, framesPerPeriod_);

//            if (written_frames < 0) {
//                ErrorHandler(static_cast<int>(written_frames), SND_PCM_STREAM_PLAYBACK);
//                if (i < 2) {
//                    std::cerr << "Wait 100ms then try again" << std::endl;
//                    usleep(100000);
//                }
//            }
//            else {
//                std::cout << "Device " << playbackDevice_.GetHw() << " received " << written_frames
//                    << " from buffer" << std::endl;
//                writeInAlsaPosition_ = (writeInAlsaPosition_ +
//                    snd_pcm_frames_to_bytes(playbackDevice_.GetHandle(), written_frames)) % bufferSize_;

//                return written_frames;
//            }
//        }
//        throw std::runtime_error(std::string("Failed to read from buffer after 3 attempts"));
//    }

//    std::cerr << "Not enough data to read from buffer. " << fillFrames_ << " frames available, "
//        << framesPerPeriod_ << " needed" << std::endl;
//    return 0;
//}

snd_pcm_sframes_t Buffer::WriteToDevice() {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    // Проверяем, сколько данных доступно для воспроизведения (только UDP)
    const size_t fill_bytes = (writeFromUdpPosition_ < writeInAlsaPosition_) ?
        (writeFromUdpPosition_ + bufferSize_ - writeInAlsaPosition_) :
        (writeFromUdpPosition_ - writeInAlsaPosition_);

    const size_t fill_frames = snd_pcm_bytes_to_frames(playbackDevice_.GetHandle(), fill_bytes);

    if (fill_frames >= framesPerPeriod_) {
        for (int i = 0; i < 3; i++) {
            const snd_pcm_sframes_t written_frames = snd_pcm_writei(
                playbackDevice_.GetHandle(),
                buffer_.data() + writeInAlsaPosition_,
                framesPerPeriod_
            );

            if (written_frames < 0) {
                ErrorHandler(static_cast<int>(written_frames), SND_PCM_STREAM_PLAYBACK);
                if (i < 2) usleep(100000);
            } else {
                writeInAlsaPosition_ = (writeInAlsaPosition_ +
                    snd_pcm_frames_to_bytes(playbackDevice_.GetHandle(), written_frames)) % bufferSize_;
                return written_frames;
            }
        }
        throw std::runtime_error("Failed to write to device after 3 attempts");
    }
    std::cerr << "Not enough UDP data to play" << std::endl;
    return 0;
}


//snd_pcm_sframes_t Buffer::SendToUDP() {
//    std::lock_guard<std::mutex> lock(bufferMutex_);
//    if (buffer_.empty()) {
//        std::cerr << "Buffer is empty, nothing to send" << std::endl;
//        return 0;
//    }

//    size_t bytes_to_send = snd_pcm_frames_to_bytes(captureDevice_.GetHandle(),
//        static_cast<ssize_t>(framesPerPeriod_));

//    if (bytes_to_send > 1024) bytes_to_send = 1024;

//    try {
//        if (writeInBufPosition_ >= bytes_to_send) {
//            gateway_.Send(&buffer_[0] + (writeInBufPosition_ - bytes_to_send), bytes_to_send);
//        }
//        else {
//            const size_t first_part = bytes_to_send - writeInBufPosition_;
//            gateway_.Send(&buffer_[0] + (bufferSize_ - first_part), first_part);
//            gateway_.Send(&buffer_[0], writeInBufPosition_);
//        }
//        std::cout << "Sent " << bytes_to_send << " bytes via UDP" << std::endl;
//        return snd_pcm_bytes_to_frames(captureDevice_.GetHandle(), static_cast<ssize_t>(bytes_to_send));
//    }
//    catch (const std::runtime_error& e) {
//        std::cerr << "UDP send error: " << e.what() << std::endl;
//        return 0;
//    }
//}

snd_pcm_sframes_t Buffer::SendToUDP() {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    const size_t bytes_to_send = 1024;  // Фиксированный размер пакета

    try {
        // Отправляем данные, которые были записаны с микрофона
        if (writeInBufPosition_ >= bytes_to_send) {
            gateway_.Send(&buffer_[0] + (writeInBufPosition_ - bytes_to_send), bytes_to_send);
        } else {
            const size_t first_part = bytes_to_send - writeInBufPosition_;
            gateway_.Send(&buffer_[0] + (bufferSize_ - first_part), first_part);
            gateway_.Send(&buffer_[0], writeInBufPosition_);
        }
        return bytes_to_send;
    } catch (const std::runtime_error& e) {
        std::cerr << "UDP send error: " << e.what() << std::endl;
        return 0;
    }
}

//snd_pcm_sframes_t Buffer::ReceiveFromUDP() {
//    std::lock_guard<std::mutex> lock(bufferMutex_);
//    if (buffer_.empty()) {
//        std::cerr << "Buffer is empty, cannot receive" << std::endl;
//        return 0;
//    }

////    const size_t bytes_to_receive = snd_pcm_frames_to_bytes(captureDevice_.GetHandle(),
////        static_cast<ssize_t>(framesPerPeriod_));

//    size_t bytes_to_receive = 1024;

//    char temp_buffer[bytes_to_receive];

//    try {
//        const size_t received_bytes = gateway_.Receive(temp_buffer, bytes_to_receive);
//        if (received_bytes == 0) {
//            if (writeInAlsaPosition_ + bytes_to_receive > bufferSize_) {
//                const size_t first_part = bufferSize_ - writeInAlsaPosition_;
//                std::memset(&buffer_[0] + writeInAlsaPosition_, 0, first_part);
//                std::memset(&buffer_[0], 0, bytes_to_receive - first_part);
//            }
//            else {
//                std::memset(&buffer_[0] + writeInAlsaPosition_, 0, bytes_to_receive);
//            }
//            writeInAlsaPosition_ = (writeInAlsaPosition_ + bytes_to_receive) % bufferSize_;
//            std::cout << "No UDP data received, filled with silence" << std::endl;
//            return snd_pcm_bytes_to_frames(captureDevice_.GetHandle(), static_cast<ssize_t>(bytes_to_receive));
//        }

//        const size_t fill_bytes = (writeInBufPosition_ < writeInAlsaPosition_) ?
//            (writeInBufPosition_ + bufferSize_ - writeInAlsaPosition_) :
//            (writeInBufPosition_ - writeInAlsaPosition_);

//        const size_t fill_frames = snd_pcm_bytes_to_frames(captureDevice_.GetHandle(),
//            static_cast<ssize_t>(fill_bytes));

//        const size_t available_frames = (bufferSize_ / frameSize_) - fill_frames;

//        if (available_frames < framesPerPeriod_) {
//            std::cerr << "Not enough space in buffer to store UDP data" << std::endl;
//            return 0;
//        }

//        if (writeInAlsaPosition_ + received_bytes <= bufferSize_) {
//            std::memcpy(&buffer_[0] + writeInAlsaPosition_, temp_buffer, received_bytes);
//        }
//        else {
//            const size_t first_part = bufferSize_ - writeInAlsaPosition_;
//            std::memcpy(&buffer_[0] + writeInAlsaPosition_, temp_buffer, first_part);
//            std::memcpy(&buffer_[0], temp_buffer + first_part, received_bytes - first_part);
//        }

//        writeInAlsaPosition_ = (writeInAlsaPosition_ + received_bytes) % bufferSize_;
//        std::cout << "Received " << received_bytes << " bytes via UDP" << std::endl;

//        return snd_pcm_bytes_to_frames(captureDevice_.GetHandle(), static_cast<ssize_t>(received_bytes));
//    }
//    catch (const std::runtime_error& e) {
//        std::cerr << "UDP receive error: " << e.what() << std::endl;
//        return 0;
//    }
//}


snd_pcm_sframes_t Buffer::ReceiveFromUDP() {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    const size_t bytes_to_receive = 1024;
    char temp_buffer[bytes_to_receive];

    try {
        const size_t received_bytes = gateway_.Receive(temp_buffer, bytes_to_receive);
        if (received_bytes == 0) {
            // Заполняем тишиной, если данных нет
            if (writeFromUdpPosition_ + bytes_to_receive > bufferSize_) {
                const size_t first_part = bufferSize_ - writeFromUdpPosition_;
                std::memset(&buffer_[0] + writeFromUdpPosition_, 0, first_part);
                std::memset(&buffer_[0], 0, bytes_to_receive - first_part);
            } else {
                std::memset(&buffer_[0] + writeFromUdpPosition_, 0, bytes_to_receive);
            }
            writeFromUdpPosition_ = (writeFromUdpPosition_ + bytes_to_receive) % bufferSize_;
            return 0;
        }

        // Записываем данные в буфер по writeFromUdpPosition_
        if (writeFromUdpPosition_ + received_bytes <= bufferSize_) {
            std::memcpy(&buffer_[0] + writeFromUdpPosition_, temp_buffer, received_bytes);
        } else {
            const size_t first_part = bufferSize_ - writeFromUdpPosition_;
            std::memcpy(&buffer_[0] + writeFromUdpPosition_, temp_buffer, first_part);
            std::memcpy(&buffer_[0], temp_buffer + first_part, received_bytes - first_part);
        }

        writeFromUdpPosition_ = (writeFromUdpPosition_ + received_bytes) % bufferSize_;
        return snd_pcm_bytes_to_frames(captureDevice_.GetHandle(), received_bytes);
    } catch (const std::runtime_error& e) {
        std::cerr << "UDP receive error: " << e.what() << std::endl;
        return 0;
    }
}

void Buffer::ErrorHandler(const int error, const snd_pcm_stream_t mode) const
{
    const Device & temp = (mode == SND_PCM_STREAM_CAPTURE) ? captureDevice_ : playbackDevice_;

    // -EPIPE and -ESTRPIPE can be fixed by buffer recover
    if (error == -EPIPE) {

        if (mode == SND_PCM_STREAM_CAPTURE) {
            std::cerr << "An overrun occurred. Trying to recover" << std::endl;
        }
        else {
            std::cerr << "An underrun occurred. Trying to recover" << std::endl;
        }
        RecoverBuffer(temp, error);
    }

    else if (error == -ESTRPIPE) {
        std::cerr << "Stream was suspended. Trying to recover" << std::endl;
        RecoverBuffer(temp, error);
    }

    // -EAGAIN reports about temporary unavailability. Can be fixed by waiting
    else if (error == -EAGAIN) {
        if ((snd_pcm_wait(temp.GetHandle(), 1000000)) <= 0) {
            throw std::runtime_error(std::string("Resource unavailable")
                + snd_strerror(error));
        }
    }

    // Other errors can`t be fixed
    else {
        throw std::runtime_error(std::string("Failed to read from device")
            + snd_strerror(error));
    }
}

// Wrap for buffer recovering with error handling
void Buffer::RecoverBuffer(const Device &device, const int error)
{
    const int return_code = snd_pcm_recover(device.GetHandle(), error, 1);
    if (return_code < 0) {
        throw std::runtime_error(std::string("Failed to recover buffer. ")
            + snd_strerror(return_code));
    }
    std::cerr << "The device was recovered" << std::endl;
}
