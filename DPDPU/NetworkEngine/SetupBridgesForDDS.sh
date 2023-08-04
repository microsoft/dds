ovs-vsctl del-br ovsbr1
ovs-vsctl add-br sf_bridge1
ovs-vsctl add-port sf_bridge1 p0
ovs-vsctl add-port sf_bridge1 en3f0pf0sf1
ovs-vsctl add-br sf_bridge2
ovs-vsctl add-port sf_bridge2 pf0hpf
ovs-vsctl add-port sf_bridge2 en3f0pf0sf2
ovs-vsctl show
