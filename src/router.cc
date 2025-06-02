#include "router.hh"
#include "address.hh"

#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  if ( m_root == nullptr ) {
    m_root = std::make_unique<TrieNode>();
  }

  TrieNode* node = m_root.get();

  for ( int i = 0; i < prefix_length; i++ ) {
    bool bit = ( route_prefix & ( Router::mask >> i ) ) != 0;
    if ( node->m_child[bit] == nullptr ) {
      node->m_child[bit] = std::make_unique<TrieNode>();
    }
    node = node->m_child[bit].get();
  }
  node->m_val = RouteEntry( route_prefix, prefix_length, next_hop, interface_num ); // 根结点记录默认路由 0.0.0.0/0
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( auto interface : _interfaces ) {
    while ( !interface->datagrams_received().empty() ) {
      InternetDatagram& dgram = interface->datagrams_received().front();

      if ( dgram.header.ttl > 1 ) {
        dgram.header.ttl -= 1;
        dgram.header.compute_checksum();
        auto entry = get_next_hop( dgram.header.dst );
        if ( entry.has_value() ) {
          Address next_hop = entry->m_next_hop.value_or( Address::from_ipv4_numeric( dgram.header.dst ) );
          _interfaces[entry->m_interface_num]->send_datagram( dgram, next_hop );
        }
      }
      interface->datagrams_received().pop();
    }
  }
}

std::optional<Router::RouteEntry> Router::get_next_hop( uint32_t dst )
{
  std::optional<RouteEntry> res = nullopt;

  TrieNode* node = m_root.get();

  for ( int i = 0; i < 32; i++ ) {
    bool bit = ( ( mask >> i ) & dst ) != 0;
    if ( node->m_child[bit] == nullptr ) {
      break;
    }
    node = node->m_child[bit].get();
  }

  if ( !node->m_val.has_value() ) {
    node = m_root.get(); // 使用默认路由
  }

  res = node->m_val;
  cerr << "DEBUG: find route dst: " << Address::from_ipv4_numeric( dst ).ip() << " "
       << Address::from_ipv4_numeric( node->m_val->m_route_prefix ).ip() << "/"
       << static_cast<int>( node->m_val->m_prefix_length ) << " => "
       << ( node->m_val->m_next_hop.has_value() ? node->m_val->m_next_hop->ip() : "(direct)" ) << " on interface "
       << node->m_val->m_interface_num << std::endl;

  return res;
}