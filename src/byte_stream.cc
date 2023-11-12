#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( available_capacity() == 0 || data.empty() ) {
    return;
  }
  auto length = min( data.size(), available_capacity() );
  data.resize( length );
  data_buffer_.emplace_back( std::move( data ) );
  view_buffer.emplace_back( data_buffer_.back().c_str(), length );
  buffering += length;
  sent += length;
}

void Writer::close()
{
  writer_closed = true;
}

void Writer::set_error()
{
  writer_error = true;
}

bool Writer::is_closed() const
{
  return writer_closed;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffering;
}

uint64_t Writer::bytes_pushed() const
{
  return sent;
}

string_view Reader::peek() const
{
  if ( data_buffer_.empty() ) {
    return {};
  }

  return { view_buffer.front() };
}

bool Reader::is_finished() const
{
  return data_buffer_.empty() && writer_closed;
}

bool Reader::has_error() const
{
  return writer_error;
}

void Reader::pop( uint64_t len )
{
  len = min( len, buffering );
  buffering -= len;
  popped += len;
  while ( len > 0 ) {
    auto str_len = view_buffer.front().size();
    if ( str_len > len ) {
      view_buffer.front().remove_prefix( len );
      return;
    }
    view_buffer.pop_front();
    data_buffer_.pop_front();
    len -= str_len;
  }
}
uint64_t Reader::bytes_buffered() const
{
  return buffering;
}

uint64_t Reader::bytes_popped() const
{
  return popped;
}