#include <list>

#include "rangeset.hh"

template<typename Key>
void rangeset<Key>::erase(const Key& lo, const Key& up)
{
    range newrange;
    newrange.lower = lo;
    newrange.upper = up;
    
    typedef range tmpelem;
    typedef std::list<tmpelem> tmplist;
    tmplist newitems;
    
    /* FIXME: This is slower than it needs to be */
    for(iterator b,a=data.begin(); a!=data.end(); a=b)
    {
        b = a; ++b;
        if(a->coincides(newrange))
        {
            if(a->lower < lo)
            {
                range lowrange;
                lowrange.lower = a->lower;
                lowrange.upper = lo;
                newitems.push_front(lowrange);
            }
            if(a->upper > up)
            {
                range uprange;
                uprange.lower = up;
                uprange.upper = a->upper;
                newitems.push_front(uprange);
            }
            data.erase(a);
        }
    }
    for(typename tmplist::const_iterator
            i = newitems.begin();
            i != newitems.end();
            ++i)
    {
        data.insert(*i);
    }
}

template<typename Key>
void rangeset<Key>::erase(const Key& value)
{
    erase(value, value+1);
}

template<typename Key>
void rangeset<Key>::set(const Key& lo, const Key& up)
{
    erase(lo, up);
    
    /* FIXME: This is way slower than it needs to be */
    range newrange;
    newrange.lower = lo;
    newrange.upper = up;
    data.insert(newrange);
}

template<typename Key>
void rangeset<Key>::insert(const Key& value)
{
    set(value, value+1);
}



template<typename Key>
typename rangeset<Key>::const_iterator
    rangeset<Key>::find(const Key& v) const
{
    /* FIXME: Should use lower_bound, upper_bound or something */
    
    for(const_iterator a=data.begin(); a!=data.end(); ++a)
        if(a->contains(v)) return a;
    return data.end();
}

#if 0
template<typename Key>
template<typename Listtype>
void rangeset<Key>::find_all_coinciding
   (const Key& lo, const Key& up,
    Listtype& target)
{
    range newrange;
    newrange.lower = lo;
    newrange.upper = up;
    
    target.clear();
    
    for(const_iterator a=data.begin(); a!=data.end(); ++a)
        if(a->coincides(newrange))
            target.push_back(a);
}
#endif

template<typename Key>
void rangeset<Key>::compact()
{
Retry:
    for(iterator b,a=data.begin(); a!=data.end(); a=b)
    {
        b=a; ++b;
        
        // If the next one is followup to this one
        if(b->lower == a->upper)
        {
            range newrange;
            newrange.lower = a->lower;
            newrange.upper = b->upper;
            
            data.insert(newrange);
            data.erase(a);
            data.erase(b);
            goto Retry;
        }
    }
}
