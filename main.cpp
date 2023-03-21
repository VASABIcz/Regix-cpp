#include <iostream>
#include <chrono>
#include "Regix.h"

int main() {
    auto reg = regix::constructRegix("uwu");

    std::cout << "finished parsing" << std::endl;

    // reg->print();

    auto res = measureTime([&]() {
        bool volatile x;
        for (auto i = 0; i < 1'000'000; i++) {
            x = reg->doesMatch("uwu");
        }
    });
    std::cout << res << std::endl;

    return 0;
}
