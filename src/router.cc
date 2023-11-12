#include "router.hh"
#include <iostream>
#include <limits>

using namespace std;

uint32_t Router::prefix_generator( const uint8_t times )

{
  // So there will be '1' for times.
  return ( ( 1 << times ) - 1 ) << ( 32 - times );
}

auto Router::find_rule( const InternetDatagram& dgram )
{
  const auto& dest = dgram.header.dst;
  for ( auto iter_map = rule_map.begin(); iter_map != rule_map.end(); ++iter_map ) {
    for ( auto iter_vec = iter_map->second.begin(); iter_vec != iter_map->second.end(); ++iter_vec ) {
      if ( ( dest & prefix_generator( iter_map->first ) ) == iter_vec->route_destination ) {
        return iter_vec;
      }
    }
  }
  return rule_map.begin()->second.end();
}

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
  rule_map[prefix_length].push_back( { route_prefix, next_hop, interface_num } );
}

void Router::route()
{
  for ( auto& interface : interfaces_ ) {
    auto net_datagram = interface.maybe_receive();
    if ( !net_datagram.has_value() ) {
      continue;
    }
    auto& datagram = net_datagram.value();
    if ( datagram.header.ttl <= 1 ) {
      continue;
    }
    datagram.header.ttl -= 1;
    datagram.header.compute_checksum();
    const auto iter_no_value = rule_map.cbegin()->second.end();
    auto iter = find_rule( datagram );
    if ( iter == iter_no_value ) {
      continue;
    }
    interfaces_.at( iter->interface_num )
      .send_datagram( datagram, iter->next_hop.value_or( Address::from_ipv4_numeric( datagram.header.dst ) ) );
  }
}
