#ifndef bqtRangeSetHH
#define bqtRangeSetHH

#include <set>
#include "range.hh"

/***************
 *
 * The idea of a rangeset is that you don't need to have
 * a std::vector<bool> of 2 gigabytes size or a std::set<>
 * with 1000000 elements.
 * It is especially ideal for structures that have large
 * consequent blocks of similar setting.
 *
 * Another idea of implementing this would be to store changepoints
 * only.
 * i.e., for a rangeset that has 0..5, 12..15 and 20..25,
 * you would have a set with 0,12,20 and another with 5,15,25.
 */
template<typename Key>
class rangeset
{
    typedef rangetype<Key> range;
    typedef std::set<rangetype<Key> > Cont;
    Cont data;
public:
    typedef typename Cont::const_iterator const_iterator;
    typedef typename Cont::iterator iterator;

    rangeset() : data() {}
    
    /* Erase everything between the given range */
    void erase(const Key& lo, const Key& up);
    
    /* Erase single value */
    void erase(const Key& value);
    
    /* Set a range */
    void set(const Key& lo, const Key& up);
    
    /* Set a single value */
    void insert(const Key& value);
    
    /* Find the range that has this value */
    const_iterator find(const Key& lo) const;
    
    /* Standard functions */
    const_iterator begin() const { return data.begin(); }
    const_iterator end() const { return data.end(); }
    unsigned size() const { return data.size(); }
    bool empty() const { return data.empty(); }
    void clear() { data.clear(); }
    
#if 0
    template<typename Listtype>
    void find_all_coinciding(const Key& lo, const Key& up,
                             Listtype& target);
#endif
    
    /* Optimization function */
    void compact();
    
    // default copy cons. and assign-op. are fine
};

#include "rangeset.tcc"

#endif
