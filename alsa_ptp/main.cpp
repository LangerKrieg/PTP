#include <iostream>
#include <csignal>
#include <atomic>
#include <alsa/asoundlib.h>

#include "Device.h"
#include "Buffer.h"
#include "udpstreamer.h"

std::atomic<bool> keepRunning(true);

void signal_handler(int signum) {
    keepRunning = false;
}

void configure_audio_mixer()
{
    system("amixer -c 1 set 'Left HP Mixer Left DAC2',0 unmute");
    system("amixer -c 1 set 'Left HP Mixer Left DAC1',0 unmute");
    system("amixer -c 1 set 'Left ADC Mixer INB1',0 unmute");
    system("amixer -c 1 set 'Left ADC Mixer INB2',0 unmute");
    system("amixer -c 1 set 'INB',0 57%");
    system("amixer -c 1 set 'Headphone',0 60%");

    system("amixer -c 0 set 'Left HP Mixer Left DAC2',0 unmute");
    system("amixer -c 0 set 'Left HP Mixer Left DAC1',0 unmute");
    system("amixer -c 0 set 'Left ADC Mixer INB1',0 unmute");
    system("amixer -c 0 set 'Left ADC Mixer INB2',0 unmute");
    system("amixer -c 0 set 'INB',0 57%");
    system("amixer -c 0 set 'Speaker',0 40%");
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::string hw_play_arg = argv[1];
    std::string hw_capture_arg = argv[2];

    std::string ip_arg = argv[3];
    std::string port_arg = argv[4];

    std::string remote_ip_arg = argv[5];
    std::string remote_port_arg = argv[6];

    if (std::stoi(argv[7]) == 1) {
        configure_audio_mixer();
    }

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
