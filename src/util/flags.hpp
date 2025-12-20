#pragma once

#include <utility>
#include <type_traits>

namespace toycc {
    template <typename T> requires(std::is_scoped_enum<T>::value)
    class Flags {
        private:
            T value = static_cast<T>(0);

        public:
            constexpr Flags() = default;
            constexpr Flags(T value) : value(std::move(value)) {}
            constexpr Flags(const Flags &) = default;
            constexpr Flags(Flags &&) = default;
            constexpr Flags &operator=(const Flags &) = default;
            constexpr Flags &operator=(Flags &&) = default;

            constexpr Flags& operator|= (T rhs) { value = static_cast<T>(std::to_underlying(value) | std::to_underlying(rhs));  return *this; }
            constexpr Flags& operator&= (T rhs) { value = static_cast<T>(std::to_underlying(value) | std::to_underlying(rhs));  return *this; }
            constexpr Flags& operator^= (T rhs) { value = static_cast<T>(std::to_underlying(value) ^ std::to_underlying(rhs));  return *this; }

            constexpr Flags& operator|= (Flags<T> rhs) {  return this->operator|=(rhs.value);  }
            constexpr Flags& operator&= (Flags<T> rhs) {  return this->operator&=(rhs.value);  }
            constexpr Flags& operator^= (Flags<T> rhs) {  return this->operator^=(rhs.value);  }

            constexpr Flags operator| (T rhs) const { Flags<T> result = *this;  result |= rhs;  return result; }
            constexpr Flags operator& (T rhs) const { Flags<T> result = *this;  result &= rhs;  return result; }
            constexpr Flags operator^ (T rhs) const { Flags<T> result = *this;  result ^= rhs;  return result; }

            constexpr Flags operator| (Flags<T> rhs) const { Flags<T> result = *this;  result |= rhs;  return result; }
            constexpr Flags operator& (Flags<T> rhs) const { Flags<T> result = *this;  result &= rhs;  return result; }
            constexpr Flags operator^ (Flags<T> rhs) const { Flags<T> result = *this;  result ^= rhs;  return result; }

            constexpr operator bool() const {
                return value != 0;
            }
    };
}
