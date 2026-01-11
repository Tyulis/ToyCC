import itertools
import numpy as np

graph = np.asarray([[1, 2, -1, 0, 0, 0, 0, 0, 0],
                    [0, 0, 1, 0, 0, -1, 0, 0, 0],
                    [0, 0, 1, 2, -1, 0, 0, 0, 0],
                    [0, 0, 1, 2, 0, 0, -1, 0, 0],
                    [0, 0, 0, 0, 1, 0, 2, -1, 0],
                    [0, 0, 0, 0, 0, 1, 0, 2, -1],
                    [0, 0, 0, 0, 0, 0, 0, 0,  1]])

graph_statements = ["ADD", "MINUS", "ADD", "ADD", "ADD", "EQ", "JUMP_IF_TRUE"]

add_subgraph = np.asarray([[1, 2, -1,  0],
                           [1, 2,  0, -1],
                           [0, 0,  1,  2]])
add_statements = ["ADD", "ADD", "ADD"]

jump_subgraph = np.asarray([[-1],
                            [ 1]])
jump_statements = ["EQ", "JUMP_IF_TRUE"]

def match_combination(combination, graph, subgraph):
    columns = np.nonzero(np.count_nonzero(graph[combination, :], axis=0) >= 2)[0]

    if len(columns) < subgraph.shape[1]:
        return None

    for permutation in itertools.permutations(columns):
        match_attempt = graph[combination, :][:, permutation]

        matches = True
        for row in range(match_attempt.shape[0]):
            for column in range(match_attempt.shape[1]):
                if subgraph[row, column] != 0 and subgraph[row, column] != match_attempt[row, column]:
                    matches = False
                    break
            if not matches:
                break
        if matches:
            return combination

    return None


def match(graph, graph_statements, subgraph, subgraph_statements):
    statement_matches = []
    for statement in subgraph_statements:
        available = []
        for i, graph_statement in enumerate(graph_statements):
            if graph_statement == statement:
                available.append(i)
        if len(available) == 0:
            return []
        statement_matches.append(available)

    matches = []
    for combination in itertools.product(*statement_matches):
        match = match_combination(combination, graph, subgraph)
        if match is not None:
            matches.append(match)

    return matches


add_matches = match(graph, graph_statements, add_subgraph, add_statements)
jump_matches = match(graph, graph_statements, jump_subgraph, jump_statements)

print(add_matches)
print(jump_matches)
