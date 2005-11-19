#include <map>
#include <cstdio>
#include <cstring>

using std::strchr;
using std::sprintf;
using std::strtod;

#define ARGH_VERSION_2
#include "argh.hh"

using std::map;
using std::pair;
using std::string;

class argh_aliasmap : public map<string, ParamHandler::keytype>
{
};
class argh_descsmap : public map<ParamHandler::keytype, pair<string,string> >
{
};
class argh_atypemap : public map<ParamHandler::keytype, ParamHandler::ArgInfo>
{
};

ParamHandler::ParamHandler()
{
    aliases  = new argh_aliasmap();
    descs    = new argh_descsmap();
    argtypes = new argh_atypemap();
}
ParamHandler::~ParamHandler()
{
    delete aliases;
    delete descs;
    delete argtypes;
}

int ParamHandler::ShortOpt(ParamHandler::keytype key, const char *s)
{
    argh_atypemap::const_iterator i = argtypes->find(key);
    if(i == argtypes->end())
    {
        if(key=='=' && longo.size())
            ErrorNeedNoArg(longo);
        else
            ErrorIllegalOption(key);
        return ParseError();
    }
    switch(i->second.type)
    {
        case 0: /* bool */
            shortpointer = s;
            return key;
        case 1: /* int */
        {
            int value = 0; bool nega = false;
            if(*s=='=')++s;
            if(!*s && (argpos+1) < argc)s = argv[++argpos];
            if(i->second.min < 0 && *s=='-') { nega = true; ++s; }
            if(s[0] < '0' || s[0] > '9')goto ArgMissing;
            while(s[0] >= '0' && s[0] <= '9')
                value = value*10 + (*s++ - '0');
            if(value < i->second.min || value > i->second.max)
            {
                char Buf[128]; sprintf(Buf, "%d", value); param = Buf;
                goto RangeFailure;
            }
            shortpointer = s;
            intparm = value;
            return key;
        }
        case 2: /* float */
        {
            if(*s=='=')++s;
            if(!*s && (argpos+1) < argc)s = argv[++argpos];
            char *newend;
            doubleparm = strtod(s, &newend);
            if(newend == s)goto ArgMissing;
            if(doubleparm < i->second.fmin || doubleparm > i->second.fmax)
            {
                char Buf[256]; sprintf(Buf, "%g", doubleparm); param = Buf;
                goto RangeFailure;
            }
            shortpointer = newend;
            return key;
        }
        case 3: /* string */
        {
            param = s;
            if(param.size() && param[0]=='=' && longobuf.size())param=s+1;
            else if(!param.size() && (argpos+1) < argc && (argv[argpos+1][0] != '-' || !argv[argpos+1][1]))
                param = argv[++argpos];
            else if(!param.size() && i->second.min > 0)
            {
ArgMissing:     if(longo.size())
                    ErrorNeedsArg(longo);
                else
                    ErrorNeedsArg(key);
                return ParseError();
            }
            if((int)param.size() < i->second.min || param.size() > i->second.maxl)
            {
RangeFailure:   ErrorOutOfRange(param);
                return ParseError();
            }
            shortpointer = "";
            return key;
        }
    }
    InternalError(key, s);
    return key;
}

