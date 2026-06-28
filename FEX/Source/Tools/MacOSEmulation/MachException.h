#pragma once

namespace MacOSEmulation {

class MachException {
public:
    MachException() = default;
    ~MachException() = default;

    bool Init();
    void Shutdown();
};

} // namespace MacOSEmulation
