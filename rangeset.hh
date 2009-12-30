#ifndef bqtRangeSetHH
#define bqtRangeSetHH

#include "range.hh"

/***************
 *
 * The idea of a rangeset is that you don't need to have
 * a vector<bool> of a gigabyte size.
 *
 * Implemented using changepoints.
 */
template<typename Key, typename Allocator = std::allocator<Key> >
class rangeset
{
    class Valueholder
    {
        bool nil;
    public:
        Valueholder(bool set=false): nil(!set) {}
        void set() { nil=false; }
        void clear() { nil=true; }
        bool is_nil() const { return nil; }
        bool operator==(const Valueholder& b) const { return nil==b.nil; }
        bool operator!=(const Valueholder& b) const { return nil!=b.nil; }
    };
    typedef rangecollection<Key, Valueholder, Allocator> Cont;
    Cont data;

public:
    /* Iterates over _set_ ranges */
    struct const_iterator: public rangetype<Key>
    {
        const const_iterator* operator-> () const { return this; }
        typename Cont::const_iterator i;
    public:
        const_iterator(const Cont& c): i(), data(c) { }
        const_iterator(const const_iterator& b) : i(b.i), data(b.data) { }

        bool operator==(const const_iterator& b) const { return i == b.i; }
        bool operator!=(const const_iterator& b) const { return !operator==(b); }
        const_iterator& operator++ ();
        const_iterator& operator-- ();

    private:
        const Cont& data;
        void Reconstruct();
        friend class rangeset;
    };
private:
    const const_iterator ConstructIterator(typename Cont::const_iterator i) const;

public:
    rangeset() : data() {}

    /* Erase everything between the given range */
    void erase(const Key& lo, const Key& up) { data.erase(lo, up); }

    /* Erase a single value */
    void erase(const Key& lo) { data.erase(lo, lo+1); }

    void erase_before(const Key& lo) { data.erase_before(lo); }
    void erase_after(const Key& up) { data.erase_after(up); }

    /* Modify the given range to have the given value */
    void set(const Key& lo, const Key& up) { data.set(lo, up, true); }

    void insert(const Key& pos) { set(pos, pos+1); }

    rangeset intersect(const rangeset& b) const;

    /* Find the range that has this value */
    const_iterator find(const Key& v) const { return ConstructIterator(data.find(v)); }

    /* Standard functions */
    const_iterator begin() const { return ConstructIterator(data.begin()); }
    const_iterator end() const { return ConstructIterator(data.end()); }
    const_iterator lower_bound(const Key& v) const { return ConstructIterator(data.lower_bound(v)); }
    const_iterator upper_bound(const Key& v) const { return ConstructIterator(data.upper_bound(v)); }
    unsigned size() const { return data.size(); }
    bool empty() const { return data.empty(); }
    void clear() { data.clear(); }

    bool operator==(const rangeset& b) const { return data == b.data; }
    bool operator!=(const rangeset& b) const { return !operator==(b); }

};

#include "rangeset.tcc"

#endif
