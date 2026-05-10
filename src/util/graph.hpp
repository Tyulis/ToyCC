#pragma once

#include <queue>
#include <stack>
#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include "util/sets.hpp"

namespace toycc {
    using namespace boost::multi_index;

    struct InvalidStructure : public std::runtime_error {
        InvalidStructure(std::string message) : std::runtime_error(message) {}
    };

    // struct Node{};

    template <typename Node, typename EdgeAttribute = std::monostate> requires(std::is_default_constructible_v<EdgeAttribute>)
    class Graph {
        public:
            struct Edge {
                std::shared_ptr<Node> entry;
                std::shared_ptr<Node> exit;
                EdgeAttribute attr;

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
            struct edge_tag {};

            using EdgeMap = multi_index_container<Edge,
                indexed_by<hashed_non_unique<tag<entry_tag>, member<Edge, std::shared_ptr<Node>, &Edge::entry>>,
                           hashed_non_unique<tag<exit_tag>,  member<Edge, std::shared_ptr<Node>, &Edge::exit>>,
                           hashed_unique    <tag<edge_tag>,  identity<Edge>, EdgeHash>>>;

            using EntryEdgeIndex = EdgeMap::template index<entry_tag>::type;
            using ExitEdgeIndex  = EdgeMap::template index<exit_tag>::type;
            using EdgeIndex      = EdgeMap::template index<edge_tag>::type;

            NodeSet _nodes;
            EdgeMap _edges;

            EntryEdgeIndex& entry_edge_index() { return _edges.template get<entry_tag>(); }
            const EntryEdgeIndex& entry_edge_index() const { return _edges.template get<entry_tag>(); }
            ExitEdgeIndex& exit_edge_index() { return _edges.template get<exit_tag>(); }
            const ExitEdgeIndex& exit_edge_index() const { return _edges.template get<exit_tag>(); }
            EdgeIndex& edge_index() { return _edges.template get<edge_tag>(); }
            const EdgeIndex& edge_index() const { return _edges.template get<edge_tag>(); }

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
                return exit_edge_index().erase(node);
            }

            // Remove all edges that exit the given node (= all edges whose `entry` is that node)
            inline size_t remove_out_edges(std::shared_ptr<Node> node) {
                EdgeSet removed_edges;
                return entry_edge_index().erase(node);
            }

            inline size_t disconnect_node(std::shared_ptr<Node> node) {
                return remove_in_edges(node) + remove_out_edges(node);
            }

            // Remove the node and all associated edges from the graph
            inline std::shared_ptr<Node> pop_node(std::shared_ptr<Node> node) {
                auto it = _nodes.find(node);
                if (it != _nodes.end()) {
                    node = *it;
                    disconnect_node(node);
                    _nodes.erase(it);
                    return node;
                } else {
                    return nullptr;
                }
            }

            // Add a new edge, and add its extremity nodes if they aren't already in the graph.
            // If there is already an edge with the same `entry` and `exit`, replace it
            inline Edge add_edge(Edge edge) {
                if (!_nodes.contains(edge.entry))
                    _nodes.insert(edge.entry);
                if (!_nodes.contains(edge.exit))
                    _nodes.insert(edge.exit);

                EdgeIndex& index = edge_index();
                auto it = index.find(edge);
                if (it == index.end())  index.insert(edge);
                else                    index.replace(it, edge);
                return edge;
            }

            // Add a new edge. If there is already an edge from `entry` to `exit`, replace it with an edge with the given `attribute`
            inline Edge add_edge(std::shared_ptr<Node> entry, std::shared_ptr<Node> exit, EdgeAttribute attribute = {}) {
                return add_edge(Edge {entry, exit, attribute});
            }

            // Remove the edge if it exists
            inline Edge pop_edge(const Edge& edge) {
                EdgeIndex& index = edge_index();
                auto it = index.find(edge);
                if (it != index.end())
                    index.erase(it);
                return edge;
            }

            // Remove the edge if it exists
            inline Edge pop_edge(std::shared_ptr<Node> entry, std::shared_ptr<Node> exit) {
                return pop_edge(Edge {entry, exit, {}});
            }


            // -------- Query the graph structure
            // Check whether the graph contains any node
            inline bool empty() const {
                return _nodes.empty();
            }

            // Check whether the given node belongs to the graph (including when it has no edges)
            inline bool contains(std::shared_ptr<Node> node) const {
                return _nodes.contains(node);
            }

            // Check whether the graph contains an edge from the given `entry` node to the `exit` node
            inline bool contains(std::shared_ptr<Node> entry, std::shared_ptr<Node> exit) const {
                return edge_index().contains(Edge {entry, exit, {}});
            }

