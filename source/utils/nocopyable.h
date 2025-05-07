#pragma once

namespace Nanite {
class NoCopyable {
protected:
    NoCopyable()  = default;
    ~NoCopyable() = default;

private:
    NoCopyable(NoCopyable&&) = delete;
};
} // namespace Nanite