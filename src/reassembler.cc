#include "reassembler.hh"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <sys/types.h>
#include <utility>
#include <iostream>

#include <assert.h>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{

  if(!is_end_ && is_last_substring){
    is_end_ = true;
    endof_index_ = first_index + data.size();// 妙
  }
  
  // 1. 空的不要
  if(data.empty()){
    check_stream_close();
    return;
  } 

  // 2. 完全重复的不要
  uint64_t end = first_index + data.size();
  if(end <= first_unassembled_){
    check_stream_close();
    return;
  }

  // 3. 前缀重复的去掉前缀
  if(first_index < first_unassembled_){

    uint64_t cut = first_unassembled_ - first_index;
    first_index = first_unassembled_;

    return insert(first_index, data.substr(cut), is_last_substring);
  }
  // 4. 超出缓存的不要
  if(first_index > first_unacceptable()){
    check_stream_close();
    return;
  } 
  if(end > first_unacceptable()){
    uint64_t cut = first_unacceptable() - first_index; // 截取的长度
    return insert(first_index, data.substr(0,cut), is_last_substring);
  }

  uint64_t new_end = first_index + data.size();
  // 合并重叠的段
  auto itr = cache_.lower_bound(first_index);
  if(itr != cache_.begin()){
    itr--;// 新插入段的前后段可能会发生合并
  }

  std::string new_data = move(data);
  std::string merged;

  while(itr != cache_.end()){
    uint64_t seg_start = itr->first;
    uint64_t seg_end = itr->second.size() + seg_start;

    if(seg_end < first_index){
      itr++;
      continue;
    }
    if(seg_start > new_end) break;

    uint64_t merged_start = std::min(seg_start, first_index);
    uint64_t merged_end = std::max(seg_end,new_end);

    merged.resize(merged_end - merged_start);

    std::copy(new_data.begin(),new_data.end(),merged.begin()+(first_index-merged_start));
    std::copy(itr->second.begin(), itr->second.end(), merged.begin()+(seg_start-merged_start));

    first_index = merged_start;
    new_end = merged_end;
    new_data = move(merged);
    itr = cache_.erase(itr);
  }

  cache_[first_index] = move(new_data);
  Writer& writer = output_.writer();

  while (!cache_.empty()) {
    auto it = cache_.begin();
    if(it->first > first_unassembled_) break;

    uint64_t seg_start = it->first;
    std::string& seg_data = it->second;

    if(seg_start + seg_data.size() <= first_unassembled_){
      cache_.erase(it);
      continue;
    }

    uint64_t cut = first_unassembled_ - seg_start;
    std::string to_write = seg_data.substr(cut);
    first_unassembled_ += to_write.size();
    writer.push(move(to_write));
    cache_.erase(it);
  }


  check_stream_close();
}

void Reassembler::check_stream_close()
{
  Writer& writer = output_.writer();
  if(is_end_ && first_unassembled_ == endof_index_){
    std::cout << "first_unassembled_: " << first_unassembled_ << std::endl;
    writer.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t cache_size = 0;
  for(auto itr = cache_.cbegin(); itr != cache_.cend(); itr++){
    cache_size += itr->second.size();
  }
  return cache_size;
}


/**



*/