            // Check whether the graph contains an edge corresponding to the given edge (regardless of its custom attribute)
            inline bool contains(Edge edge) const {
                return edge_index().contains(edge);
            }

            // Return the node if it belongs to the graph, or nullptr otherwise
            inline std::shared_ptr<Node> find_node(std::shared_ptr<Node> node) const {
                auto it = _nodes.find(node);
                if (it == _nodes.end())  return nullptr;
                else                     return *it;
            }

            // Return any graph node that is equal to the requested `query`, or nullptr otherwise
            template <typename QueryType> requires requires(const Node& node, const QueryType& query) { {node == query} -> std::convertible_to<bool>; }
            inline std::shared_ptr<Node> find_node(const QueryType& query) const {
                for (std::shared_ptr<Node> node : _nodes)
                    if (*node == query)
                        return node;
                return nullptr;
            }

            // Return all graph nodes that evaluate equal to the requested `query`
            template <typename QueryType> requires requires(const Node& node, const QueryType& query) { {node == query} -> std::convertible_to<bool>; }
            inline NodeSet find_nodes(const QueryType& query) const {
                NodeSet result;
                for (std::shared_ptr<Node> node : _nodes)
                    if (*node == query)
                        result.insert(node);
                return result;
            }

            // Return the node if it is a source node of the graph, or nullptr otherwise
            inline std::shared_ptr<Node> find_source_node(std::shared_ptr<Node> query) const {
                const ExitEdgeIndex& exit_index = exit_edge_index();
                std::shared_ptr<Node> node = find_node(query);
                if (node.get() == nullptr)
                    return nullptr;

                if (exit_index.find(node) == exit_index.end())  return node;
                else                                            return nullptr;
            }

            // Return the node if it is a source node of the graph, or nullptr otherwise
            inline std::shared_ptr<Node> find_source_node(const Node& query) const {
                const ExitEdgeIndex& exit_index = exit_edge_index();

                for (std::shared_ptr<Node> node : _nodes)
                    if (*node == query && exit_index.find(node) == exit_index.end())
                        return node;
                return nullptr;
            }

            // Return the node if it is a sink node of the graph, or nullptr otherwise
            inline std::shared_ptr<Node> find_sink_node(std::shared_ptr<Node> query) const {
                const EntryEdgeIndex& entry_index = entry_edge_index();
                std::shared_ptr<Node> node = find_node(query);
                if (node.get() == nullptr)
                    return nullptr;

                if (entry_index.find(node) == entry_index.end())  return node;
                else                                              return nullptr;
            }

            // Return the node if it is a sink node of the graph, or nullptr otherwise
            inline std::shared_ptr<Node> find_sink_node(const Node& query) const {
                const EntryEdgeIndex& entry_index = entry_edge_index();

                for (std::shared_ptr<Node> node : _nodes)
                    if (*node == query && entry_index.find(node) == entry_index.end())
                        return node;
                return nullptr;
            }

            // Return the edge with the same entry and exit as `edge`, if it exists
            inline std::optional<Edge> find_edge(const Edge& edge) const {
                const EdgeIndex& index = edge_index();
                auto it = index.find(edge);
                if (it == index.end())  return {};
                else                    return *it;
            }

            // Return the edge from `entry` to `exit`, if it exists
            inline std::optional<Edge> find_edge(std::shared_ptr<Node> entry, std::shared_ptr<Node> exit) const {
                return find_edge(Edge {entry, exit, {}});
            }

            // Get the number of nodes in the graph
            inline size_t nof_nodes() const {
                return _nodes.size();
            }

            // Get the set of all nodes in the graph
            inline NodeSet nodes() const {
                return _nodes;
            }

            // Get the set of all edges in the graph
            inline EdgeSet edges() const {
                return {_edges.begin(), _edges.end()};
            }

            // Get all edges connected to the given node
            inline EdgeSet connected_edges(std::shared_ptr<Node> node) const {
                const EntryEdgeIndex& entry_index = entry_edge_index();
                const ExitEdgeIndex&  exit_index  = exit_edge_index();

                auto [entry_begin, entry_end] = entry_index.equal_range(node);
                auto [exit_begin,  exit_end]  = exit_index.equal_range(node);

                EdgeSet result;
                result.insert(entry_begin, entry_end);
                result.insert(exit_begin, exit_end);
                return result;
            }

            // Get the set of nodes without outgoing edges
            inline NodeSet sources() const {
                const ExitEdgeIndex& exit_index = exit_edge_index();

                NodeSet result;
                for (std::shared_ptr<Node> node : _nodes)
                    if (exit_index.find(node) == exit_index.end())
                        result.insert(node);

                return result;
            }

