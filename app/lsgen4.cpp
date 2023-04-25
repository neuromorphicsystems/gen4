#include "../common/evk4.hpp"
#include "../common/psee413.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    for (const auto& serial : sepia::evk4::available_serials()) {
        std::cout << serial << " (EVK4)" << std::endl;
    }
    for (const auto& serial : sepia::psee413::available_serials()) {
        std::cout << serial << " (PSEE413)" << std::endl;
    }
}
