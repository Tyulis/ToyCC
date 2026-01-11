#pragma once

#include <ranges>
#include <vector>
#include <unordered_set>

namespace toycc {
    template <typename T>
    class CartesianProduct {
        public:
            CartesianProduct(const std::vector<std::vector<T>>& sets) : current_indices(sets.size()), sets(sets) {}

            void next() {
                size_t set_index = current_indices.size() - 1;
                current_indices[set_index] += 1;
                while (set_index > 0 && current_indices[set_index] >= sets[set_index].size()) {
                    current_indices[set_index] = 0;
                    set_index -= 1;
                    current_indices[set_index] += 1;
                }
            }

            bool done() const {
                return current_indices[0] >= sets[0].size();
            }

            std::vector<T> current() const {
                std::vector<T> result(current_indices.size());
                for (const auto& [set_index, value_index] : std::ranges::enumerate_view(current_indices))
                    result[set_index] = sets[set_index][value_index];
                return result;
            }

        private:
            std::vector<size_t> current_indices;
            std::vector<std::vector<T>> sets;
    };

    template <typename T>
    class Permutations {
        public:
            Permutations(const std::vector<T>& set, size_t length) : current_indices(length), set(set) {
                for (size_t item = 0; item < length; item++)
                    current_indices[item] = item;
            }

            void next() {
                do { next_product(); } while (!is_permutation());
            }

            bool done() const {
                return current_indices[0] >= set.size();
            }

            std::vector<T> current() const {
                std::vector<T> result(current_indices.size());
                for (const auto& [set_index, value_index] : std::ranges::enumerate_view(current_indices))
                    result[set_index] = set[value_index];
                return result;
            }

        private:
            std::vector<size_t> current_indices;
            std::vector<T> set;

            // FIXME : This is a naive algorithm, it's probably optimizable
            bool is_permutation() const {
                std::unordered_set<size_t> used_indices;
                for (size_t index : current_indices) {
                    if (used_indices.contains(index))
                        return false;
                    used_indices.insert(index);
                }
                return true;
            }

            void next_product() {
                size_t set_index = current_indices.size() - 1;
                current_indices[set_index] += 1;
                while (set_index > 0 && current_indices[set_index] >= set.size()) {
                    current_indices[set_index] = 0;
                    set_index -= 1;
                    current_indices[set_index] += 1;
                }
            }
    };
}
