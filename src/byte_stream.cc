#include "byte_stream.hh"
#include <algorithm>
#include <string>
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ),contain_size_(capacity+1), buf_(contain_size_){

}

bool Writer::is_closed() const
{
  // Your code here.
  return closed_;
}

void Writer::push( string data )
{
  uint64_t data_len = data.size();
  uint64_t push_len = data_len > available_capacity() ? available_capacity() : data_len;
  string_view push_data = string_view(data).substr(0,push_len);
  if(contain_size_ - wof_ < push_len){
    uint64_t front_part = contain_size_ - wof_;
    std::copy(push_data.cbegin(),push_data.cbegin()+front_part,buf_.begin()+wof_);
    std::copy(push_data.cbegin()+front_part, push_data.cend(), buf_.begin());
  }else{
    std::copy(push_data.cbegin(), push_data.cend(), buf_.begin()+wof_);
  }

  wof_ += push_len;
  wof_ %= contain_size_;
  total_push_bytes_ += push_len;

  elem_n += push_len;
}

void Writer::close()
{
  // Your code here.
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - elem_nums();
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return total_push_bytes_;
}

bool Reader::is_finished() const
{
  // Your code here.
  return is_empty() && closed_;
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return total_pop_bytes_;
}

string Reader::peek() const
{
  // Your code here.
  string res;
  if(rof_ + elem_nums() > contain_size_){
    res.append(buf_.cbegin()+rof_, buf_.cend());
    res.append(buf_.cbegin(), buf_.cbegin()+wof_);
  }else{
    res.append(buf_.cbegin()+rof_, buf_.cbegin()+rof_+elem_nums());
  }

  return res;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  uint64_t pop_len = len > elem_nums() ? elem_nums() : len;
  rof_ += pop_len;
  rof_ %= contain_size_;
  total_pop_bytes_ += pop_len;

  elem_n -= pop_len;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return elem_nums();
}
