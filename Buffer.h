#pragma once
#include "Device.h"
#include <vector>

class Buffer {
public:
    Buffer(Device& input, Device& output);
    void ReadFromDevice();
    void WriteToDevice();

private:
    Device& inputDevice;
    Device& outputDevice;
    std::vector<int16_t> data;
};
