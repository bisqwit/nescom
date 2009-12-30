#include <cstdio> // for stderr
#include "rangeset.hh"

template<typename Key,typename Allocator>
const typename rangeset<Key,Allocator>::const_iterator
    rangeset<Key,Allocator>::ConstructIterator(typename Cont::const_iterator i) const
{
    const_iterator tmp(data);
    while(i != data.end() && i->second.is_nil()) ++i;
    tmp.i = i;
    tmp.Reconstruct();
    return tmp;
}
template<typename Key,typename Allocator>
void rangeset<Key,Allocator>::const_iterator::Reconstruct()
{
    if(i != data.end())
    {
        rangetype<Key>::lower = i->first;
        typename Cont::const_iterator j = i;
        if(++j != data.end())
            rangetype<Key>::upper = j->first;
        else
            rangetype<Key>::upper = rangetype<Key>::lower;

        if(i->second.is_nil())
        {
            fprintf(stderr, "rangeset: internal error\n");
        }
    }
}
template<typename Key,typename Allocator>
typename rangeset<Key,Allocator>::const_iterator& rangeset<Key,Allocator>::const_iterator::operator++ ()
{
    /* Note: The last node before end() is always nil. */
    while(i != data.end())
    {
        ++i;
        if(!i->second.is_nil())break;
    }
    Reconstruct();
    return *this;
}
template<typename Key,typename Allocator>
typename rangeset<Key,Allocator>::const_iterator& rangeset<Key,Allocator>::const_iterator::operator-- ()
{
    /* Note: The first node can not be nil. */
    while(i != data.begin())
    {
        --i;
        if(!i->second.is_nil())break;
    }
    Reconstruct();
    return *this;
}

template<typename Key,typename Allocator>
rangeset<Key,Allocator> rangeset<Key,Allocator>::intersect(const rangeset<Key,Allocator>& b) const
{
    rangeset<Key,Allocator> result;
    const_iterator ai = begin();
    const_iterator bi = b.begin();

    for(;;)
    {
        if(ai == end()) break;
        if(bi == b.end()) break;

        if(ai->upper <= bi->lower) { ++ai; continue; }
        if(bi->upper <= ai->lower) { ++bi; continue; }

        rangetype<Key> intersection = ai->intersect(bi);
        if(!intersection.empty())
            result.set(intersection.lower, intersection.upper);

        if(ai->upper < bi->upper)         // A is smaller
            ++ai;
        else if(ai->upper == bi->upper)   // equal
            { ++ai; ++bi; }
        else                              // B is smaller
            ++bi;
    }
    return result;
}


template<typename Key,typename Allocator>
rangetype<Key> rangeset<Key,Allocator>::find_set_subrange
    (const Key& lo, const Key& up, size_t minlen, allocationstrategy strategy) const
{
    rangeset<Key,Allocator> tmp;
    tmp.set(lo,up);
    rangeset<Key,Allocator> clipped_by = tmp.intersect(*this);

    rangetype<Key> retval; bool first=true; size_t prevsize=0;
    for(const_iterator i = clipped_by.begin(); i != clipped_by.end(); ++i)
        if(i.upper - i.lower >= minlen)
            switch(strategy)
            {
                case First:
                    retval = i;
                    return retval;
                case Smallest:
                    if(first) { first=false; prevsize=i.upper-i.lower; retval=i; }
                    else if(i.upper-i.lower < prevsize) { prevsize=i.upper-i.lower; retval=i; }
                    break;
                case Largest:
                    if(first) { first=false; prevsize=i.upper-i.lower; retval=i; }
                    else if(i.upper-i.lower > prevsize) { prevsize=i.upper-i.lower; retval=i; }
                    break;
            }
    return retval;
}

template<typename Key,typename Allocator>
rangetype<Key> rangeset<Key,Allocator>::find_unset_subrange
    (const Key& lo, const Key& up, size_t minlen, allocationstrategy strategy) const
{
    rangeset<Key,Allocator> tmp;
    tmp.set(lo,up);
    rangeset<Key,Allocator> clipped_by = tmp.intersect(*this);
    for(const_iterator i = clipped_by.begin(); i != clipped_by.end(); ++i)
        tmp.erase(i.lower, i.upper);
    // Now tmp is the negated intersection

    rangetype<Key> retval; bool first=true; size_t prevsize=0;
    for(const_iterator i = tmp.begin(); i != tmp.end(); ++i)
        if(i.upper - i.lower >= minlen)
            switch(strategy)
            {
                case First:
                    retval = i;
                    return retval;
                case Smallest:
                    if(first) { first=false; prevsize=i.upper-i.lower; retval=i; }
                    else if(i.upper-i.lower < prevsize) { prevsize=i.upper-i.lower; retval=i; }
                    break;
                case Largest:
                    if(first) { first=false; prevsize=i.upper-i.lower; retval=i; }
                    else if(i.upper-i.lower > prevsize) { prevsize=i.upper-i.lower; retval=i; }
                    break;
            }
    return retval;
}
