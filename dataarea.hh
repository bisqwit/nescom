#ifndef bqt65asmDataAreaHH
#define bqt65asmDataAreaHH

#include <map>
#include <vector>

class DataArea
{
    typedef std::vector<unsigned char> vec;
    typedef std::map<unsigned, vec> map;
    map blobs;
private:
    map::iterator GetRef(unsigned offset);
    static void PrepareVec(vec&, unsigned offset);
    
    void Optimize(map::iterator i);
public:
    DataArea(): blobs() { }
    
    void WriteByte(unsigned pos, unsigned char byte);
    void WriteLump(unsigned pos, const std::vector<unsigned char>& lump);
    
    unsigned char GetByte(unsigned pos) const;
    
    unsigned GetBase() const;
    unsigned GetTop() const;
    unsigned GetSize() const { return GetTop() - GetBase(); }

    unsigned FindNextBlob(unsigned where, unsigned& length) const;
    
    /* Returns the number of bytes that actually exist within the given range. */
    unsigned GetUtilization(unsigned begin, unsigned size) const;

    const std::vector<unsigned char> GetContent() const;
    
    const std::vector<unsigned char> GetContent(unsigned begin, unsigned size) const;
};

#endif
