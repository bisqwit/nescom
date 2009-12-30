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

