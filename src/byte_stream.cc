#include "byte_stream.hh"
#include <algorithm>
#include <string>
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), buffer_(), prefix_( 0 ), elem_nums_( 0 ) {}

bool Writer::is_closed() const
{
  // Your code here.
  return closed_;
}

void Writer::push( string data )
{
  uint64_t data_len = data.size();
  uint64_t push_len = min( available_capacity(), data_len );
  if ( push_len == 0 )
    return; // 一定要min之后

  data.resize( push_len );
  buffer_.push( move( data ) );
  total_push_bytes_ += push_len;
  elem_nums_ += push_len;
}

void Writer::close()
{
  // Your code here.
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - elem_nums_;
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return total_push_bytes_;
}

bool Reader::is_finished() const
{
  // Your code here.
  return ( elem_nums_ == 0 ) && closed_;
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return total_pop_bytes_;
}

string_view Reader::peek() const
{
  // Your code here.
  if ( elem_nums_ == 0 )
    return string_view {};

  string_view res( buffer_.front() );
  res.remove_prefix( prefix_ );

  return res;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  uint64_t pop_len = min( len, elem_nums_ );

  total_pop_bytes_ += pop_len;
  elem_nums_ -= pop_len;

  while ( pop_len ) {
    if ( pop_len >= buffer_.front().size() - prefix_ ) {
      pop_len -= ( buffer_.front().size() - prefix_ );
      buffer_.pop();
      prefix_ = 0;
    } else {
      prefix_ += pop_len;
      pop_len = 0;
    }
  }
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return elem_nums_;
}
