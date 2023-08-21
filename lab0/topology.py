import networkx as nx

def load_topology(topo_file):
    """ Load a topogoly. The file given as input contains connection between
    switches. For each switch, one direct connection with a host is added."""

    switches_dic = {}

    # Initializing the graph of switches
    G = nx.Graph()

    with open(topo_file, 'r') as fd:
        for line in fd.readlines():
            if line[0] != '#' and len(line) > 1:
                linetab = line.rstrip('\n').split()

                from_switch = int(linetab[0])
                to_switch = int(linetab[1])

                # Add the nodes in the graph, with an attribute ports
                G.add_node(from_switch, ports={})
                G.add_node(to_switch, ports={})

                # Add the edge between the two nodes
                G.add_edge(from_switch, to_switch)

    # Set the port ID for edge of each switch
    for sfrom in sorted(G):
        for sto in sorted(G[sfrom]):
            G.node[sfrom]['ports'][str(sto)] = len(G.node[sfrom]['ports'])+1
        # Add a port for the host that will be connected to the switch
        G.node[sfrom]['ports']['host'] = len(G.node[sfrom]['ports'])+1

    return G

