#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to an uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const auto& next_ip = next_hop.ipv4_numeric();
  auto iter_map = ARP_map.find( next_ip );
  EthernetFrame ethernetFrame {};
  if ( iter_map != ARP_map.end() && current_time_ms - iter_map->second.place_time < 30000 ) {
    // Actually got ARP and doesn't expire.
    ethernetFrame.header = {iter_map->second.mac_address, ethernet_address_, EthernetHeader::TYPE_IPv4};
    ethernetFrame.payload = serialize(dgram);
    send_queue.emplace(move(ethernetFrame));
  }
  else {
    if(!ARP_flood_prevent.contains(next_ip) || current_time_ms - ARP_flood_prevent[next_ip] >= 5000) {
      ARP_flood_prevent[next_ip] = current_time_ms;
      ethernetFrame.header = {ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP};
      ARPMessage arpMessage {};
      arpMessage.sender_ip_address = ip_address_.ipv4_numeric();
      arpMessage.sender_ethernet_address = ethernet_address_;
      arpMessage.target_ip_address = next_ip;
      arpMessage.opcode = ARPMessage::OPCODE_REQUEST;
      ethernetFrame.payload = serialize(arpMessage);
      send_queue.emplace(ethernetFrame);
    }
    ethernetFrame.header.type = EthernetHeader::TYPE_IPv4;
    ethernetFrame.payload = serialize(dgram);
    no_mac_address_frame[next_ip].emplace_back(move(ethernetFrame));
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return nullopt;
  }
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram ret {};
    if ( parse( ret, frame.payload ) ) {
      return ret;
    }
    return nullopt;
  }
  // Then the ethernet frame is meant for arp.
  ARPMessage arp_unboxed {};
  if ( parse( arp_unboxed, frame.payload ) ) {
    if ( arp_unboxed.opcode == ARPMessage::OPCODE_REQUEST ) {
      if ( arp_unboxed.target_ip_address != ip_address_.ipv4_numeric() ) {
        return nullopt;
      }
      ARPMessage arpMessage {};
      arpMessage.opcode = ARPMessage::OPCODE_REPLY;
      arpMessage.sender_ip_address = ip_address_.ipv4_numeric();
      arpMessage.sender_ethernet_address = ethernet_address_;
      arpMessage.target_ip_address = arp_unboxed.sender_ip_address;
      arpMessage.target_ethernet_address = arp_unboxed.sender_ethernet_address;
      EthernetFrame ethernetFrame_send {};
      // header: des sur type
      ethernetFrame_send.header = { arp_unboxed.sender_ethernet_address, ethernet_address_, frame.header.type };
      ethernetFrame_send.payload = serialize( arpMessage );
      send_queue.emplace( move( ethernetFrame_send ) );
      // Then we learn from this ARP
      ARP_map[arp_unboxed.sender_ip_address] = { arp_unboxed.sender_ethernet_address, current_time_ms };
    } else {
      // REPLY
      // Update Map and Push Frame in Vector to Queue
      ARP_map[arp_unboxed.sender_ip_address] = { frame.header.src, current_time_ms };
      if ( no_mac_address_frame.contains( arp_unboxed.sender_ip_address ) ) {
        auto& array = no_mac_address_frame[arp_unboxed.sender_ip_address];
        for ( auto& i : array ) {
          i.header.dst = frame.header.src;
          send_queue.emplace( i );
        }
        array.clear();
      }
      no_mac_address_frame.erase( arp_unboxed.sender_ip_address );
    }
  }
  return nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  current_time_ms += ms_since_last_tick;
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if ( send_queue.empty() ) {
    return nullopt;
  }
  auto ret = send_queue.front();
  send_queue.pop();
  return ret;
}
