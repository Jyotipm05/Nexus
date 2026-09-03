import wavex;
#include <iostream>

int main() {
    auto p = wavex::protocol::http;
    const int val = static_cast<int>(p);
    std::cout << val << std::endl;
    return 0;
}
