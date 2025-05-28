#include <iostream>
#include <csignal>
#include <atomic>
#include <alsa/asoundlib.h>

#include "Device.h"
#include "Buffer.h"
#include "udpstreamer.h"

std::atomic<bool> keepRunning(true);

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received\n";
    keepRunning = false;
}

void configure_audio_mixer(int speakOrMicro)
{
    speakOrMicro == 1 ? system("alsactl -f phone_state.txt restore") : system("alsactl -f speaker_state.txt restore");
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::string hw_play_arg = argv[1];
    std::string hw_capture_arg = argv[2];

    std::string ip_arg = argv[3];
    std::string port_arg = argv[4];

    std::string remote_ip_arg = argv[5];
    std::string remote_port_arg = argv[6];

    configure_audio_mixer(int(hw_play_arg[3]));

    try {
        Device captureDevice(hw_capture_arg, SND_PCM_STREAM_CAPTURE);
        Device playbackDevice(hw_play_arg, SND_PCM_STREAM_PLAYBACK);

        UdpStreamer streamer(ip_arg, std::stoi(port_arg), remote_ip_arg,
            std::stoi(remote_port_arg));

        Buffer buffer(captureDevice, playbackDevice, streamer);

        while (keepRunning) {
            if (buffer.ReadFromDevice() > 0) {
                buffer.SendToUDP();
            }
            if(buffer.ReceiveFromUDP() > 0) {
                buffer.WriteToDevice();
            }
        }

    }
    catch(const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
