#ifdef dirr3_argh_hh
#include "argh2.h"
#else
#include "argh.h"
#endif

static ParamHandler *Cph = NULL;
void argh_init(void)
{
    Cph = new ParamHandler();
}
void argh_done(void)
{
    delete Cph;
    Cph = NULL;
}
void argh_add_long(const char *longname, int alias)
{
    Cph->AddLong(longname, alias);
}
void argh_add_bool(int c)
{
    Cph->AddBool(c);
}
void argh_add_int(int c, int min, int max)
{
    Cph->AddInt(c, min, max);
}
void argh_add_float(int c, double min, double max)
{
    Cph->AddFloat(c, min, max);
}
void argh_add_string(int c, unsigned minlen, unsigned maxlen)
{
    Cph->AddString(c, minlen, maxlen);
}
void argh_add_desc(int c, const char *s, const char *optparam)
{
    Cph->AddDesc(c, s, optparam?optparam:"");
}
void argh_start_parse(int argc, const char *const *argv)
{
    Cph->StartParse(argc, argv);
}
int argh_get_bool(void)
{
    return Cph->GetBool();
}
int argh_get_int(void)
{
    return Cph->GetInt();
}
double argh_get_float(void)
{
    return Cph->GetFloat();
}
const char *argh_get_string(void)
{
    return Cph->GetString().c_str();
}
int argh_ok(void)
{
    return Cph->ok();
}
int argh_get_param(void)
{
    return Cph->GetParam();
}
void argh_list_options(void)
{
    Cph->ListOptions();
}
