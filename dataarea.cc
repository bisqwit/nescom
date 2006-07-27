#include "dataarea.hh"

namespace
{
    static const unsigned char Empty = 0;
}

void DataArea::PrepareVec(vec& v, unsigned offset)
{
    if(offset >= v.size())
    {
        unsigned newsize = offset+1;
        if(v.capacity() < newsize)
        {
            const unsigned step = 2048;
            unsigned newcapacity = (newsize + step) &~ (step-1);
            v.reserve(newcapacity);
        }
        v.resize(newsize);
    }
}

DataArea::map::iterator DataArea::GetRef(unsigned offset)
{
    map::iterator i = blobs.upper_bound(offset);
    if(i == blobs.begin()) i = blobs.end(); else --i;
    
    if(i != blobs.end())
    {
        unsigned prev_top = i->first + i->second.size();
        if(prev_top >= offset)
        {
            return i;
        }
    }
    
    return blobs.insert(std::make_pair(offset, vec())).first;
}

void DataArea::WriteByte(unsigned pos, unsigned char byte)
{
/*    
    fprintf(stderr, "GetRef($%X,$%02X) ", pos,byte);
*/
    map::iterator i = GetRef(pos);
    vec& vector   = i->second;
    unsigned base = i->first;
/*
    fprintf(stderr, "-> %u bytes at $%X\n", vector.size(), base);
*/
    unsigned vecpos = pos - base;
    PrepareVec(vector, vecpos);
    vector[vecpos] = byte;
    
    Optimize(i);
}

void DataArea::WriteLump(unsigned pos, const std::vector<unsigned char>& lump)
{
    if(lump.empty()) return;
    
    /* This is slow! Please invent a better algorithm some day. */
    for(unsigned a=0; a<lump.size(); ++a)
        WriteByte(pos+a, lump[a]);
}

unsigned char DataArea::GetByte(unsigned pos) const
{
    map::const_iterator i = blobs.upper_bound(pos);
    if(i == blobs.begin()) i = blobs.end(); else --i;
    
    if(i != blobs.end())
    {
        unsigned prev_top = i->first + i->second.size();
        if(prev_top > pos)
        {
            unsigned base = i->first;
            return i->second[pos - base];
        }
    }
    /* Nothing found. */
    return Empty;
}

unsigned DataArea::GetBase() const
{
    map::const_iterator i = blobs.begin();
    if(i == blobs.end()) return 0;
    return i->first;
}

unsigned DataArea::GetTop() const
{
    map::const_iterator i = blobs.end();
    if(i == blobs.begin()) return 0;
    --i;
    return i->first + i->second.size();
}

const std::vector<unsigned char> DataArea::GetContent() const
{
    std::vector<unsigned char> result(GetSize(), Empty);
    const unsigned base = GetBase();
    for(map::const_iterator i = blobs.begin(); i != blobs.end(); ++i)
    {
        unsigned pos = i->first - base;
        std::copy(i->second.begin(), i->second.end(), result.begin()+pos);
    }
    return result;
}

const std::vector<unsigned char> DataArea::GetContent(unsigned begin, unsigned size) const
{
    std::vector<unsigned char> result(size, Empty);

    for(map::const_iterator i = blobs.begin(); i != blobs.end(); ++i)
    {
        /* Source */
        unsigned begin_offset = i->first;
        /* Size of block */
        unsigned count  = i->second.size();
        /* Where would this go to in the target */
        unsigned target = begin_offset - begin;

/*
        fprintf(stderr, "Blob at $%X is %u bytes\n", i->first, i->second.size());
*/
        
        /* If we don't want the first bytes of this block */
        if(begin > begin_offset)
        {
            /* Skip some bytes from the beginning of this block. */
            unsigned decrease = begin - begin_offset;
            /* If the length of skip is bigger than this block, there's nothing to do */
            if(decrease >= count) continue;
            /* Less bytes left */
            count -= decrease;
            /* Offset is now even */
            begin_offset = begin;
            /* Target is now even */
            target = 0;
        }
        /* If this goes past what we want, do nothing */
        if(target >= size)
        {
            continue;
        }
        
        /* Ensure we don't copy too much */
        if(count > size-target)
        {
            count = size-target;
        }
        
        unsigned pos = begin_offset - i->first;

/*
        fprintf(stderr,
            "GetContent($%X,%u): Copying %u bytes from %u (base $%X) to %u (base $%X)\n",
                begin,size,
                count,
                pos, i->first,
                target, begin);
*/

        std::copy(i->second.begin() + pos,
                  i->second.begin() + pos + count,
                  result.begin() + target);
    }
    return result;
}

unsigned DataArea::FindNextBlob(unsigned where, unsigned& length) const
{
    map::const_iterator i = blobs.lower_bound(where);
    if(i == blobs.end()) { length = 0; return 0; }
    length = i->second.size();
    return i->first;
}

unsigned DataArea::GetUtilization(unsigned begin, unsigned size) const
{
    unsigned result = 0;
    
    /* Algorithm copied from GetContent(..., ...) */
    
    for(map::const_iterator i = blobs.begin(); i != blobs.end(); ++i)
    {
        /* Source */
        unsigned begin_offset = i->first;
        /* Size of block */
        unsigned count  = i->second.size();
        /* Where would this go to in the target */
        unsigned target = begin_offset - begin;

        /* If we don't want the first bytes of this block */
        if(begin > begin_offset)
        {
            /* Skip some bytes from the beginning of this block. */
            unsigned decrease = begin - begin_offset;
            /* If the length of skip is bigger than this block, there's nothing to do */
            if(decrease >= count) continue;
            /* Less bytes left */
            count -= decrease;
            /* Offset is now even */
            begin_offset = begin;
            /* Target is now even */
            target = 0;
        }
        /* If this goes past what we want, do nothing */
        if(target >= size)
        {
            continue;
        }
        
        /* Ensure we don't copy too much */
        if(count > size-target)
        {
            count = size-target;
        }
        
        result += count;
    }
    return result;
}

void DataArea::Optimize(map::iterator i)
{
    /* This function joins consequent blocks. */
    
    if(i != blobs.end())
    {
        /* Check if the next block follows this block immediately. */
        map::iterator j = i; ++j;
        while(j != blobs.end() && j->first == i->first + i->second.size())
        {
            i->second.insert(i->second.end(),
                             j->second.begin(), j->second.end());
            map::iterator k = j; ++k;
            blobs.erase(j);
            j = k;
            /* Check if the next block follows too */
        }
    }
}
