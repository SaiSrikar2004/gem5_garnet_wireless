/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "mem/ruby/network/garnet/RoutingUnit.hh"

#include "base/cast.hh"
#include "base/compiler.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/InputUnit.hh"
#include "mem/ruby/network/garnet/Router.hh"
#include "mem/ruby/slicc_interface/Message.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

RoutingUnit::RoutingUnit(Router *router)
{
    m_router = router;
    m_routing_table.clear();
    m_weight_table.clear();

}

void
RoutingUnit::addRoute(std::vector<NetDest>& routing_table_entry)
{
    if (routing_table_entry.size() > m_routing_table.size()) {
        m_routing_table.resize(routing_table_entry.size());
    }
    for (int v = 0; v < routing_table_entry.size(); v++) {
        m_routing_table[v].push_back(routing_table_entry[v]);
    }
}

void
RoutingUnit::addWeight(int link_weight)
{
    m_weight_table.push_back(link_weight);
}

bool
RoutingUnit::supportsVnet(int vnet, std::vector<int> sVnets)
{
    // If all vnets are supported, return true
    if (sVnets.size() == 0) {
        return true;
    }

    // Find the vnet in the vector, return true
    if (std::find(sVnets.begin(), sVnets.end(), vnet) != sVnets.end()) {
        return true;
    }

    // Not supported vnet
    return false;
}

/*
 * This is the default routing algorithm in garnet.
 * The routing table is populated during topology creation.
 * Routes can be biased via weight assignments in the topology file.
 * Correct weight assignments are critical to provide deadlock avoidance.
 */
int
RoutingUnit::lookupRoutingTable(int vnet, NetDest msg_destination)
{
    // First find all possible output link candidates
    // For ordered vnet, just choose the first
    // (to make sure different packets don't choose different routes)
    // For unordered vnet, randomly choose any of the links
    // To have a strict ordering between links, they should be given
    // different weights in the topology file

    int output_link = -1;
    int min_weight = INFINITE_;
    std::vector<int> output_link_candidates;
    int num_candidates = 0;

    // Identify the minimum weight among the candidate output links
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

        if (m_weight_table[link] <= min_weight)
            min_weight = m_weight_table[link];
        }
    }

    // Collect all candidate output links with this minimum weight
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

            if (m_weight_table[link] == min_weight) {
                num_candidates++;
                output_link_candidates.push_back(link);
            }
        }
    }

    if (output_link_candidates.size() == 0) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }

    // Randomly select any candidate output link
    int candidate = 0;
    if (!(m_router->get_net_ptr())->isVNetOrdered(vnet))
        candidate = rand() % num_candidates;

    output_link = output_link_candidates.at(candidate);
    return output_link;
}


void
RoutingUnit::addInDirection(PortDirection inport_dirn, int inport_idx)
{
    m_inports_dirn2idx[inport_dirn] = inport_idx;
    m_inports_idx2dirn[inport_idx]  = inport_dirn;
}

void
RoutingUnit::addOutDirection(PortDirection outport_dirn, int outport_idx)
{
    m_outports_dirn2idx[outport_dirn] = outport_idx;
    m_outports_idx2dirn[outport_idx]  = outport_dirn;
}


// outportCompute() is called by the InputUnit
// It calls the routing table by default.
// A template for adaptive topology-specific routing algorithm
// implementations using port directions rather than a static routing
// table is provided here.

std::pair<int,int>
RoutingUnit::outportCompute(RouteInfo route, int inport,
                            PortDirection inport_dirn)
{
    int outport = -1;
    int dest_hybrid_router=-1;

    if (route.dest_router == m_router->get_id()) {

        // Multiple NIs may be connected to this router,
        // all with output port direction = "Local"
        // Get exact outport id from table
        outport = lookupRoutingTable(route.vnet, route.net_dest);
        return std::make_pair(outport,dest_hybrid_router);
    }

    // Routing Algorithm set in GarnetNetwork.py
    // Can be over-ridden from command line using --routing-algorithm = 1
    RoutingAlgorithm routing_algorithm =
        (RoutingAlgorithm) m_router->get_net_ptr()->getRoutingAlgorithm();

    switch (routing_algorithm) {
        case TABLE_:  outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
        case XY_:     outport =
            outportComputeXY(route, inport, inport_dirn); break;
        // any custom algorithm
        case CUSTOM_: {std::tie(outport, dest_hybrid_router) = outportComputeCustom(route, inport, inport_dirn);
        break;
    }
        default: outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
    }

    assert(outport != -1);
    return std::make_pair(outport,dest_hybrid_router);
}

