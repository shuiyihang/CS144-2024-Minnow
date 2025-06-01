#include "tcp_sender.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <algorithm>
#include <cstdint>
#include <string>

#include "byte_stream.hh"
#include <iostream>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return next_seqno_ - ackno_; // 未完成的序列号个数
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retran_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t window = window_size_ > 0 ? window_size_ : 1;

  while ( next_seqno_ < ackno_ + window ) { // 只发送窗口内的

    uint64_t space_left = ackno_ + window - next_seqno_;
    if ( space_left == 0 )
      break;

    TCPSenderMessage msg;
    if ( !syn_sent_ ) {
      msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
      msg.SYN = true;

      handle_fin_and_rst( msg, space_left );

      syn_sent_ = true;

      if ( msg.sequence_length() >= space_left ) {
        if ( !timer_running_ ) {
          timer_start();
        }
        transmit( msg );
        outstanding_seg_.emplace_back( next_seqno_, msg );
        next_seqno_ += msg.sequence_length();
        return;
      }
      space_left -= msg.sequence_length(); // > 0
    }

    uint64_t payload_size = std::min( TCPConfig::MAX_PAYLOAD_SIZE, space_left );

    std::string payload;
    read( input_.reader(), payload_size, payload );

    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
    msg.payload = move( payload );

    handle_fin_and_rst( msg, space_left );

    if ( msg.sequence_length() == 0 ) { // syn + payload + fin
      break;
    }

    transmit( msg );

    if ( !timer_running_ ) {
      timer_start();
    }

    uint64_t msg_len = msg.sequence_length();
    outstanding_seg_.emplace_back( next_seqno_, move( msg ) );

    next_seqno_ += msg_len;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
  if ( input_.reader().has_error() ) {
    msg.RST = true;
  }
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size_ = msg.window_size;
  if ( msg.RST ) {
    input_.set_error();
  }
  if ( !msg.ackno.has_value() )
    return;

  uint64_t abs_ackno = msg.ackno->unwrap( isn_, next_seqno_ );

  if ( abs_ackno > next_seqno_ || abs_ackno <= ackno_ ) { // 忽略
    return;
  }
  if ( abs_ackno > ackno_ ) { // 如果确认了新数据
    ackno_ = abs_ackno;
    retran_timeout_ = initial_RTO_ms_;
    consecutive_retran_ = 0;
  }

  while ( !outstanding_seg_.empty() ) {
    const auto& seg_info = outstanding_seg_.front();
    if ( ackno_ >= seg_info.first + seg_info.second.sequence_length() ) {
      outstanding_seg_.pop_front();
    } else {
      break;
    }
  }

  if ( outstanding_seg_.empty() ) {
    timer_stop();
  } else {
    timer_start(); // 还有未确认段,重启定时器
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( !timer_running_ )
    return;

  time_since_last_tran_ += ms_since_last_tick;
  if ( time_since_last_tran_ >= retran_timeout_ ) {
    if ( !outstanding_seg_.empty() ) {
      const auto& [seqno, msg] = outstanding_seg_.front();
      transmit( msg );

      if ( window_size_ > 0 ) {
        consecutive_retran_++;
        retran_timeout_ *= 2;
      }
    }
    time_since_last_tran_ = 0; // 重新开始定时
  }
}

void TCPSender::timer_start()
{
  timer_running_ = true;
  time_since_last_tran_ = 0;
}

void TCPSender::timer_stop()
{
  timer_running_ = false;
}

void TCPSender::handle_fin_and_rst( TCPSenderMessage& msg, uint64_t space_left )
{
  if ( !fin_sent_ && input_.reader().is_finished() && space_left > msg.sequence_length() ) {
    msg.FIN = true;
    fin_sent_ = true; // 阻止最外层while重复发生fin
  }
  if ( input_.reader().has_error() ) {
    msg.RST = true;
  }
}