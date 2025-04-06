#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <utility>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) )
    , isn_( isn )
    , initial_RTO_ms_( initial_RTO_ms )
    , syn_sent_(false)
    , fin_sent_(false)
    , timer_running_(false)
    , time_since_last_tran_(0)
    , retran_timeout_(initial_RTO_ms_)
    , consecutive_retran_(0)
    , window_size_(1)
    , ackno_(0)
    , next_seqno_(0)
    , outstanding_seg_()
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

private:
  bool syn_sent_;
  bool fin_sent_;
  bool timer_running_;
  uint64_t time_since_last_tran_;
  uint64_t retran_timeout_;
  uint64_t consecutive_retran_;// 连续重传的次数
  uint64_t window_size_;
  uint64_t ackno_;// 最大被ack的绝对序列号
  uint64_t next_seqno_;
  std::deque<std::pair<uint64_t, TCPSenderMessage>> outstanding_seg_;

private:
  void timer_start();
  void timer_stop();
  inline void handle_fin_and_rst(TCPSenderMessage& msg,uint64_t space_left);

};
