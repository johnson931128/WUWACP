
#include "wuwacp/cpu/hart.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>

int main() {
    wuwacp::Hart cpu;

    // Test 1: Reset initializes PC and registers
    cpu.reset(0x80000000ULL);

    if (cpu.pc() != 0x80000000ULL) {
        std::cerr << "[FAIL] Reset PC\n";
        return 1;
    }

    for (unsigned i = 0; i < 32; ++i) {
        if (cpu.read_register(i) != 0) {
            std::cerr << "[FAIL] Register reset\n";
            return 1;
        }
    }

    // Test 2: General-purpose register writes
    cpu.write_register(1, 123);
    cpu.write_register(31, 456);

    if (cpu.read_register(1) != 123 ||
        cpu.read_register(31) != 456) {
        std::cerr << "[FAIL] Register read/write\n";
        return 1;
    }

    // Test 3: x0 must always remain zero
    cpu.write_register(0, 999);

    if (cpu.read_register(0) != 0) {
        std::cerr << "[FAIL] x0 invariant\n";
        return 1;
    }

    // Test 4: Reset clears previous state
    cpu.reset(0x80200000ULL);

    if (cpu.pc() != 0x80200000ULL ||
        cpu.read_register(1) != 0 ||
        cpu.read_register(31) != 0) {
        std::cerr << "[FAIL] Reset state\n";
        return 1;
    }

    // Test 5: Invalid register index is rejected
    try {
        cpu.write_register(32, 1);
        std::cerr << "[FAIL] Invalid index\n";
        return 1;
    } catch (const std::out_of_range&) {
        // Expected
    }

    std::cout << "[PASS] CPU state tests\n";
    return 0;
}
