#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <queue>
#include <vector>

class Timer
{
private:
  bool _running_ { false };
  uint64_t _initial_RTO;
  uint64_t _actual_RTO;
  uint64_t _current_time_elapsed {};

public:
  Timer() = delete;
  explicit Timer( uint64_t init_RTO ) : _initial_RTO( init_RTO ), _actual_RTO( init_RTO ) {}
  bool is_started() const { return _running_; }

  void start()
  {
    _running_ = true;
    _current_time_elapsed = 0;
  }

  void stop() { _running_ = false; }

  void elapse( uint64_t since_last )
  {
    if ( _running_ ) {
      _current_time_elapsed += since_last;
    }
  }

  void double_RTO() { _actual_RTO *= 2; }

  void reset_RTO() { _actual_RTO = _initial_RTO; }

  bool is_expired() const { return _running_ && _current_time_elapsed >= _actual_RTO; }
};

class TCPSender
{
private:
  bool SYN { false };
  bool FIN { false };
  Wrap32 isn_;
  uint16_t current_window_size { 1 };
  uint64_t initial_RTO_ms_;
  uint64_t bytes_sent { 0 };
  uint64_t bytes_outstanding { 0 };
  uint64_t consecutive_re { 0 };
  Timer timer { initial_RTO_ms_ };
  std::queue<TCPSenderMessage> send_queue {};
  std::queue<TCPSenderMessage> maybe_re_trans {};

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
