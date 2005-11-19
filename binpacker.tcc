// Bisqwit's 1d Bin Packing algorithm implementation (C++).
// 
// Copyright (C) 1992,2003 Bisqwit (http://iki.fi/bisqwit/)
//
// Read binpacker.hh .

#define BINPACKER_DUMP       0
#define BINPACKER_DUMP_ITEMS 0

#include <set>
#include <algorithm>

#if BINPACKER_DUMP
#include <iostream>
#include <iomanip>
#endif

namespace
{
    template<typename sizetype>
    class BinPacker
    {
        static const unsigned BinPackerNowhere = static_cast<unsigned>(-1);
    public:
        BinPacker
        (
           // Input: List of holes. Value = hole size
           const std::vector<sizetype> &Holes,
           // Input: List of items. Value = item size
           const std::vector<sizetype> &Items
        );
        
        const std::vector<unsigned> GetResult() const;

#if BINPACKER_DUMP
        void Dump() const;
#endif
        bool IsGood() const;
    private:
        struct hole
        {
            sizetype size, used;
            std::set<unsigned> contents;
        };
        struct item
        {
            unsigned index, location;
            sizetype size;
            bool operator< (const item &b) const { return size > b.size; }
        };
        std::vector<hole> holes;
        std::vector<item> items;
        
        void MoveItem(unsigned itemno, unsigned holeno);
        double GetCapacity(unsigned holeno) const // in %
        {
            return holes[holeno].used * 100.0 / holes[holeno].size;
        }
        sizetype GetHoleSize(unsigned holeno) const {  return holes[holeno].size; }
        sizetype GetItemSize(unsigned itemno) const {  return items[itemno].size; }
        unsigned GetItemLocation(unsigned itemno) const { return items[itemno].location; }
        
        void Shuffle();
    };

    template<typename sizetype>
    BinPacker<sizetype>::BinPacker
    (
       const std::vector<sizetype> &Holes,
       const std::vector<sizetype> &Items
    ) : holes(Holes.size()),
        items(Items.size())
    {
        for(unsigned a=0; a<Items.size(); ++a)
        {
            items[a].index    = a;
            items[a].size     = Items[a];
            items[a].location = BinPackerNowhere;
        }
        for(unsigned a=0; a<Holes.size(); ++a)
        {
            holes[a].size     = Holes[a];
            holes[a].used     = 0;
        }
        Shuffle();
    }

    template<typename sizetype>
    const std::vector<unsigned> BinPacker<sizetype>::GetResult() const
    {
        std::vector<unsigned> Result(items.size());
        
        for(unsigned a=0; a<items.size(); ++a)
            Result[items[a].index] = items[a].location;

        return Result;
    }

    template<typename sizetype>
    bool BinPacker<sizetype>::IsGood() const
    {
        for(unsigned a=0; a<holes.size(); ++a)
            if(holes[a].used > holes[a].size)
                return false;
        return true;
    }

    template<typename sizetype>
    void BinPacker<sizetype>::MoveItem(unsigned itemno, unsigned holeno)
    {
        // If the item is already in the requested hole, do nothing
        if(items[itemno].location == holeno) return;
        
        unsigned oldhole = items[itemno].location;
        if(oldhole != BinPackerNowhere)
        {
            // The old hole now has more space
            holes[oldhole].used -= items[itemno].size;
            // And doesn't own this item
            holes[oldhole].contents.erase(itemno);
        }
        
        items[itemno].location = holeno;
        if(holeno != BinPackerNowhere)
        {
            // The new hole now has less space
            holes[holeno].used += items[itemno].size;
            // And has this item
            holes[holeno].contents.insert(itemno);
        }
    }

    template<typename sizetype>
    void BinPacker<sizetype>::Shuffle()
    {
        if(holes.empty()) return;
        
        sort(items.begin(), items.end());
        
        for(unsigned a=0; a<items.size(); ++a)
        {
            unsigned besthole=0;
            sizetype bestfree=0;
            bool first=true;
            for(unsigned b=0; b<holes.size(); ++b)
            {
                if(holes[b].used+items[a].size > holes[b].size) continue;
                sizetype free = holes[b].size - holes[b].used;
                
                /* free = holes.size()-b;
                 * ^ use to prefer last holes
                 */
                
                if(first || free < bestfree)
                {
                    first = false;
                    besthole = b;
                    bestfree = free;
                }
            }
            MoveItem(a, besthole);
        }
        if(IsGood()) return; // No need to push further
        
    }

#if BINPACKER_DUMP
    template<typename sizetype>
    void BinPacker<sizetype>::Dump() const
    {
        for(unsigned a=0; a<holes.size(); ++a)
        {
            std::cerr << "  Hole(size(" << GetHoleSize(a)
                      << ")used(" << GetCapacity(a)
                      << "%)";
#if BINPACKER_DUMP_ITEMS
            std::cerr << "items(";
            bool first=true,firstout=true;
            sizetype lastitemsize=0;
            unsigned itemcount=0;
            for(std::set<unsigned>::const_iterator
                i = holes[a].contents.begin();
                i != holes[a].contents.end();
                ++i)
            {
                sizetype size = GetItemSize(*i);
                if(!first && lastitemsize != size)
                {
                    if(firstout)firstout=false;else std::cerr << ',';
                    std::cerr << lastitemsize;
                    if(itemcount != 1)std::cerr << '*' << itemcount;
                }
                if(first || lastitemsize != size)
                {
                    itemcount = 0; lastitemsize = size;
                    first = false;
                }
                ++itemcount;
            }
            if(!first)
            {
                if(!firstout)std::cerr << ',';
                std::cerr << lastitemsize;
                if(itemcount != 1)std::cerr << '*' << itemcount;
            }
            std::cerr << ')';
#endif
            std::cerr << ")\n";
        }
    }
#endif
}

template<typename sizetype>
const std::vector<unsigned> PackBins
   (const std::vector<sizetype> &Bins,
    const std::vector<sizetype> &Items)
{
    BinPacker<sizetype> packer(Bins, Items);
#if BINPACKER_DUMP
    if(!packer.IsGood()) packer.Dump();
#endif
    return packer.GetResult();
}
