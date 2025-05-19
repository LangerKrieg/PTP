#include <iostream>
#include <csignal>
#include <atomic>
#include <alsa/asoundlib.h>
#include <map>

#include "Device.h"
#include "Buffer.h"
#include "udpstreamer.h"

std::atomic<bool> keepRunning(true);

void signalHandler(int signum) {
    keepRunning = false;
}

int parsePort(std::string str)
{
    int port = std::stoi(str);
    if (port < 0 || port > 65535) {
        throw std::invalid_argument("Некорректное значение для порта");
    }
    return port;
}

std::string parseIp(std::string str)
{
    int dots = 0;

    for (unsigned int i = 0; i < str.length(); i++) {
        if (!isdigit(str[i]) && str[i] != '.') {
            throw std::invalid_argument("Неверный формат IP-адреса, не только цифры и '.");
        } else if (str[i] == '.' && i != 0 && i != str.length() - 1 && str[i + 1] != '.') {
            dots++;
        }

        if (dots > 3) {
            throw std::invalid_argument("Неверный формат IP-адреса, в IP не должно быть больше 3-х точек");
        }
    }

    if (dots < 3) {
        throw std::invalid_argument("Неверный формат IP-адреса, в IP не должно быть меньше 3-х точек");
    }
    return str;
}

std::string parseMicSpeak(std::string str)
{
    if (str.substr(0, 3) != "hw:" || str[4] != ',') {
        throw std::invalid_argument("Некорректное название устройства (динамик или микрофон)");
    }
    return str;
}

void configureAudioMixer()
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

int main(int argc, char* argv[]) {

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::string info = "\nИспользование: "
                       "\n -i1 <ip_sender>\n "
                       "-p1 <port_sender>\n "
                       "-i2 <ip_receiver>\n "
                       "-p2 <port_receiver>\n "
                       "-m <micro (hw:1,0)>\n "
                       "-s <speaker (hw:1,0)>\n ";

    std::map<std::string, std::string> args = {
        {"-i1", "192.168.0.252"},
        {"-i2", "172.31.3.33"},
        {"-p1", "5003"},
        {"-p2", "5003"},
        {"-f", "16"},
        {"-d", "s"},
        {"-m", "hw:1,0"},
        {"-s", "hw:1,0"},
        {"-l", info}
    };

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.substr(0, 1) == "-") {
            std::string key = arg;

            // Ключи, которые не требуют значений (например, -l)
            if (key == "-l") {
                std::cout << args.at("-l") << std::endl;
                return 0;
            }

            // Проверка, что для ключа есть значение
            if (i + 1 >= argc || argv[i + 1][0] == '-') {
                std::cerr << "Отсутствует значение для параметра: " << key << "\n";
                return 1;
            }

            std::string value = argv[i + 1];
            if (args.count(key)) {
                args[key] = value;
                i++;
            } else {
                std::cerr << "Неизвестный параметр: " << key << "\n";
                return 1;
            }
        }
    }

    try {

        if (argc > 17) {
            throw std::runtime_error("\nИспользование: " + std::string(argv[0]) +
                    "\n -i1 <ip_sender>\n "
                    "-p1 <port_sender>\n "
                    "-i2 <ip_receiver>\n "
                    "-p2 <port_receiver>\n "
                    "-m <micro (hw:1,0)>\n "
                    "-s <speaker (hw:1,0)>\n ");
        }

        std::string ip1 = parseIp(args["-i1"]);
        int port1 = parsePort(args["-p1"]);
        std::string ip2 = parseIp(args["-i2"]);
        int port2 = parsePort(args["-p2"]);
        std::string micro = parseMicSpeak(args["-m"]);
        std::string speak = parseMicSpeak(args["-s"]);

        configureAudioMixer();

        Device captureDevice(micro, SND_PCM_FORMAT_S16_LE, SND_PCM_STREAM_CAPTURE);
        Device playbackDevice(speak, SND_PCM_FORMAT_S16_LE, SND_PCM_STREAM_PLAYBACK);

        // Инициализация UDP
        UdpStreamer udpStreamer;
        if (!udpStreamer.initialize(ip1, port1, ip2, port2)) {
            std::cerr << "Failed to initialize UDP streamer" << std::endl;
            return 1;
        }

        Buffer buffer(captureDevice, playbackDevice);

        while (keepRunning) {
            // Чтение с микрофона и отправка по UDP
            if (buffer.ReadFromDevice() > 0) {
                udpStreamer.sendBuffer(buffer);
            }

            // Прием по UDP и воспроизведение
            if (udpStreamer.receiveToBuffer(buffer)) {
                buffer.WriteToDevice();
            }

            usleep(1000); // Небольшая пауза
        }
    }
    catch(const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
