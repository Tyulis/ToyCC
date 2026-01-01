#pragma once

#include <queue>
#include <concepts>
#include <unordered_set>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>

namespace toycc {
    using namespace boost::multi_index;

    // struct Node{};

    template <typename Node>
    class Graph {
        public:
            struct Edge {
                std::shared_ptr<Node> entry;
                std::shared_ptr<Node> exit;

                bool operator== (const Edge& rhs) const {
                    return entry == rhs.entry && exit == rhs.exit;
                }
            };

            struct EdgeHash {
                size_t operator() (Edge edge) const {
                    return reinterpret_cast<uintptr_t>(edge.entry.get()) ^ (reinterpret_cast<uintptr_t>(edge.exit.get()) << 1);
                }
            };

            using NodeSet = std::unordered_set<std::shared_ptr<Node>>;
            using EdgeSet = std::unordered_set<Edge, EdgeHash>;

            static void noop(std::shared_ptr<Node>) {}

        private:
            struct entry_tag {};
            struct exit_tag {};

            using EdgeMap = multi_index_container<Edge,
                indexed_by<hashed_non_unique<tag<entry_tag>, member<Edge, std::shared_ptr<Node>, &Edge::entry>>,
                           hashed_non_unique<tag<exit_tag>,  member<Edge, std::shared_ptr<Node>, &Edge::exit>>>>;

            using EntryEdgeIndex = EdgeMap::template index<entry_tag>::type;
            using ExitEdgeIndex  = EdgeMap::template index<exit_tag>::type;

            NodeSet _nodes;
            EdgeMap _edges;

        public:
            // -------- Modify the graph structure
            // Add a new node without any edges
            inline std::shared_ptr<Node> add_node(std::shared_ptr<Node> node) {
                _nodes.insert(node);
                return node;
            }

            template <typename... Args>
            inline std::shared_ptr<Node> emplace_node(Args&&... args) {
                auto [it, done] = _nodes.emplace(std::make_shared<Node>(args...));
                return *it;
            }

            // Remove all edges that enter the given node (= all edges whose `exit` is that node)
            inline size_t remove_in_edges(std::shared_ptr<Node> node) {
                EdgeSet removed_edges;
                ExitEdgeIndex& exit_index = _edges.template get<exit_tag>();
                return exit_index.erase(node);
            }

            // Remove all edges that exit the given node (= all edges whose `entry` is that node)
            inline size_t remove_out_edges(std::shared_ptr<Node> node) {
                EdgeSet removed_edges;
                EntryEdgeIndex& entry_index = _edges.template get<entry_tag>();
                return entry_index.erase(node);
            }

            inline size_t disconnect_node(std::shared_ptr<Node> node) {
                return remove_in_edges(node) + remove_out_edges(node);
            }

            // Remove the node and all associated edges from the graph
            inline std::shared_ptr<Node> pop_node(std::shared_ptr<Node> node) {
                auto it = _nodes.find(node);
                if (it != _nodes.end()) {
                    disconnect_node(*it);
                    _nodes.erase(it);
                    return *it;
                } else {
                    return nullptr;
                }
            }

            // Add a new edge, and add its extremity nodes if they aren't already in the graph
            inline Edge add_edge(Edge edge) {
                if (!_nodes.contains(edge.entry))
                    _nodes.insert(edge.entry);
                if (!_nodes.contains(edge.exit))
                    _nodes.insert(edge.exit);

                _edges.insert(edge);
                return edge;
            }

            inline Edge add_edge(std::shared_ptr<Node> entry, std::shared_ptr<Node> exit) {
                return add_edge(Edge {entry, exit});
            }

            // Remove the edge if it exists
            inline Edge pop_edge(std::shared_ptr<Node> entry, std::shared_ptr<Node> exit) {
                Edge edge(entry, exit);
                auto it = _edges.find(edge);
                if (it != _edges.end())
                    _edges.erase(it);
                return edge;
            }


            // -------- Query the graph structure
            // Get the set of all nodes in the graph
            inline NodeSet nodes() const {
                return _nodes;
            }

            // Get the set of all edges in the graph
            inline EdgeSet edges() const {
                return _edges;
            }

            // Check whether the given node belongs to the graph (including when it has no edges)
            inline bool contains(std::shared_ptr<Node> node) const {
                return _nodes.contains(node);
            }

