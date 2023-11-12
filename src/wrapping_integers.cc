#include "wrapping_integers.hh"
using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Convert absolute seqno -> seqno
  return zero_point + n;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  const uint64_t TWO32 = ( 1UL << 32 ); // 2^32
  // Wrap uint32_t && seqno
  // Checkpoint = Window Position
  const uint32_t offset = raw_value_ - zero_point.raw_value_;

  // That have two branches: raw_value_ > zero_point.raw_value_
  // False: absolute sequence num must greater than 2^32
  // True: Maybe absolute num <= 2^32 or not but raw_value just greater than zero_point

  if ( offset >= checkpoint ) {
    return offset;
  }
  const uint64_t real_checkpoint = ( TWO32 >> 1 ) + ( checkpoint - offset );
  const uint64_t offset_times = real_checkpoint / TWO32;
  return TWO32 * offset_times + offset;
}
