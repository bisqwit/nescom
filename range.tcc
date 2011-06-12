#include <algorithm> // for std::max, std::min
#include "range.hh"

/* map::lower_bound(k) = find the first element whose key >= k */
/* map::upper_bound(k) = find the first element whose key > k */

template<typename Key, typename Valueholder, typename Allocator>
size_t rangecollection<Key,Valueholder,Allocator>::erase(const Key& lo, const Key& up)
{
    size_t n_removed = 0;

    typename Cont::iterator next_thing = data.lower_bound(up);
    if(next_thing != data.end() && next_thing->first == up)
    {
        /* If our "up" is already defined */
        if(next_thing->second.is_nil())
        {
            /* Erase it, if it's the same as us */
            data.erase(next_thing);
        }
    }
    else if(next_thing == data.begin())
    {
        /* If the world begins after our end, do nothing */
    }
    else
    {
        --next_thing;
        if(!next_thing->second.is_nil())
        {
            /* We need this node at our "up" */
            data.insert(next_thing, std::make_pair(up, next_thing->second));
        }
    }

    /*
     -  Erase all elements that are left inside our range
    */
    for(typename Cont::iterator j,i = data.lower_bound(lo);
        i != data.end() && i->first < up;
        i=j)
    {
        j=i; ++j;
        data.erase(i);
        ++n_removed;
    }

    /*
     -  Find what was going on before <lo>
        If it does not exist or was different than this,
          Insert the new value at <lo>
    */
    typename Cont::iterator prev_thing = data.lower_bound(lo);
    if(prev_thing == data.begin())
    {
        /* If nothing was going on, we do nothing */
    }
    else
    {
        /* Start us with a nil if the previous node wasn't nil */
        --prev_thing;
        if(!prev_thing->second.is_nil())
        {
            data.insert(prev_thing, std::make_pair(lo, Valueholder()));
            ++n_removed;
        }
    }
    return n_removed;
}

template<typename Key, typename Valueholder, typename Allocator>
size_t rangecollection<Key,Valueholder,Allocator>::erase_before(const Key& lo)
{
    if(!empty())
    {
        const_iterator b = begin();
        if(b->first < lo) return erase(b->first, lo);
    }
    return 0;
}

template<typename Key, typename Valueholder, typename Allocator>
size_t rangecollection<Key,Valueholder,Allocator>::erase_after(const Key& hi)
{
    if(!empty())
    {
        typename Cont::const_reverse_iterator b = data.rbegin();
        if(b->first > hi) return erase(hi, b->first);
    }
    return 0;
}

template<typename Key, typename Valueholder, typename Allocator>
  template<typename Valuetype>
void rangecollection<Key,Valueholder,Allocator>::set(const Key& lo, const Key& up, const Valuetype& val)
{
    Valueholder newvalue(val);

    /*
     -  Find what is supposed to be continuing after this block
        If nothing was going on before the end, add nil at <up>
        If was different than the new value, add it at <up>
    */
    typename Cont::iterator next_thing = data.lower_bound(up);
    if(next_thing != data.end() && next_thing->first == up)
    {
        /* If our "up" is already defined */
        if(next_thing->second == newvalue)
        {
            /* Erase it, if it's the same as us */
            data.erase(next_thing);
        }
    }
    else if(next_thing == data.begin())
    {
        /* We need a nil node at our "up" */
        data.insert(next_thing, std::make_pair(up, Valueholder()));
    }
    else
    {
        --next_thing;
        if(next_thing->second != newvalue)
        {
            /* We need this node at our "up" */
            data.insert(next_thing, std::make_pair(up, next_thing->second));
        }
    }

    /*
     -  Erase all elements that are left inside our range
    */
    for(typename Cont::iterator j,i = data.lower_bound(lo);
        i != data.end() && i->first < up;
        i=j)
    {
        j=i; ++j;
        data.erase(i);
    }

    /*
     -  Find what was going on before <lo>
        If it does not exist or was different than this,
          Insert the new value at <lo>
    */
    typename Cont::iterator prev_thing = data.lower_bound(lo);
    if(prev_thing == data.begin())
    {
        data.insert(prev_thing, std::make_pair(lo, newvalue));
    }
    else
    {
        --prev_thing;
        if(prev_thing->second != newvalue)
        {
            data.insert(prev_thing, std::make_pair(lo, newvalue));
        }
    }
}

