//
//  malicious_peer.cc
//  P2PSP
//
//  This code is distributed under the GNU General Public License (see
//  THE_GENERAL_GNU_PUBLIC_LICENSE.txt for extending this information).
//  Copyright (C) 2016, the P2PSP team.
//  http://www.p2psp.org
//

#include "malicious_peer.h"

namespace p2psp {

MaliciousPeer::MaliciousPeer(){
  magic_flags_ = Common::kDBS;
};
MaliciousPeer::~MaliciousPeer(){};

void MaliciousPeer::Init() { INFO("Initialized"); }

int MaliciousPeer::ProcessMessage(const std::vector<char> &message,
                                  const ip::udp::endpoint &sender) {
  // Now, receive and send.

  ip::udp::endpoint peer;

  // TODO: Remove hardcoded values
  if (message.size() == 1026) {
    // A video chunk has been received

    uint16_t chunk_number = ntohs(*(short *)message.data());

    chunk_ptr[chunk_number % buffer_size_].data = std::vector<char>(message.data() + sizeof(uint16_t),
                                                         message.data() + message.size());
    chunk_ptr[chunk_number % buffer_size_].received = chunk_number;

    received_counter_++;

    if (sender == splitter_) {
      // Send the previous chunk in burst sending
      // mode if the chunk has not been sent to all
      // the peers of the list of peers.

      TRACE("DBS: (" << team_socket_.local_endpoint().address().to_string()
                     << ","
                     << std::to_string(team_socket_.local_endpoint().port())
                     << ")"
                     << "<-" << std::to_string(chunk_number) << "-"
                     << "(" << sender.address().to_string() << ","
                     << std::to_string(sender.port()) << ")");

      while (receive_and_feed_counter_ < (int)peer_list_.size() &&
             receive_and_feed_counter_ > 0) {
        peer = peer_list_[receive_and_feed_counter_];
        SendChunk(peer);

        TRACE("DBS: (" << team_socket_.local_endpoint().address().to_string()
                       << ","
                       << std::to_string(team_socket_.local_endpoint().port())
                       << ")"
                       << "-"
                       << std::to_string(ntohs(receive_and_feed_previous_[0]))
                       << "->"
                       << "(" << peer.address().to_string() << ","
                       << std::to_string(peer.port()) << ")");

        debt_[peer]++;

        if (debt_[peer] > kMaxChunkDebt) {
          INFO("(" << peer.address().to_string() << ","
                  << std::to_string(peer.port()) << ")"
                  << " removed by unsupportive (" +
                         std::to_string(debt_[peer]) + " lossess)");
          debt_.erase(peer);
          peer_list_.erase(
              std::find(peer_list_.begin(), peer_list_.end(), peer));
        }

        receive_and_feed_counter_++;
      }

      receive_and_feed_counter_ = 0;
      receive_and_feed_previous_ = message;
    } else {
      // The sender is a peer

      TRACE("DBS: (" << team_socket_.local_endpoint().address().to_string()
                     << ","
                     << std::to_string(team_socket_.local_endpoint().port())
                     << ")"
                     << "<-" << std::to_string(chunk_number) << "-"
                     << "(" << sender.address().to_string() << ","
                     << std::to_string(sender.port()) << ")");

      if (peer_list_.end() ==
          std::find(peer_list_.begin(), peer_list_.end(), sender)) {
        peer_list_.push_back(sender);
        debt_[sender] = 0;
        INFO("(" << sender.address().to_string() << ","
                << std::to_string(sender.port()) << ")"
                << " added by chunk " << std::to_string(chunk_number));
      } else {
        debt_[sender]--;
      }
    }

    // A new chunk has arrived and the previous must be forwarded to next peer
    // of
    // the list of peers.

    std::vector<char> empty(1024);
    std::memset(empty.data(), 0, empty.size());

    if (receive_and_feed_counter_ < (int)peer_list_.size() &&
        receive_and_feed_previous_ != empty) {
      // Send the previous chunk in congestion avoiding mode.

      peer = peer_list_[receive_and_feed_counter_];
      SendChunk(peer);
      debt_[peer]++;

      if (debt_[peer] > kMaxChunkDebt) {
        INFO("DBS:(" << peer.address().to_string() << ","
                    << std::to_string(peer.port()) << ")"
                    << " removed by unsupportive (" +
                           std::to_string(debt_[peer]) + " lossess)");
        debt_.erase(peer);
        peer_list_.erase(std::find(peer_list_.begin(), peer_list_.end(), peer));
      }

      TRACE(
          "DBS:(" << team_socket_.local_endpoint().address().to_string() << ","
                  << std::to_string(team_socket_.local_endpoint().port()) << ")"
                  << "-" << std::to_string(ntohs(receive_and_feed_previous_[0]))
                  << "->"
                  << "(" << peer.address().to_string() << ","
                  << std::to_string(peer.port()) << ")");

      receive_and_feed_counter_++;
    }

    return chunk_number;
  } else {
    // A control chunk has been received

    INFO("DBS: Control message received");

    if (message[0] == 'H') {
      if (peer_list_.end() ==
          std::find(peer_list_.begin(), peer_list_.end(), sender)) {
        // The peer is new
        peer_list_.push_back(sender);
        debt_[sender] = 0;
        INFO("(" << sender.address().to_string() << ","
                << std::to_string(sender.port()) << ")"
                << " added by [hello] ");
      } else {
        if (peer_list_.end() !=
            std::find(peer_list_.begin(), peer_list_.end(), sender)) {
          // sys.stdout.write(Color.red)
          INFO("DBS: (" << team_socket_.local_endpoint().address().to_string()
                       << ","
                       << std::to_string(team_socket_.local_endpoint().port())
                       << ") \b: received \"goodbye\" from ("
                       << sender.address().to_string() << ","
                       << std::to_string(sender.port()) << ")");
          // sys.stdout.write(Color.none)
          peer_list_.erase(
              std::find(peer_list_.begin(), peer_list_.end(), sender));
          debt_.erase(sender);
        }
      }

      return -1;
    }
  }

  return -1;
}

void MaliciousPeer::SendChunk(const ip::udp::endpoint &peer) {
  std::vector<char> chunk = receive_and_feed_previous_;
  GetPoisonedChunk(chunk);
  if (persistent_attack_) {
    team_socket_.send_to(buffer(chunk), peer);
    sendto_counter_++;
    return;
  }

  if (on_off_attack_) {
    int r = std::rand() % 100 + 1;
    if (r <= on_off_ratio_) {
      team_socket_.send_to(buffer(chunk), peer);
    } else {
      team_socket_.send_to(buffer(receive_and_feed_previous_), peer);
    }
    sendto_counter_++;
    return;
  }

  if (selective_attack_) {
    if (std::find(selected_peers_for_attack_.begin(),
                  selected_peers_for_attack_.end(),
                  peer) != selected_peers_for_attack_.end()) {
      team_socket_.send_to(buffer(chunk), peer);
    } else {
      team_socket_.send_to(buffer(receive_and_feed_previous_), peer);
    }
    sendto_counter_++;
    return;
  }

  team_socket_.send_to(buffer(receive_and_feed_previous_), peer);
  sendto_counter_++;
}

void MaliciousPeer::GetPoisonedChunk(std::vector<char> &chunk) {
  int offset = sizeof(uint16_t);
  memset(chunk.data() + offset, 0, chunk.size() - offset);
}

void MaliciousPeer::SetPersistentAttack(bool value) {
  persistent_attack_ = value;
}

void MaliciousPeer::SetOnOffAttack(bool value, int ratio) {
  on_off_ratio_ = value;
  on_off_ratio_ = ratio;
}

void MaliciousPeer::SetSelectiveAttack(
    bool value, const std::vector<ip::udp::endpoint> selected) {
  selective_attack_ = true;
  selected_peers_for_attack_.insert(selected_peers_for_attack_.end(),
                                    selected.begin(), selected.end());
}
}
