#ifndef bqtO65hh
#define bqtO65hh

/* xa65 object file loader and linker for C++
 * For loading and linking 65816 object files
 * Copyright (C) 1992,2016 Bisqwit (http://iki.fi/bisqwit/)
 *
 * Version 1.9.2 - Aug 18 2003, Sep 4 2003, Jan 23 2004,
 *                 Jan 31 2004, Feb 18 2005, Mar 27 2016
 */

#include <cstdio>
#include <vector>
#include <string>
#include <utility>

using std::FILE;
using std::vector;
using std::pair;

/* An xa65 object file loader */

/*
 * Features:
 *
 *    Loading TEXT segment
 *    Relocating the TEXT segment
 *    Acquiring symbol pointers
 *    Defining the undefined numeric constants
 *    Defining the undefined symbol addresses
 *    Five type of fixups and relocs: 16bit, 16/hi, 16/lo, long, seg
 *
 * Defects:
 *
 *    Only the TEXT segment is handled.
 *    File validity is not checked carefully.
 *    Undefined symbols may only be defined once.
 *
 */

enum SegmentSelection
{
    CODE=2,
    DATA=3,
    ZERO=5,
    BSS=4
};
const std::string GetSegmentName(const SegmentSelection seg);

#include "relocdata.hh"

/**
 * O65 object class.
 */
class O65
{
public:
    O65();
    ~O65();

    // Copy constructor, assignment operator
    O65(const O65 &);
    const O65& operator= (const O65 &);

    /*! Loads an object file from the specified file */
    void Load(FILE *fp);

    /*! Relocate the given segment to new address */
    void Locate(SegmentSelection seg, unsigned newaddress);

    /*! Returns the base address of the given segment */
    unsigned GetBase(SegmentSelection seg) const;

    /*! Defines the value of a symbol. */
    /*! The symbol must have been accessed in order to be defined. */
    void LinkSym(const std::string& name, unsigned value);

    /*! Declares a global label in the selected segment */
    void DeclareGlobal(SegmentSelection seg, const std::string& name, unsigned address);

    /*! Declares a 8-bit relocation to given symbol */
    void DeclareByteRelocation(SegmentSelection seg, const std::string& name, unsigned addr);
    void DeclareHiByteRelocation(SegmentSelection seg, const std::string& name, unsigned addr);
    /*! Declares a 16-bit relocation to given symbol */
    void DeclareWordRelocation(SegmentSelection seg, const std::string& name, unsigned addr);
    /*! Declares a 24-bit relocation to given symbol */
    void DeclareLongRelocation(SegmentSelection seg, const std::string& name, unsigned addr);

    /*! Returns the contents of a segment */
    const vector<unsigned char>& GetSeg(SegmentSelection seg) const;
    const vector<pair<unsigned char, std::string> >& GetCustomHeaders() const;

    /*! Returns the segment size */
    unsigned GetSegSize(SegmentSelection seg) const;

    /*! Returns the address of a global */
    unsigned GetSymAddress(SegmentSelection seg, const std::string& name) const;

    /*! Resizes a segment */
    void Resize(SegmentSelection seg, unsigned newsize);

    /*! Write to segment. Warning: no range checks */
    void Write(SegmentSelection seg, unsigned addr, unsigned char value);

    /*! Redefine a segment. Warning: Does not change symbols. */
    void LoadSegFrom(SegmentSelection seg, const vector<unsigned char>& buf);

    bool HasSym(SegmentSelection seg, const std::string& name) const;

    const vector<std::string> GetSymbolList(SegmentSelection seg) const;
    const vector<std::string> GetExternList() const;

    /*! Verifies that all symbols have been properly defined */
    void Verify() const;

    /*! Has an error been found? */
    bool Error() const;

    /*! Set error flag */
    void SetError();

    /*! Get relocation data of the given segment */
    const Relocdata<unsigned> GetRelocData(SegmentSelection seg) const;

private:
    class Defs;
    class Segment;

    vector<pair<unsigned char, std::string> > customheaders;

    Defs *defs;
    Segment *code, *data, *zero, *bss;

    Segment** GetSegRef(SegmentSelection seg);
    const Segment*const * GetSegRef(SegmentSelection seg) const;

    bool error;
};

#endif
