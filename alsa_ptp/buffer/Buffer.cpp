//
// Created by kuki on 5/8/25.
//

#include "Buffer.h"

#include <stdexcept>
#include <string>
#include <iostream>

Buffer::Buffer(const Device &capture_device, const Device &playback_device)
	: captureDevice_(capture_device), playbackDevice_(playback_device)
{
	// Configure the buffer using the recording device parameters
	frameSize_ = format_to_bytes(captureDevice_.GetFormat()) * captureDevice_.GetChannels();
	framesPerPeriod_ = captureDevice_.GetPeriodSize();
	bufferSize_ = frameSize_ * framesPerPeriod_ * 2;

	// Pointer to write to this buffer
	writeInBufPosition_ = 0;

	// Pointer to write to Alsa buffer
	writeInAlsaPosition_ = 0;

	// Setting buffer size
	buffer_.resize(bufferSize_);

	// Preparing devices to work
	snd_pcm_prepare(captureDevice_.GetHandle());
	snd_pcm_prepare(playbackDevice_.GetHandle());
}


int Buffer::format_to_bytes(const snd_pcm_format_t format)
{
	if(format == SND_PCM_FORMAT_S8) return 1;
	if(format == SND_PCM_FORMAT_S16_LE) return 2;
	if(format == SND_PCM_FORMAT_S24_LE) return 3;

	return -1;
}

// Capture from device
snd_pcm_sframes_t Buffer::ReadFromDevice()
{
	// How much space is already filled
	const unsigned int fill_bytes = (writeInBufPosition_ < writeInAlsaPosition_) ?
		(writeInBufPosition_ + bufferSize_ - writeInAlsaPosition_) :
		(writeInBufPosition_ - writeInAlsaPosition_);

	const unsigned int fill_frames = snd_pcm_bytes_to_frames(captureDevice_.GetHandle(), fill_bytes);
	const unsigned int available_frames = (bufferSize_ / frameSize_) - fill_frames;

	// If we have more free space(frames) than need for write one period.
	if(available_frames >= framesPerPeriod_) {

		// A loop to try again if an error occurs
		for(int i = 0; i < 3; i++) {

			// Reading from device from the InBuf pointer position
			const snd_pcm_sframes_t read_frames = snd_pcm_readi(captureDevice_.GetHandle(),
				buffer_.data() + writeInBufPosition_, framesPerPeriod_);

			// Processing the errors
			if(read_frames < 0) {
				ErrorHandler(static_cast<int>(read_frames), SND_PCM_STREAM_CAPTURE);
			}
			else {
				std::cout << "Read " << read_frames << " from " << captureDevice_.GetHw() <<
					". Free space: " << available_frames - read_frames << '/' << bufferSize_ << std::endl;

				// Move the pointer. If it goes beyond the buffer boundary, it will start again from 0
				writeInBufPosition_ = (writeInBufPosition_ +
					snd_pcm_frames_to_bytes(captureDevice_.GetHandle(), read_frames)) % bufferSize_;

				return read_frames;
			}
		}
	}
	else {
		std::cerr << "No free space in the buffer at this time" << std::endl;
		return 0;
	}
	throw std::runtime_error(std::string("Failed to read from device after 3 attempts"));
}

// Playback
snd_pcm_sframes_t Buffer::WriteToDevice()
{
	const unsigned int fill_bytes = (writeInBufPosition_ < writeInAlsaPosition_) ?
		(writeInBufPosition_ + bufferSize_ - writeInAlsaPosition_) :
		(writeInBufPosition_ - writeInAlsaPosition_);

	const size_t fill_frames = snd_pcm_bytes_to_frames(playbackDevice_.GetHandle(), fill_bytes);

	if(fill_frames > framesPerPeriod_) {
		for(int i = 0; i < 3; i++) {

			// Using writei instead of readi
			const snd_pcm_sframes_t written_frames = snd_pcm_writei(playbackDevice_.GetHandle(),
				buffer_.data() + writeInAlsaPosition_, framesPerPeriod_);

			if(written_frames < 0) {
				ErrorHandler(static_cast<int>(written_frames), SND_PCM_STREAM_PLAYBACK);
			}
			else {
				std::cout << "Device " << playbackDevice_.GetHw() << " received " << written_frames
					<< " from buffer" << std::endl;
				writeInAlsaPosition_ = (writeInAlsaPosition_ +
					snd_pcm_frames_to_bytes(playbackDevice_.GetHandle(), written_frames)) % bufferSize_;

				return written_frames;
			}
		}
	}
	else {
		std::cerr << "Not enough data to read from buffer. " << fill_frames << " frames available, "
			<< framesPerPeriod_ << " needed" << std::endl;
		return 0;
	}
	throw std::runtime_error(std::string("Failed to read from buffer after 3 attempts"));
}



void Buffer::ErrorHandler(const int error, const snd_pcm_stream_t mode) const
{
	const Device & temp = (mode == SND_PCM_STREAM_CAPTURE) ? captureDevice_ : playbackDevice_;

	// -EPIPE and -ESTRPIPE can be fixed by buffer recover
	if(error == -EPIPE) {

		if(mode == SND_PCM_STREAM_CAPTURE) {
			std::cerr << "An overrun occurred. Trying to recover" << std::endl;
		}
		else {
			std::cerr << "An underrun occurred. Trying to recover" << std::endl;
		}
		RecoverBuffer(temp, error);
	}

	else if(error == -ESTRPIPE) {
		std::cerr << "Stream was suspended. Trying to recover" << std::endl;
		RecoverBuffer(temp, error);
	}

	// -EAGAIN reports about temporary unavailability. Can be fixed by waiting
	else if(error == -EAGAIN) {
		if((snd_pcm_wait(temp.GetHandle(), 1000)) <= 0) {
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
	if(return_code < 0) {
		throw std::runtime_error(std::string("Failed to recover buffer. ")
			+ snd_strerror(return_code));
	}
	std::cerr << "The device was recovered" << std::endl;
}


