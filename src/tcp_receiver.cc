#include "tcp_receiver.hh"
#include <iostream>
using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{

  if ( message.SYN ) {
    activated = true;
    // This datagram is SYN marked.
    ISN_Number = message.seqno;
    message.seqno = message.seqno + 1;
  }
  if ( activated ) {
    const uint64_t unwrapped_Wrap32 = message.seqno.unwrap( ISN_Number, inbound_stream.bytes_pushed() ) - 1;
    reassembler.insert( unwrapped_Wrap32, message.payload.release(), message.FIN, inbound_stream );
  }

}
TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // Next should send and window size
  const uint16_t available_capacity
    = inbound_stream.available_capacity() >= 65535 ? 65535 : inbound_stream.available_capacity();
  if ( !activated ) {
    return { nullopt, available_capacity };
  }
  return { Wrap32::wrap( activated + inbound_stream.bytes_pushed() + inbound_stream.is_closed(), ISN_Number ),
           available_capacity };
}
