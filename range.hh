#ifndef bqt_RangeHH
#define bqt_RangeHH

template<typename Key>
struct rangetype
{
    /* lower = begin of the range, inclusive */
    /* upper = end of the range, exclusive */
    /* Example range: 0-5 includes the numbers 0,1,2,3,4 but not 5. */
    /* Reverse ranges are not allowed. */
    Key lower, upper;
    
    /* Compareoperators. Without these we can't belong into a std::set or std::map. */
    bool operator< (const rangetype& b) const
    { return lower!=b.lower?lower<b.lower
                           :upper<b.upper; }
    bool operator==(const rangetype& b) const
    { return lower==b.lower&&upper==b.upper; }
    
    /* Public accessory functions */
    bool coincides(const rangetype& b) const
    {
        return lower < b.upper && upper > b.lower;
    }
    bool contains(const Key& v) const { return lower <= v && upper > v; }

    unsigned length() const { return upper - lower; }
};

#endif
