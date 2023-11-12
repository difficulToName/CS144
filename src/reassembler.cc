#include "reassembler.hh"
using namespace std;

// Modify log:
// 1. In test "reassembler_win" we changed line 105, which sets the iterator out of string.
// 2. In test case contributed by Eli Wald, the code before change may store redundant part, which may undermine the capacity rule.
// 3. In Lab 2 the case reveals that the reassembler may give up the data that cannot be handed in instantly but is able to fill the space between existing data.

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  uint64_t end_index = first_index + data.size();
  if ( is_last_substring ) {
    is_last = true;
  }
  if ( data.empty() ) {
    if ( discrete_str.empty() && is_last ) {
      output.close();
      return;
    }
  }
  if ( end_index <= output.bytes_pushed() || output.available_capacity() - pending == 0 || first_index >= output.bytes_pushed() + output.available_capacity()) {
    return;
  }
  if ( first_index <= output.bytes_pushed() ) {
    data = data.substr( output.bytes_pushed() - first_index );

    if ( data.size() > output.available_capacity() ) {
      data.resize( data.size() - output.available_capacity() );
    }
    output.push( move( data ) );

    while ( !discrete_str.empty() && discrete_str.front().end <= output.bytes_pushed() ) {
      pending -= discrete_str.begin()->str.size();
      discrete_str.pop_front();
    }
    if ( !discrete_str.empty() && output.bytes_pushed() >= discrete_str.front().start ) {
      const string& front_str = discrete_str.begin()->str;
      string rest_str = { front_str.begin() + output.bytes_pushed() - discrete_str.front().start, front_str.end() };
      output.push( move( rest_str ) );
      pending -= front_str.size();
      discrete_str.pop_front();
    }

  } else {
    // For unit test of "Eli Wald"
    if(first_index + data.size() > output.bytes_pushed() + output.available_capacity()) {
      data.resize(output.available_capacity());
      end_index = first_index + data.size();
    }
    if ( first_index + data.size() > output.bytes_pushed() + output.available_capacity() ) {
      if ( discrete_str.empty() ) {
        data.resize( output.bytes_pushed() + output.available_capacity() - first_index );
        end_index = first_index + data.size();
      } else {
        uint64_t size_remain = 0;
        auto iter_back = discrete_str.end();
        --iter_back;
        size_remain += output.bytes_pushed() + output.available_capacity() - iter_back->end;
        if ( iter_back == discrete_str.begin() ) {
          auto iter_front = iter_back;
          --iter_front;
          while ( iter_front != discrete_str.begin() && first_index >= iter_front->start ) {
            size_remain += iter_back->start - iter_front->end;
            iter_back = iter_front;
            --iter_front;
          }
          if ( first_index > iter_front->end && first_index < iter_back->start ) {
            size_remain += iter_back->end - first_index;
          }
        }
        if ( size_remain < data.size() ) {
          data.resize( size_remain );
        }
      }
    }
    auto iter = discrete_str.begin();
    for ( auto find = discrete_str.begin(); find != discrete_str.end(); ++find ) {
      if ( first_index >= iter->start && end_index <= iter->end ) {
        return;
      }
    }

    while ( iter != discrete_str.end() && iter->end <= first_index ) {
      ++iter;
    }

    if ( iter != discrete_str.end() && iter->start > first_index ) {

      string front_str = { data.begin(), data.begin() + min( iter->start - first_index, data.size() ) };
      pending += front_str.size();
      discrete_str.emplace( iter, first_index, move( front_str ) );
    }

    while ( iter != discrete_str.end() && iter->end < end_index ) {
      auto front_iter = iter;
      ++iter;
      if ( iter == discrete_str.end() ) {
        break;
      }

      string filling = { data.begin() + front_iter->end - first_index,
                         data.begin() + min( iter->start - first_index, data.size() ) };

      pending += filling.size();
      discrete_str.emplace( iter, front_iter->end, move( filling ) );
    }

    if ( discrete_str.empty() || first_index >= discrete_str.back().end ) {
      pending += data.size();
      discrete_str.emplace_back( first_index, move( data ) );

    } else if ( iter == discrete_str.end() ) { // reassembler_win: To promise don't insert data already have
      --iter;
      if ( iter->end <= end_index ) {
        string end_str = { data.begin() + iter->end - first_index, data.end() };
        if ( !end_str.empty() ) {
          pending += end_str.size();
          discrete_str.emplace_back( iter->end, move( end_str ) );
        }
      }
    }
  }

  // Merge
  if ( !discrete_str.empty() ) {
    auto front = discrete_str.begin();
    auto back = front;
    ++back;
    while ( back != discrete_str.end() ) {
      while ( back != discrete_str.end() && front->end != back->start ) {
        ++front;
        ++back;
      }
      if ( back == discrete_str.end() ) {
        break;
      }
      while ( front->end == back->start ) {
        front->end += back->str.size();
        front->str += back->str;
        discrete_str.erase( back );
        back = front;
        ++back;
      }
      front = back;
      if ( back != discrete_str.end() ) {
        ++back;
      }
    }
  }
  // Pop
  while ( !discrete_str.empty() && discrete_str.begin()->start == output.bytes_pushed() ) {
    if ( discrete_str.front().str.empty() ) {
      discrete_str.pop_front();
      continue;
    }
    pending -= discrete_str.begin()->str.size();
    output.push( move( discrete_str.begin()->str ) );
    discrete_str.pop_front();
  }
  if ( discrete_str.empty() && is_last ) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return pending;
}