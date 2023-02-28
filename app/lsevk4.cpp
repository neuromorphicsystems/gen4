#include "../common/evk4.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    for (const auto& serial : sepia::evk4::available_serials()) {
        std::cout << serial << std::endl;
    }
}
