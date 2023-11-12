#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return bytes_outstanding;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_re;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if ( send_queue.empty() ) {
    return nullopt;
  }
  if ( !timer.is_started() ) {
    timer.start();
  }
  auto ret = send_queue.front();
  send_queue.pop();
  return ret;
}

void TCPSender::push( Reader& outbound_stream )
{
  const uint64_t to_be_pushed = current_window_size == 0 ? 1 : current_window_size;
  while ( bytes_outstanding < to_be_pushed ) {
    TCPSenderMessage message {};
    message.seqno = Wrap32::wrap( bytes_sent, isn_ );
    if ( !SYN ) {
      message.SYN = true;
      SYN = true;
      bytes_outstanding += 1;
    }
    const uint64_t current_turn_len = min( TCPConfig::MAX_PAYLOAD_SIZE, to_be_pushed - bytes_outstanding );
    read( outbound_stream, current_turn_len, message.payload );
    bytes_outstanding += message.payload.size();
    if ( bytes_outstanding < to_be_pushed && outbound_stream.is_finished() && !FIN ) {
      message.FIN = true;
      FIN = true;
      bytes_outstanding += 1;
    }
    if ( message.sequence_length() == 0 ) {
      break;
    }
    send_queue.emplace( message );
    maybe_re_trans.emplace( message );
    bytes_sent += message.sequence_length();
    if ( outbound_stream.bytes_buffered() == 0 ) {
      break;
    }
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return { Wrap32::wrap( bytes_sent, isn_ ), false, { "" }, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  current_window_size = msg.window_size;
  const uint64_t ackno_needed = msg.ackno->unwrap( isn_, bytes_sent );
  if ( ackno_needed > bytes_sent ) {
    return;
  }
  while ( !maybe_re_trans.empty() ) {
    const uint64_t end_position
      = maybe_re_trans.front().seqno.unwrap( isn_, bytes_sent ) + maybe_re_trans.front().sequence_length();
    if ( end_position <= ackno_needed ) {
      bytes_outstanding -= maybe_re_trans.front().sequence_length();
      maybe_re_trans.pop();
      timer.reset_RTO();
      if ( !maybe_re_trans.empty() ) {
        timer.start();
      }
      consecutive_re = 0;
    } else {
      break;
    }
  }
  if ( maybe_re_trans.empty() ) {
    timer.stop();
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  timer.elapse( ms_since_last_tick );
  if ( timer.is_expired() ) {
    send_queue.emplace( maybe_re_trans.front() );
    if ( current_window_size != 0 ) {
      consecutive_re += 1;
      timer.double_RTO();
    }
    timer.start();
  }
}
