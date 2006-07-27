#ifndef ctMiscFunHH
#define ctMiscFunHH

#include <string>

using std::basic_string;

template<typename CharT>
const basic_string<CharT>
str_replace(const basic_string<CharT>& search,
            CharT with,
            const basic_string<CharT>& where);

template<typename CharT>
const basic_string<CharT>
str_replace(const basic_string<CharT>& search,
            const basic_string<CharT>& with,
            const basic_string<CharT>& where);

template<typename CharT>
void
str_replace_inplace(basic_string<CharT>& where,
                    const basic_string<CharT>& search,
                    CharT with);

template<typename CharT>
void
str_replace_inplace(basic_string<CharT>& where,
                    const basic_string<CharT>& search,
                    const basic_string<CharT>& with);


inline void
str_replace_inplace(std::string& a, const std::string& b, const std::string& c)
{
    str_replace_inplace<char> (a,b,c);
}


inline void
str_replace_inplace(std::wstring& a, const std::wstring& b, const std::wstring& c)
{
    str_replace_inplace<wchar_t> (a,b,c);
}

/* sprintf() */
inline const std::string format(const char* fmt, ...);



#include "miscfun.tcc"

#endif
