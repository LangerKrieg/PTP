#include <iostream>
#include <csignal>
#include <atomic>
#include <unistd.h>
#include "Device.h"
#include "Buffer.h"

std::atomic<bool> keepRunning(true);

void signalHandler(int signum) {
    keepRunning = false;
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        Device captureDevice("hw:1,0", SND_PCM_STREAM_CAPTURE);
        Device playbackDevice("hw:1,0", SND_PCM_STREAM_PLAYBACK);

        captureDevice.SetHWParams(8000, SND_PCM_FORMAT_S16_LE, 1);
        playbackDevice.SetHWParams(8000, SND_PCM_FORMAT_S16_LE, 1);

        Buffer buffer(captureDevice, playbackDevice);

        std::cout << "Recording and playing... Press Ctrl+C to stop.\n";

        while (keepRunning) {
            buffer.ReadFromDevice();
            buffer.WriteToDevice();
            usleep(10000); // ~10ms pause
        }

        std::cout << "Stopped.\n";
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
