#pragma once

#include <chrono>
#include <string>
#include <iostream>

struct DebugTimer {
    const std::chrono::high_resolution_clock::time_point startTime;
    mutable std::string debugName;
    mutable int divisor = 0;

    explicit DebugTimer(std::string name)
        : startTime(std::chrono::high_resolution_clock::now()), debugName(std::move(name)) {}
    explicit DebugTimer() : DebugTimer("Unnamed") {}

    void rename(const std::string& name) const {
        debugName = name;
    }

    void extraDivisor(const int amount) const {
        divisor = amount;
    }

    void end() const {
        const auto endTime = std::chrono::high_resolution_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        std::cout << "[DebugTimer (" << debugName << ")] Took " << duration << "ms";
        if (divisor != 0) {
            std::cout << " (Per: " << duration/divisor << " ms)";
        }
        std::cout << std::endl;
    }

    ~DebugTimer() { end(); }
};