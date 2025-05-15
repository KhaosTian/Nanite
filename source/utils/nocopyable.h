#pragma once

namespace Nanity {
class NoCopyable {
protected:
    NoCopyable()  = default;
    ~NoCopyable() = default;

private:
    NoCopyable(NoCopyable&&) = delete;
};
} // namespace Nanity