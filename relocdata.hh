#ifndef RelocDataDefined
#define RelocDataDefined

#include <vector>
#include <utility>
#include <algorithm>

enum SegmentSelection;

template<typename VarType>
class Relocdata
{
private:
    template<typename T>
    class RT
    {
    public:
        typedef T                        Type;
        typedef SegmentSelection         SegType;
        
        typedef std::pair<SegType, Type> FixupType;
        typedef std::pair<Type, VarType> RelocType;
        
        typedef std::vector<FixupType> FixupList;
        typedef std::vector<RelocType> RelocList;
        
        FixupList Fixups;
        RelocList Relocs;
    public:
        void AddFixup(SegType s, Type addr) { Fixups.push_back(FixupType(s, addr)); }
        void AddReloc(Type addr, const VarType& s) { Relocs.push_back(RelocType(addr, s)); }
        
        RT(): Fixups(), Relocs() { }
        
        void sort()
        {
            /*
              Fixups are sorted according to seg, then addr, then low/offpart
              Relocs are sorted according to addr, then low/offpart, then symbol index
              Primary use is to make binary searching work.
            */
            std::sort(Fixups.begin(), Fixups.end());
            std::sort(Relocs.begin(), Relocs.end());
        }
    };
    
    typedef std::pair<unsigned,unsigned> r2_t;
    
public:
    // too lazy to make handler-functions for all these
    
    typedef RT<unsigned> R16_t;    R16_t R16;       // addr
    typedef RT<unsigned> R16lo_t;  R16lo_t R16lo;   // addr
    typedef RT<r2_t>     R16hi_t;  R16hi_t R16hi;   // addr,lowpart
    typedef RT<r2_t>     R24seg_t; R24seg_t R24seg; // addr,offspart
    typedef RT<unsigned> R24_t;    R24_t R24;       // addr
    
    Relocdata(): R16(), R16lo(), R16hi(), R24seg(), R24() { }
    
    void clear() { R16.clear(); R16lo.clear(); R16hi.clear(); R24seg.clear(); R24.clear(); }
    void sort() { R16.sort(); R16lo.sort(); R16hi.sort(); R24seg.sort(); R24.sort(); }
};

#endif