ParamHandler::keytype ParamHandler::GetParam()
{
    if(error)return -1;
    
Retry:
    polarity = true;
    
    if(shortpointer && *shortpointer)
        return ShortOpt(*shortpointer, shortpointer+1);

    if(++argpos >= argc)return -1;
    
    /* new arg */
    const char *s = argv[argpos];
    if(!opts || *s!='-' || !s[1])
    {
        param = s;
        return 0;
    }
    
    longo = "";
    longobuf = "";
    
    if(s[1] != '-')
        return ShortOpt(s[1], s+2);

    if(!s[2]) /* -- */
    {
        opts = false;
        goto Retry;
    }
    
    /* --something */
    longo = s+2;
    bool nega = false, negafail = false;
    if(longo.substr(0, 4) == "not-")          { nega=true; longo.erase(0, 4); }
    else if(longo.substr(0, 3) == "no-")      { nega=true; longo.erase(0, 3); }
    else if(longo.substr(0, 8) == "without-") { nega=true; longo.erase(0, 8); }
    else if(longo.substr(0, 5) == "with-")   { nega=false; longo.erase(0, 5); }
    
NegaDone:
    unsigned p = longo.find('=');
    if(p == longo.npos)p = longo.find(':');
    string option;
    if(p != longo.npos) {option=longo.substr(p+1);longo.erase(p);}
    argh_aliasmap::const_iterator i = aliases->find(longo);
    if(i == aliases->end())
    {
        if(nega) { nega=false; longo=s+2; goto NegaDone; }
        
        ErrorUnknownOption(s, negafail);
        return ParseError();
    }
    if(nega)
    {
        argh_atypemap::const_iterator j = argtypes->find(i->second);
        if(j != argtypes->end() && j->second.type != 0)
        {
            negafail = true;
            nega = false;
            longo = s+2;
            goto NegaDone;
        }
        polarity = false;
    }
    longobuf = "";
    if(option.size()) { longobuf += '='; longobuf += option; }
    return ShortOpt(i->second, longobuf.c_str());
}

#include <vector>
using std::vector;

void ParamHandler::ListOptions()
{
    argh_atypemap::const_iterator i;
    argh_aliasmap::const_iterator j;
    argh_descsmap::const_iterator k;
    
    vector<pair<string,string> > hdrs;
    unsigned widest=0;

    for(j=aliases->begin(); j!=aliases->end(); ++j)
        if(argtypes->find(j->second) == argtypes->end())
        {
            k = descs->find(j->second);
            if(k == descs->end())continue;

            string s = "    --";
            s += j->first;
            if(k->second.second.size())
            {
                s += ' ';
                s += k->second.second;
            }
            if(s.size() > widest)widest = s.size();
            hdrs.push_back(pair<string,string> (s, k->second.first));
        }
    
    for(i=argtypes->begin(); i!=argtypes->end(); ++i)
    {
        k = descs->find(i->first);
        if(k == descs->end())continue;
        
        string s;
        
        bool did = false;
        
        if(i->first < 256)
        {
        	s += "-";
        	s += i->first;
        	did = true;
        }

        for(j=aliases->begin(); j!=aliases->end(); ++j)
            if(j->second == i->first)
            {
                if(did) s += ", ";
                s += "--";
                s += j->first;
                did = true;
            }
        if(k->second.second.size())
        {
            s += ' ';
            s += k->second.second;
        }
        if(s.size() > widest)widest = s.size();
        hdrs.push_back(pair<string,string> (s, k->second.first));
    }
    
    for(unsigned a=0; a<hdrs.size(); ++a)
        PrintOpt(widest, hdrs[a].first, hdrs[a].second);
}

ParamHandler::Reference ParamHandler::AddLong(const string &longname, ParamHandler::keytype alias)
{
    (*aliases)[longname]=alias;
    return MakeRef(alias);
}

ParamHandler::Reference ParamHandler::AddDesc(ParamHandler::keytype c, const string &s, const string &param)
{
    (*descs)[c] = pair<string,string>(s,param);
    return MakeRef(c);
}

ParamHandler::Reference ParamHandler::AddBool(ParamHandler::keytype c)
{
    (*argtypes)[c].type=0;
    return MakeRef(c);
}

ParamHandler::Reference ParamHandler::AddInt(ParamHandler::keytype c, int min, int max)
{
    ArgInfo&a=(*argtypes)[c];a.type=1;a.min=min;a.max=max;
    return MakeRef(c);
}

ParamHandler::Reference ParamHandler::AddFloat(ParamHandler::keytype c, double min, double max)
{
    ArgInfo&a=(*argtypes)[c];a.type=2;a.fmin=min;a.fmax=max;
    return MakeRef(c);
}