template<typename Key, typename Valueholder, typename Allocator>
const typename rangecollection<Key,Valueholder,Allocator>::const_iterator
    rangecollection<Key,Valueholder,Allocator>::find(const Key& v) const
{
    typename Cont::const_iterator tmp = data.lower_bound(v);
    if(tmp == data.end())
    {
        return tmp;
        // return end(); -^ the same thing
    }
    if(tmp->first > v)
    {
        // This range begins after the given key, so get the previous one
        if(tmp == data.begin()) return end();
        --tmp;
    }
    if(tmp->second.is_nil()) return end();
    return tmp;
}

template<typename Key>
rangetype<Key> rangetype<Key>::intersect(const rangetype<Key>& b) const
{
    rangetype<Key> result;
    result.lower = std::max(lower, b.lower);
    result.upper = std::min(upper, b.upper);
    if(result.upper < result.lower) result.upper = result.lower;
    return result;
}


template<typename Key, typename Valueholder, typename Allocator>
void rangecollection<Key,Valueholder,Allocator>::offset
    (const Key& lo, const Key& up, long offset, bool delete_when_zero)
{
    if(offset == 0 && !delete_when_zero) return;

    // Find out what comes after our range.
    // Ensure there's a definite changepoint at "up".
    { typename Cont::iterator next_thing(data.lower_bound(up));
      if(next_thing == data.end()
      || next_thing->first != up)
      {
          /* We need a node at our "up". Simply clone whatever
           * preceded next_thing -- or if it's the beginning,
           * insert a nil node.
           */
          if(next_thing == data.begin())
              data.insert(next_thing, std::make_pair(up, Valueholder())); // nil node
          else
          {
              typename Cont::iterator prev_thing(next_thing); --prev_thing;
              data.insert(next_thing, std::make_pair(up, prev_thing->second));
    } }   }

    // Find out what before after our range.
    // Ensure there's a definite changepoint at "lo".
    { typename Cont::iterator prev_thing(data.lower_bound(lo));
      if(prev_thing == data.end()
      || prev_thing->first != lo)
      {
          /* We need a node at our "lo". Simply clone whatever
           * preceded next_thing -- or if it's the beginning,
           * insert a nil node.
           */
          if(prev_thing == data.begin())
              data.insert(prev_thing, std::make_pair(lo, Valueholder())); // nil node
          else
          {
              typename Cont::iterator prev2_thing(prev_thing); --prev2_thing;
              data.insert(prev_thing, std::make_pair(lo, prev2_thing->second));
    } }   }

    // For all changepoints in our range, update the value.
    for(typename Cont::iterator i(data.lower_bound(lo)); i->first != up; ++i)
    {
        i->second.set(i->second.get_value() + offset);
        if(delete_when_zero && i->second.get_value() == 0)
        {
            i->second.clear();
        }
    }

    // Last, remove duplicate "change" points we possibly introduced
    { typename Cont::iterator next_thing(data.lower_bound(up)); // This exists.
      typename Cont::iterator prev_thing(next_thing); --prev_thing;
      if(prev_thing->second == next_thing->second)
      {
          // next_thing is redundant
          data.erase(next_thing);
    } }

    { typename Cont::iterator prev_thing(data.lower_bound(lo)); // This exists.
      typename Cont::iterator prev2_thing(prev_thing); --prev2_thing;
      if(prev_thing->second == prev2_thing->second)
      {
          // prev_thing is redundant
          data.erase(prev_thing);
    } }
}
