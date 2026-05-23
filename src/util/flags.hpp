#pragma once

#include <bit>
#include <utility>
#include <type_traits>

namespace toycc {
    template <typename T> requires(std::is_scoped_enum<T>::value)
    class FlagsetIterator {
        public:
            constexpr FlagsetIterator(std::underlying_type_t<T> flags) : flags(flags) {}

            constexpr T operator*() const {
                return static_cast<T>(first());
            }

            constexpr FlagsetIterator operator++(int) const {
                FlagsetIterator old = *this;
                operator++();
                return old;
            }

            constexpr FlagsetIterator& operator++() {
                flags ^= first();
                return *this;
            }

            constexpr bool operator== (const FlagsetIterator& other) {
                return flags == other.flags;
            }

            constexpr bool operator!= (const FlagsetIterator& other) {
                return flags != other.flags;
            }

        private:
            std::underlying_type_t<T> flags;

            constexpr std::underlying_type_t<T> first() const {
                return flags & ~(flags - 1);
            }
    };

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
            constexpr Flags& operator&= (T rhs) { value = static_cast<T>(std::to_underlying(value) & std::to_underlying(rhs));  return *this; }
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

            constexpr bool operator== (Flags<T> rhs) const { return std::to_underlying(value) == std::to_underlying(rhs.value); }
            constexpr bool operator== (T rhs)        const { return std::to_underlying(value) == std::to_underlying(rhs); }
            constexpr bool operator!= (Flags<T> rhs) const { return std::to_underlying(value) != std::to_underlying(rhs.value); }
            constexpr bool operator!= (T rhs)        const { return std::to_underlying(value) != std::to_underlying(rhs); }

            constexpr Flags& set(T rhs) {
                return *this |= rhs;
            }

            constexpr Flags& clear(T rhs) {
                value = static_cast<T>(std::to_underlying(value) & ~(std::to_underlying(rhs)));
                return *this;
            }

            constexpr Flags without(T rhs) const {
                return Flags {static_cast<T>(std::to_underlying(value) & ~(std::to_underlying(rhs)))};
            }

            constexpr bool includes(Flags<T> rhs) const {
                return (*this & rhs) == rhs;
            }

            constexpr bool includes(T rhs) const {
                return (*this & rhs);
            }

            constexpr T first() const {
                const std::underlying_type_t<T> intval = std::to_underlying(value);
                return static_cast<T> (intval & ~(intval - 1));
            }

            constexpr FlagsetIterator<T> begin() const {
                return {std::to_underlying(value)};
            }

            constexpr FlagsetIterator<T> end() const {
                return {0};
            }

            constexpr std::size_t count() const {
                return std::popcount(std::to_underlying(value));
            }

            constexpr bool empty() const {
                return std::to_underlying(value) == 0;
            }

            constexpr operator bool() const {
                return !empty();
            }
    };

    template <typename T> requires(std::is_scoped_enum<T>::value)
    constexpr Flags<T> operator| (T lhs, T rhs) {
        return Flags<T> {lhs} | rhs;
    }

    template <typename T> requires(std::is_scoped_enum<T>::value)
    constexpr Flags<T> operator| (T lhs, Flags<T> rhs) {
        return rhs | lhs;
    }

    template <typename T> requires(std::is_scoped_enum<T>::value)
    constexpr Flags<T> operator^ (T lhs, T rhs) {
        return Flags<T> {lhs} ^ rhs;
    }

    template <typename T> requires(std::is_scoped_enum<T>::value)
    constexpr Flags<T> operator^ (T lhs, Flags<T> rhs) {
        return rhs ^ lhs;
    }

    template <typename T> requires(std::is_scoped_enum<T>::value)
    constexpr Flags<T> operator& (T lhs, T rhs) {
        return Flags<T> {lhs} & rhs;
    }

    template <typename T> requires(std::is_scoped_enum<T>::value)
    constexpr Flags<T> operator& (T lhs, Flags<T> rhs) {
        return rhs & lhs;
    }
}
