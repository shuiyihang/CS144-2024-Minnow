#include <iostream>
#include <string>
#include <utility>

#include "arp_message.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"
#include "parser.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
  , arp_cached()
  , waiting_dgram()
  , waiting_arp_rsp()
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // if(dgram.header.ttl)
  if ( arp_cached.find( next_hop.ipv4_numeric() ) != arp_cached.end() ) {
    send( ethernet_address_, std::get<0>( arp_cached[next_hop.ipv4_numeric()] ), EthernetHeader::TYPE_IPv4, dgram );
  } else {
    // 暂时没有先将 同一IP的数据报缓存下来，发送arp查询，等到arp reply时，将数据报发送出去
    waiting_dgram[next_hop.ipv4_numeric()].push_back( dgram );
    // std::cout << "[debug]:" << next_hop.ipv4_numeric() << " cached" << std::endl;

    if ( waiting_arp_rsp.find( next_hop.ipv4_numeric() ) == waiting_arp_rsp.end() ) {
      // 还没有发送过arp请求
      ARPMessage arp;
      arp.opcode = ARPMessage::OPCODE_REQUEST;
      arp.sender_ethernet_address = ethernet_address_;
      arp.sender_ip_address = ip_address_.ipv4_numeric();
      arp.target_ip_address = next_hop.ipv4_numeric(); // 目标IP地址

      waiting_arp_rsp[next_hop.ipv4_numeric()] = NetworkInterface::ARP_TIMEOUT_TTL;

      send( ethernet_address_, ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP, arp );
    }
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  std::string dest = to_string( frame.header.dst );
  if ( dest != to_string( ETHERNET_BROADCAST ) && dest != to_string( ethernet_address_ ) ) {
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) ) {
      datagrams_received_.push( move( dgram ) );
    } else {
      std::cerr << " bad IPv4 datagram " << std::endl;
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp;
    if ( parse( arp, frame.payload ) ) {

      arp_cached[arp.sender_ip_address]
        = std::make_pair( arp.sender_ethernet_address, NetworkInterface::EXPIRE_TIME );

      // 广播出去的arp请求，只有目标ip相同的机器需要回应
      if ( arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == ip_address_.ipv4_numeric() ) {
        ARPMessage rsp;
        rsp.opcode = ARPMessage::OPCODE_REPLY;
        rsp.sender_ethernet_address = ethernet_address_;
        rsp.sender_ip_address = ip_address_.ipv4_numeric();
        rsp.target_ethernet_address = arp.sender_ethernet_address;
        rsp.target_ip_address = arp.sender_ip_address;

        send( ethernet_address_, arp.sender_ethernet_address, EthernetHeader::TYPE_ARP, rsp );
      } else if ( arp.opcode == ARPMessage::OPCODE_REPLY ) {
        // do nothing
        // std::cout << "[debug]: recv arp rsp"
        //           << " sender addr " << arp.sender_ip_address << std::endl;
        if ( waiting_dgram.find( arp.sender_ip_address ) != waiting_dgram.end() ) {
          // std::cout << "[debug]: recv arp rsp,resend dgram" << std::endl;
          waiting_arp_rsp.erase( arp.sender_ip_address );
          for ( auto& dgram : waiting_dgram[arp.sender_ip_address] ) {
            send( ethernet_address_, arp.sender_ethernet_address, EthernetHeader::TYPE_IPv4, dgram );
          }
          waiting_dgram.erase( arp.sender_ip_address );
        }
      }

    } else {
      std::cerr << "bad ARP message" << std::endl;
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // 缓存
  for ( auto itr = arp_cached.begin(); itr != arp_cached.end(); ) {
    if ( std::get<1>( itr->second ) <= ms_since_last_tick ) {
      itr = arp_cached.erase( itr ); // arp缓存过期清除
    } else {
      std::get<1>( itr->second ) -= ms_since_last_tick;
      itr++;
    }
  }
  // arp请求
  for ( auto it = waiting_arp_rsp.begin(); it != waiting_arp_rsp.end(); ) {
    if ( it->second <= ms_since_last_tick ) {
      std::cerr << "[debug]: arp timeout: [ ip:" << it->first << " ]" << std::endl;
      waiting_dgram.erase( it->first );
      it = waiting_arp_rsp.erase( it );
    } else {
      it->second -= ms_since_last_tick;
      it++;
    }
  }
}

template<typename DatagramType>
void NetworkInterface::send( const EthernetAddress& src,
                             const EthernetAddress& dst,
                             uint16_t type,
                             const DatagramType& dgram )
{
  EthernetFrame frame;
  frame.header.src = src;
  frame.header.dst = dst;
  frame.header.type = type;

  frame.payload = serialize( dgram );
  transmit( frame );
}

template void NetworkInterface::send( const EthernetAddress& src,
                                      const EthernetAddress& dst,
                                      uint16_t type,
                                      const InternetDatagram& dgram );
template void NetworkInterface::send( const EthernetAddress& src,
                                      const EthernetAddress& dst,
                                      uint16_t type,
                                      const ARPMessage& dgram );