ParamHandler::Reference ParamHandler::AddString(ParamHandler::keytype c, unsigned min, unsigned max)
{
    ArgInfo&a=(*argtypes)[c];a.type=3;a.min=min;a.maxl=max;
    return MakeRef(c);
}

void ParamHandler::StartParse(int ac, const char *const *av, int firstarg)
{
    argc=ac;argv=av;
    argpos=firstarg-1;opts=true;
    longo="";shortpointer="";
    polarity=true;
    error=false;

    A0 = argv[0];
    for(const char *q; (q = strchr(A0, '/')); )A0 = q+1;
}
void ParamHandler::StartParse(int ac, const char **av, int firstarg)
{
	StartParse(ac, const_cast<const char*const *> (av), firstarg);
}
void ParamHandler::StartParse(int ac, char **av, int firstarg)
{
	StartParse(ac, const_cast<const char*const *> (av), firstarg);
}
void ParamHandler::StartParse(int ac, char *const *av, int firstarg)
{
	StartParse(ac, const_cast<const char*const *> (av), firstarg);
}

#define reffunc(fun, args, fun2, args2) \
  ParamHandler::Reference &ParamHandler::Reference::fun args \
  { \
      par.fun2 args2; \
      return *this; \
  }

reffunc(SetBool, (), AddBool, (key))
reffunc(SetInt, (int min, int max), AddInt, (key, min, max))
reffunc(SetFloat, (double min, double max), AddFloat, (key, min, max))
reffunc(SetString, (unsigned min, unsigned max), AddString, (key, min, max))
reffunc(SetDesc, (const string &s, const string &param), AddDesc, (key, s, param))

#undef reffunc

int ParamHandler::ParseError()
{
    error=true;
    return -1;
}

ParamHandler::Reference ParamHandler::MakeRef(ParamHandler::keytype key)
{
    return Reference(*this, key);
}

using std::printf;
using std::fprintf;
using std::fflush;

void ParamHandler::ErrorIllegalOption(ParamHandler::keytype key)
{
    fprintf(stderr, "%s: illegal option -- %c\n", A0, (int)key);
}

void ParamHandler::ErrorNeedsArg(const string &longo)
{
    fprintf(stderr, "%s: Switch `--%s' requires an argument.\n", A0, longo.c_str());
}

void ParamHandler::ErrorNeedsArg(ParamHandler::keytype key)
{
    fprintf(stderr, "%s: Option `-%c' requires an argument.\n", A0, (int)key);
}

void ParamHandler::InternalError(ParamHandler::keytype key, const char *s)
{
    fprintf(stderr, "%s: Internal error when parsing key '%ld' (%c), s='%s'\n", A0, key, (int)key, s);
}

void ParamHandler::ErrorOutOfRange(const string &param)
{
    fprintf(stderr, "%s: parameter length/value out of range `%s'\n", A0, param.c_str());
}

void ParamHandler::ErrorUnknownOption(const char *s, bool negafail)
{
    fprintf(stderr, "%s: unrecognized option: `%s'", A0, s);
    if(negafail)fprintf(stderr, " (can only negate boolean switches)");
    fprintf(stderr, "\n");
}

void ParamHandler::ErrorNeedNoArg(const string &longo)
{
    fprintf(stderr, "%s: Parameter error: `--%s' does not take a value.\n", A0, longo.c_str());
}

void ParamHandler::PrintOpt(unsigned space, const string &opt, const string &desc)
{
    printf("    %-*s  ", space, opt.c_str());
    
    bool needeol = true;
    for(unsigned a=0; a < desc.size(); )
    {
        unsigned b = desc.find('\n', a);
        if(!needeol){printf("%*s", space+6, "");needeol=true;}
        if(b == desc.npos) { printf("%s", desc.c_str()+a); break; }
        printf("%s", desc.substr(a, b-a).c_str());
        printf("\n"); needeol = false;
        a = b+1;
    }
    if(needeol)printf("\n");
    fflush(stdout);
}

#ifdef NEED_ARGH_H
#include "argh-c.inc"
#endif
