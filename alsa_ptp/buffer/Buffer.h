//
// Created by kuki on 5/8/25.
//

#ifndef BUFFER_H
#define BUFFER_H

#include <alsa/asoundlib.h>
#include <vector>

#include "../device/Device.h"


class Buffer {
private:
	std::vector<char> buffer_;
	const Device &captureDevice_;
	const Device &playbackDevice_;

	unsigned int frameSize_;
	unsigned int framesPerPeriod_;
	unsigned int bufferSize_;

	unsigned int writeInBufPosition_;
	unsigned int writeInAlsaPosition_;

	static int format_to_bytes(snd_pcm_format_t format);

	void ErrorHandler(int error, snd_pcm_stream_t mode) const;
	static void RecoverBuffer(const Device &device, int error);

public:
	explicit Buffer(const Device &capture_device, const Device &playback_device);

	snd_pcm_sframes_t ReadFromDevice();
	snd_pcm_sframes_t WriteToDevice();

};



#endif //BUFFER_H