// XY routing implemented using port directions
// Only for reference purpose in a Mesh
// By default Garnet uses the routing table
int
RoutingUnit::outportComputeXY(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    [[maybe_unused]] int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    if (x_hops > 0) {
        if (x_dirn) {
            assert(inport_dirn == "Local" || inport_dirn == "West" || inport_dirn =="Wireless_In");
            outport_dirn = "East";
        } else {
            assert(inport_dirn == "Local" || inport_dirn == "East" || inport_dirn == "Wireless_In");
            outport_dirn = "West";
        }
    } else if (y_hops > 0) {
        if (y_dirn) {
            // "Local" or "South" or "West" or "East"
            assert(inport_dirn != "North");
            outport_dirn = "North";
        } else {
            // "Local" or "North" or "West" or "East"
            assert(inport_dirn != "South");
            outport_dirn = "South";
        }
    } else {
        // x_hops == 0 and y_hops == 0
        // this is not possible
        // already checked that in outportCompute() function
        panic("x_hops == y_hops == 0");
    }

    return m_outports_dirn2idx[outport_dirn];
}

// Template for implementing custom routing algorithm
// using port directions. (Example adaptive)
std::pair<int,int>
RoutingUnit::outportComputeCustom(RouteInfo route,
                                 int inport,
                                 PortDirection inport_dirn)
{
 PortDirection outport_dirn = "Unknown";

    int num_cols = m_router->get_net_ptr()->getNumCols();
    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;



    // std::unordered_map<int, std::vector<int>> hybrid_connections = {
    //     {18, {45, 50, 21}},
    //     {45, {18, 50, 21}},
    //     {50, {45, 18, 21}},
    //     {21, {45, 50, 18}}
    // };
    hybrid_connections=m_router->get_hybrid_connections();

    auto calculateHops = [&](int src_id, int dst_id) {
        int src_x = src_id % num_cols;
        int src_y = src_id / num_cols;
        int dst_x = dst_id % num_cols;
        int dst_y = dst_id / num_cols;
        return abs(dst_x - src_x) + abs(dst_y - src_y);
    };

    // Calculate hops for XY routing
    int xy_hops = calculateHops(my_id, dest_id);

    // Calculate hops for hybrid routing
    int hybrid_hops = std::numeric_limits<int>::max();
    int best_hybrid_router = -1;
    int dest_hybrid_router = -1;

    if (my_id == 18 || my_id == 45 || my_id == 50 || my_id == 21) {
        // If we're already at a hybrid router, calculate hops directly
        for (int connected_router : hybrid_connections[my_id]) {
            int hops = calculateHops(connected_router, dest_id) + 1; // +1 for the wireless hop
            if (hops < hybrid_hops) {
                hybrid_hops = hops;
                best_hybrid_router = connected_router;
                dest_hybrid_router = connected_router;
            }
        }
    } 
    else {
        // Calculate hops to reach the nearest hybrid router and then to the destination
        for (const auto& entry : hybrid_connections) {
            int hybrid_router = entry.first;
            int hops_to_hybrid = calculateHops(my_id, hybrid_router);
            
            for (int connected_router : entry.second) {
                int total_hops = hops_to_hybrid + 1 + calculateHops(connected_router, dest_id);
                if (total_hops < hybrid_hops) {
                    hybrid_hops = total_hops;
                    best_hybrid_router = hybrid_router;
                    dest_hybrid_router = connected_router;
                }
            }

        }
    }

    // std::cout<<" My Id = "<<my_id<<", Dst Id = "<<dest_id<<", XY hops = "<<xy_hops<<", Hybrid hops = "<<hybrid_hops<<std::endl;

    // Choose the routing method with fewer hops
    if (hybrid_hops < xy_hops) {
        // Use hybrid routing
        if (my_id == 18 || my_id == 45 || my_id == 50 || my_id == 21) {
                std::string dir="Wireless_Out"+std::to_string(dest_hybrid_router);
                std::cout<<"Out dir form routing unit "<<dir<<"/n";
                return std::make_pair(m_outports_dirn2idx[dir],dest_hybrid_router);
        } 
        else {
            // Route towards the nearest hybrid router
            int next_x = best_hybrid_router % num_cols;
            int next_y = best_hybrid_router / num_cols;
            bool x_dirn = (next_x >= my_x);
            bool y_dirn = (next_y >= my_y);
            if (next_x > my_x) outport_dirn="East";
            else if (next_x < my_x) outport_dirn="West";
            else if (next_y > my_y) outport_dirn="North";
            else if (next_y < my_y) outport_dirn="South";


    return std::make_pair(m_outports_dirn2idx[outport_dirn],dest_hybrid_router);
        }
    }
    return std::make_pair(outportComputeXY(route, inport, inport_dirn),-1);
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
