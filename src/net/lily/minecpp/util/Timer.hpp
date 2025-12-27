//
// Created by lily on 12/25/25.
//

#ifndef MINECRAFTCLIENT_TIMER_H
#define MINECRAFTCLIENT_TIMER_H

#include <chrono>

static constexpr float TICK_SPEED = 0.05f;

using Time = std::chrono::time_point<std::chrono::system_clock>;

struct Timer {

    float timer = 1.0f;
    float partialTicks = 0.0f;
    Time lastTick;

    void calculatePartialTicks(const Time now) {
        const std::chrono::duration<float> elapsed = now - lastTick;
        partialTicks = elapsed.count() / TICK_SPEED;
    }

};

#endif //MINECRAFTCLIENT_TIMER_H