#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 {static_cast<uint32_t>(zero_point.raw_value_ + n)};
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // seqno → absolute seqno
  uint32_t diff = raw_value_ - zero_point.raw_value_;
  uint64_t base = (checkpoint >> 32) << 32;

  uint64_t res = base + diff;

  // res, res+2^32, res-2^32
  // 以checkpoint为中心，左右各 2^31
  if(res + (1ul << 31) < checkpoint){ // 必须要是+,而不能是 checkpoint - (1ul << 31)
    res += (1ul << 32);
  }else if(res > checkpoint + (1ul << 31) && checkpoint > (1ul << 31)){
    res -= (1ul << 32);// 对于 - 操作,小心环回,所以确保左区间存在才 - 
  }

  return res;
}