            // Get the set of nodes without outgoing edges
            inline NodeSet sources() const {
                const ExitEdgeIndex& exit_index = _edges.template get<exit_tag>();

                NodeSet result;
                for (std::shared_ptr<Node> node : _nodes)
                    if (exit_index.find(node) == exit_index.end())
                        result.insert(node);

                return result;
            }

            // Get the set of nodes without incoming edges
            inline NodeSet sinks() const {
                const EntryEdgeIndex& entry_index = _edges.template get<entry_tag>();

                NodeSet result;
                for (std::shared_ptr<Node> node : _nodes)
                    if (entry_index.find(node) == entry_index.end())
                        result.insert(node);

                return result;
            }

            // Get all edges that come into the requested node
            inline EdgeSet in_edges(std::shared_ptr<Node> node) const {
                const ExitEdgeIndex& exit_index = _edges.template get<exit_tag>();
                const auto [begin, end] = exit_index.equal_range(node);
                return EdgeSet {begin, end};
            }

            // Get all edges that come out of the requested node
            inline EdgeSet out_edges(std::shared_ptr<Node> node) const {
                const EntryEdgeIndex& entry_index = _edges.template get<entry_tag>();
                const auto [begin, end] = entry_index.equal_range(node);
                return EdgeSet {begin, end};
            }

            // -------- Set operations
            // Get the set of all nodes of this graph that are not in `initial`
            inline NodeSet complement(NodeSet initial) const {
                NodeSet result = _nodes;
                for (std::shared_ptr<Node> node : initial)
                    result.erase(node);
                return result;
            }

            // -------- Search algorithms
            // Build the set of all nodes reachable from the `entry_node`
            inline NodeSet reachable_from(std::shared_ptr<Node> entry_node) const {
                return breadth_first_search(entry_node, noop);
            }

            // Build the set of all nodes unreachable from the `exit_node`
            inline NodeSet unreachable_from(std::shared_ptr<Node> entry_node) const {
                return complement(reachable_from(entry_node));
            }

            // Build the set of all nodes from which the `exit` node can be reached
            inline NodeSet can_reach(std::shared_ptr<Node> exit_node) const {
                return transposed_breadth_first_search(exit_node, noop);
            }

            inline NodeSet cannot_reach(std::shared_ptr<Node> exit_node) const {
                return complement(can_reach(exit_node));
            }


            NodeSet breadth_first_search(std::shared_ptr<Node> start, std::invocable<std::shared_ptr<Node>> auto node_callback) const {
                const EntryEdgeIndex& entry_index = _edges.template get<entry_tag>();

                std::queue<std::shared_ptr<Node>> queue;
                queue.push(start);

                node_callback(start);
                NodeSet visited = {start};
                while (!queue.empty()) {
                    std::shared_ptr<Node> node = queue.front();
                    queue.pop();

                    for (auto [it, end] = entry_index.equal_range(node); it != end; it++) {
                        if (!visited.contains(it->exit)) {
                            node_callback(it->exit);
                            visited.insert(it->exit);
                            queue.push(it->exit);
                        }
                    }
                }

                return visited;
            }

            NodeSet transposed_breadth_first_search(std::shared_ptr<Node> start, std::invocable<std::shared_ptr<Node>> auto node_callback) const {
                const ExitEdgeIndex& exit_index = _edges.template get<exit_tag>();

                std::queue<std::shared_ptr<Node>> queue;
                queue.push(start);

                node_callback(start);
                NodeSet visited = {start};
                while (!queue.empty()) {
                    std::shared_ptr<Node> node = queue.front();
                    queue.pop();

                    for (auto [it, end] = exit_index.equal_range(node); it != end; it++) {
                        if (!visited.contains(it->entry)) {
                            node_callback(it->entry);
                            visited.insert(it->entry);
                            queue.push(it->entry);
                        }
                    }
                }

                return visited;
            }


            // -------- Filtering algorithms
            NodeSet keep_if(std::predicate<std::shared_ptr<Node>> auto predicate) {
                NodeSet removed_nodes;
                for (std::shared_ptr<Node> node : _nodes)
                    if (!predicate(node))
                        removed_nodes.insert(pop_node(node));

                return removed_nodes;
            }
    };
}
