#pragma once

#include <unordered_set>

namespace toycc {
    template <typename T>
    inline std::unordered_set<T> unordered_set_intersection(const std::unordered_set<T>& left, const std::unordered_set<T>& right) {
        std::unordered_set<T> result;
        for (const T& element : left)
            if (right.contains(element))
                result.insert(element);
        return result;
    }

    template <typename T>
    inline std::unordered_set<T> unordered_set_union(const std::unordered_set<T>& left, const std::unordered_set<T>& right) {
        std::unordered_set<T> result = left;
        for (const T& element : right)
            result.insert(element);
        return result;
    }

    template <typename T>
    inline std::unordered_set<T> unordered_set_difference(const std::unordered_set<T>& left, const std::unordered_set<T>& right) {
        std::unordered_set<T> result;
        for (const T& element : left)
            if (!right.contains(element))
                result.insert(element);
        return result;
    }
}