            // Get the set of nodes without incoming edges
            inline NodeSet sinks() const {
                const EntryEdgeIndex& entry_index = entry_edge_index();

                NodeSet result;
                for (std::shared_ptr<Node> node : _nodes)
                    if (entry_index.find(node) == entry_index.end())
                        result.insert(node);

                return result;
            }

            // Check whether the node has any outgoing edge
            inline bool is_source(std::shared_ptr<Node> node) const {
                const ExitEdgeIndex& exit_index = exit_edge_index();
                return contains(node) && exit_index.find(node) == exit_index.end();
            }

            // Check whether the node has any incoming edge
            inline bool is_sink(std::shared_ptr<Node> node) const {
                const EntryEdgeIndex& entry_index = entry_edge_index();
                return contains(node) && entry_index.find(node) == entry_index.end();
            }

            // Check whether the node is connected to any edge
            inline bool is_connected(std::shared_ptr<Node> node) const {
                return !(is_source(node) && is_sink(node));
            }

            // Get all edges that come into the requested node
            inline EdgeSet in_edges(std::shared_ptr<Node> node) const {
                const ExitEdgeIndex& exit_index = exit_edge_index();
                const auto [begin, end] = exit_index.equal_range(node);
                return EdgeSet {begin, end};
            }

            // Get all nodes with edges going to the given `node`
            inline NodeSet previous_nodes(std::shared_ptr<Node> node) const {
                NodeSet result;
                const ExitEdgeIndex& exit_index = exit_edge_index();
                for (auto [edge, end] = exit_index.equal_range(node); edge != end; edge++)
                    result.insert(edge->entry);
                return result;
            }

            // Get all edges that come out of the requested node
            inline EdgeSet out_edges(std::shared_ptr<Node> node) const {
                const EntryEdgeIndex& entry_index = entry_edge_index();
                const auto [begin, end] = entry_index.equal_range(node);
                return EdgeSet {begin, end};
            }

            // Get all nodes with edges coming from the given `node`
            inline NodeSet next_nodes(std::shared_ptr<Node> node) const {
                NodeSet result;
                const EntryEdgeIndex& entry_index = entry_edge_index();
                for (auto [edge, end] = entry_index.equal_range(node); edge != end; edge++)
                    result.insert(edge->exit);
                return result;
            }

            // Get all nodes directly connected to the given `node`
            inline NodeSet connected_nodes(std::shared_ptr<Node> node) const {
                NodeSet result;

                const EntryEdgeIndex& entry_index = entry_edge_index();
                for (auto [edge, end] = entry_index.equal_range(node); edge != end; edge++)
                    result.insert(edge->exit);

                const ExitEdgeIndex& exit_index = exit_edge_index();
                for (auto [edge, end] = exit_index.equal_range(node); edge != end; edge++)
                    result.insert(edge->entry);

                return result;
            }

            // -------- Set operations
            // Get the set of all nodes of this graph that are not in `initial`
            inline NodeSet complement(NodeSet initial) const {
                return unordered_set_difference(_nodes, initial);
            }

            // -------- Search algorithms
            // Build the set of all nodes reachable from the `entry_node`, including the `entry_node` itself
            inline NodeSet reachable_from(std::shared_ptr<Node> entry_node) const {
                return breadth_first_search(entry_node, noop);
            }

            // Build the set of all nodes unreachable from the `exit_node`
            inline NodeSet unreachable_from(std::shared_ptr<Node> entry_node) const {
                return complement(reachable_from(entry_node));
            }

            // Build the set of all nodes from which the `exit` node can be reached, including the `entry_node` itself
            inline NodeSet can_reach(std::shared_ptr<Node> exit_node) const {
                return transposed_breadth_first_search(exit_node, noop);
            }

            inline NodeSet cannot_reach(std::shared_ptr<Node> exit_node) const {
                return complement(can_reach(exit_node));
            }

