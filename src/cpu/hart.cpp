
#include "wuwacp/cpu/hart.hpp"

namespace wuwacp {

void Hart::reset(std::uint64_t reset_pc) noexcept {
    registers_.fill(0);
    pc_ = reset_pc;
}

std::uint64_t Hart::read_register(
    unsigned index
) const {
    return registers_.at(index);
}

void Hart::write_register(
    unsigned index,
    std::uint64_t value
) {
    if (index == 0) {
        return;
    }

    registers_.at(index) = value;
}

std::uint64_t Hart::pc() const noexcept {
    return pc_;
}

} // namespace wuwacp
