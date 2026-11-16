
#pragma once

#include <array>
#include <cstdint>

namespace wuwacp {

class Hart {
public:
    void reset(std::uint64_t reset_pc) noexcept;

    [[nodiscard]]
    std::uint64_t read_register(unsigned index) const;

    void write_register(
        unsigned index,
        std::uint64_t value
    );

    [[nodiscard]]
    std::uint64_t pc() const noexcept;

private:
    std::array<std::uint64_t, 32> registers_{};
    std::uint64_t pc_ = 0;
};

} // namespace wuwacp