            NodeSet breadth_first_search(const NodeSet& start, std::invocable<std::shared_ptr<Node>> auto node_callback) const {
                const EntryEdgeIndex& entry_index = entry_edge_index();

                for (std::shared_ptr<Node> node : start)
                    node_callback(node);

                NodeSet visited(start);
                std::queue<std::shared_ptr<Node>> queue(start.cbegin(), start.cend());
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

            NodeSet breadth_first_search(std::shared_ptr<Node> start, std::invocable<std::shared_ptr<Node>> auto node_callback) const {
                std::unordered_set<std::shared_ptr<Node>> start_set = {start};
                return breadth_first_search(start_set, node_callback);
            }

            NodeSet breadth_first_search(std::invocable<std::shared_ptr<Node>> auto node_callback) const {
                return breadth_first_search(sources(), node_callback);
            }

            NodeSet breadth_first_search(const NodeSet& start) const {
                return breadth_first_search(start, noop);
            }

            NodeSet breadth_first_search(std::shared_ptr<Node> start) const {
                return breadth_first_search(start, noop);
            }

            NodeSet transposed_breadth_first_search(const NodeSet& start, std::invocable<std::shared_ptr<Node>> auto node_callback) const {
                const ExitEdgeIndex& exit_index = exit_edge_index();

                for (std::shared_ptr<Node> node : start)
                    node_callback(node);

                NodeSet visited(start);
                std::queue<std::shared_ptr<Node>> queue(start.cbegin(), start.cend());
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

            NodeSet transposed_breadth_first_search(std::shared_ptr<Node> start, std::invocable<std::shared_ptr<Node>> auto node_callback) const {
                std::unordered_set<std::shared_ptr<Node>> start_set = {start};
                return transposed_breadth_first_search(start_set, node_callback);
            }

            NodeSet transposed_breadth_first_search(std::invocable<std::shared_ptr<Node>> auto node_callback) const {
                return transposed_breadth_first_search(sinks(), node_callback);
            }

            NodeSet transposed_breadth_first_search(const std::unordered_set<std::shared_ptr<Node>>& start) const {
                return transposed_breadth_first_search(start, noop);
            }

            NodeSet transposed_breadth_first_search(std::shared_ptr<Node> start) const {
                return transposed_breadth_first_search(start, noop);
            }

            NodeSet depth_first_search(const NodeSet& start, std::invocable<std::shared_ptr<Node>> auto node_callback) const {
                const EntryEdgeIndex& entry_index = entry_edge_index();

                for (std::shared_ptr<Node> node : start)
                    node_callback(node);

                NodeSet visited(start);
                std::stack<std::shared_ptr<Node>> stack(start.cbegin(), start.cend());
                while (!stack.empty()) {
                    std::shared_ptr<Node> node = stack.top();
                    stack.pop();

                    for (auto [it, end] = entry_index.equal_range(node); it != end; it++) {
                        if (!visited.contains(it->exit)) {
                            node_callback(it->exit);
                            visited.insert(it->exit);
                            stack.push(it->exit);
                        }
                    }
                }

                return visited;
            }

            NodeSet depth_first_search(std::shared_ptr<Node> start, std::invocable<std::shared_ptr<Node>> auto node_callback) const {
                std::unordered_set<std::shared_ptr<Node>> start_set = {start};
                return depth_first_search(start_set, node_callback);
            }

            NodeSet depth_first_search(std::invocable<std::shared_ptr<Node>> auto node_callback) const {
                return breadth_first_search(sources(), node_callback);
            }

            NodeSet depth_first_search(const NodeSet& start) const {
                return depth_first_search(start, noop);
            }

            NodeSet depth_first_search(std::shared_ptr<Node> start) const {
                return depth_first_search(start, noop);
            }


            // -------- Filtering algorithms
            NodeSet keep_if(std::predicate<std::shared_ptr<Node>> auto predicate) {
                NodeSet removed_nodes;
                for (std::shared_ptr<Node> node : _nodes)
                    if (!predicate(node))
                        removed_nodes.insert(pop_node(node));

                return removed_nodes;
            }

            // -------- Structure algorithms
            // Get all nodes of a directed acyclic graph in topological order. Throws `InvalidStructure` if cycles are found
            std::vector<std::shared_ptr<Node>> topological_sort() const {
                const EntryEdgeIndex& entry_index = entry_edge_index();

                std::vector<std::shared_ptr<Node>> order(_nodes.size());
                auto order_position = order.rbegin();

                // Three states here : ongoing[false] -> not visited at all
                //                     ongoing[true]  -> currently being visited
                //                     not in ongoing -> already visited
                std::unordered_map<std::shared_ptr<Node>, bool> ongoing;
                for (std::shared_ptr<Node> node : _nodes)
                    ongoing[node] = false;

                std::function<void(std::shared_ptr<Node>)> visit = [&](std::shared_ptr<Node> node) {
                    auto status = ongoing.find(node);
                    if (status == ongoing.end())
                        return;
                    else if (status->second == true)
                        throw InvalidStructure {"Found a cycle, topological sorting is impossible"};

                    status->second = true;
                    for (auto [it, end] = entry_index.equal_range(node); it != end; it++)
                        visit(it->exit);

                    ongoing.erase(status);
                    *order_position++ = node;
                };

                while (!ongoing.empty())
                    visit(ongoing.cbegin()->first);

                return order;
            }
    };
}
