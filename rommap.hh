#ifndef bqtROMmapHH
#define bqtROMmapHH

/* Dummy rommap.hh for space.cc */

inline void MarkFree(unsigned,unsigned,const char*){}
inline unsigned ROM2SNESaddr(unsigned a) { return a | 0xC00000; }

#endif
