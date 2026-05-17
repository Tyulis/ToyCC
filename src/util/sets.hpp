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

    // Check whether `left` is included in `right` (i.e all elements of `left` are found in `right`)
    template <typename T>
    inline bool unordered_set_included(const std::unordered_set<T>& left, const std::unordered_set<T>& right) {
        for (const T& element : left)
            if (!right.contains(element))
                return false;
        return true;
    }
}
