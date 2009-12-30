#ifndef bqt_RangeHH
#define bqt_RangeHH

#include <map>

template<typename Key>
struct rangetype
{
    /* lower = begin of the range, inclusive */
    /* upper = end of the range, exclusive */
    /* Example range: 0-5 includes the numbers 0,1,2,3,4 but not 5. */
    /* Reverse ranges are not allowed. */
    Key lower, upper;

    rangetype(): lower(), upper() { }

    /* Compareoperators. Without these we can't belong into a std::set or std::map. */
    bool operator< (const rangetype& b) const
    { return lower!=b.lower?lower<b.lower
                           :upper<b.upper; }
    bool operator==(const rangetype& b) const
    { return lower==b.lower&&upper==b.upper; }

    bool operator!=(const rangetype& b) const { return !operator==(b); }

    /* Public accessory functions */
    bool coincides(const rangetype& b) const
    {
        return lower < b.upper && upper > b.lower;
    }
    bool contains(const Key& v) const { return lower <= v && upper > v; }

    rangetype<Key> intersect(const rangetype& b) const;

    /* Union and difference can produce two ranges. Thus not implemented... */

    unsigned length() const { return upper - lower; }
    bool empty() const { return !length(); }
};


template<typename Key, typename Valueholder, typename Allocator = std::allocator<Key> >
class rangecollection
{
    typedef std::map<Key, Valueholder, std::less<Key>,
      typename Allocator::template rebind<std::pair<Key, Valueholder> >::other
                    > Cont;
    Cont data;
public:
    rangecollection(): data() {}

    template<typename Valuetype>
    void set(const Key& lo, const Key& up, const Valuetype& val);
    void erase(const Key& lo, const Key& up);

    void erase_before(const Key& lo);
    void erase_after(const Key& up);

    typedef typename Cont::const_iterator const_iterator;

    const const_iterator begin() const { return data.begin(); }
    const const_iterator end() const   { return data.end(); }
    const const_iterator lower_bound(const Key& k) const   { return data.lower_bound(k); }
    const const_iterator upper_bound(const Key& k) const   { return data.upper_bound(k); }
    typename Cont::size_type size() const { return data.size(); }
    bool empty() const { return data.empty(); }
    void clear() { data.clear(); }

    /* flip() inverts the range within the given range.
     * However, it is not yet implemented! */
    void flip(const Key& floor, const Key &ceil);

    const const_iterator find(const Key& v) const;

    bool operator==(const rangecollection& b) const { return data == b.data; }
    bool operator!=(const rangecollection& b) const { return !operator==(b); }
};

#include "range.tcc"

#endif
