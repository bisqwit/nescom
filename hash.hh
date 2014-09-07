#ifndef bqtHashHH
#define bqtHashHH
/* Set to 0 if you have compilation problems
 * with hash_set or hash_map
 */
#ifndef USE_HASHMAP
#define USE_HASHMAP 0
#endif


#if USE_HASHMAP

#include <string>
#include <utility>

#include <tr1/unordered_map>
#include <tr1/unordered_set>
using namespace __gnu_cxx;

using std::basic_string;

struct BitSwapHashFon
{
  size_t operator() (unsigned n) const
  {
    unsigned rot = n&31;
    return (n << rot) | (n >> (32-rot));
  }
};

namespace BisqHash {
  template<typename T>
  struct hash
  {
    size_t operator() (const T& t) const { return t; }
  };

  template<typename T>
  struct hash<basic_string<T> >
  {
    size_t operator() (const basic_string<T> &s) const
    {
        unsigned h=0;
        for(size_t a=0,b=s.size(); a<b; ++a)
        {
            unsigned c = s[a];
            c=h^c;
            h^=(c*707106);
        }
        return h;
    }
  };
#if 1
#ifndef WIN32
  template<> struct hash<std::wstring>
  {
    size_t operator() (const std::wstring &s) const
    {
        unsigned h=0;
        for(size_t a=0,b=s.size(); a<b; ++a)
        {
            unsigned c = s[a];
            c=h^c;
            h^=(c*707106);
        }
        return h;
    }
  };
#endif
  template<> struct hash<wchar_t>
  {
    size_t operator() (wchar_t n) const
    {
        /* Since values of n<128 are the most common,
         * values of n<256 the second common
         * and big values of n are rare, we rotate some
         * bits to make the distribution more even.
         * Multiplying n by 33818641 (a prime near 2^32/127) scales
         * the numbers nicely to fit the whole range and keeps the
         * distribution about even.
         */
        return (n * 33818641UL);
    }
  };
#endif
  template<> struct hash<unsigned long long>
  {
    size_t operator() (unsigned long long n) const
    {
        return (n * 33818641UL);
    }
  };

  template<typename A,typename B> struct hash<std::pair<A,B> >
  {
     size_t operator() (const std::pair<A,B>& p) const
     {
         return hash<A>() (p.first)
              ^ hash<B>() (p.second);
     }
  };
}

template<typename K,typename V>
class hash_map: public std::tr1::unordered_map<K,V, BisqHash::hash<K> >
{
};
template<typename K>
class hash_set: public std::tr1::unordered_set<K, BisqHash::hash<K> >
{
};
//#define hash_map std::tr1::unordered_map
//#define hash_set std::tr1::unordered_set



#else

#include <map>
#include <set>

#define hash_map std::map
#define hash_multimap std::multimap
#define hash_set std::set
#define hash_multiset std::multiset

#endif // USE_HASHMAP

#endif // bqtHashHH
