#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <algorithm>
#include <cstdint>
#include <type_traits>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if(message.RST){
    reassembler_.set_stream_err();
    return;
  }

  if(!syn_flag_){
    if(!message.SYN){
      return;
    }
    syn_flag_ = true;
    zero_point_ = Wrap32::wrap(0, message.seqno);
  }
  /*
  绝对序列：
  syn| c| a| t| fin
  0  | 1| 2| 3| 4

  流序列：
  syn| c| a| t| fin
      0 | 1| 2|
  */
  uint64_t stream_index = message.seqno.unwrap(zero_point_, reassembler_.first_unassembled()) - (message.SYN == true ? 0 : 1);
  reassembler_.insert(stream_index, move(message.payload), message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage rsp;
  
  if(syn_flag_){
    uint64_t ack_index = reassembler_.writer().bytes_pushed();
    ack_index += 1;// syn
    ack_index += reassembler_.writer().is_closed();// fin

    rsp.ackno = Wrap32::wrap( ack_index, zero_point_);
  }

  rsp.window_size = static_cast<uint16_t>(std::min(reassembler_.writer().available_capacity(), static_cast<uint64_t>(UINT16_MAX)));

  if(reassembler_.writer().has_error()){
    rsp.RST = true;
  }

  return rsp;
}
