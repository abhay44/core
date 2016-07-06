//
//  peer_ims.cc -- IMS implementation.
//
//  This code is distributed under the GNU General Public License (see
//  THE_GENERAL_GNU_PUBLIC_LICENSE.txt for extending this information).
//  Copyright (C) 2016, the P2PSP team.
//  http://www.p2psp.org
//
//

#include "peer_ims.h"

namespace p2psp {
  
  Peer_IMS::Peer_IMS() {

    // Initialized in Peer_IMS::ReceiveTheMcasteEndpoint()
    //mcast_addr_ = ip::address::from_string("0.0.0.0");
    //mcast_port_ = 0;
  }

  Peer_IMS::~Peer_IMS() {}

  void Peer_IMS::Init() {};

  void Peer_IMS::ListenToTheTeam() {
    // {{{

    ip::udp::endpoint endpoint(ip::address_v4::any(), mcast_port_);
    team_socket_.open(endpoint.protocol());
    team_socket_.set_option(ip::udp::socket::reuse_address(true));
    team_socket_.bind(endpoint);
    team_socket_.set_option(ip::multicast::join_group(mcast_addr_));

    TRACE("Listening to the mcast_channel = ("
	  << mcast_addr_.to_string()
	  << ","
          << std::to_string(mcast_port_)
          << ")");

    // }}}
  }

  int Peer_IMS::ProcessMessage(const std::vector<char> &message,
			       const ip::udp::endpoint &sender) {
    // {{{

    // Ojo, an attacker could send a packet smaller and pollute the buffer,
    // althought this is difficult in IP multicst. This method should be
    // inheritaged to solve this issue.

    uint16_t chunk_number = ntohs(*(short *)message.data());

    chunks_[chunk_number % buffer_size_] = {
      std::vector<char>(message.data() + sizeof(uint16_t),
                        message.data() + message.size()),
      true};

    received_counter_++;

    return chunk_number;

    // }}}
  }

  //std::string Peer_IMS::GetMcastAddr() {
}
