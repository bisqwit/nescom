#ifndef optrar_argh_h
#define optrar_argh_h

/* Copyright (C) 1992,2004 Bisqwit (http://iki.fi/bisqwit/) */

#ifdef __cplusplus
extern "C" {
#endif

extern void argh_init(void);
extern void argh_done(void);
extern void argh_add_long(const char *longname, int alias);
extern void argh_add_bool(int alias);
extern void argh_add_int(int c, int min, int max);
extern void argh_add_float(int c, double min, double max);
extern void argh_add_string(int c, unsigned minlen, unsigned maxlen);
extern void argh_add_desc(int c, const char *s, const char *optparam);
extern void argh_start_parse(int argc, const char *const *argv);
extern int argh_get_bool(void);
extern int argh_get_int(void);
extern double argh_get_float(void);
extern int argh_ok(void);
extern int argh_get_param(void);
extern void argh_list_options(void);
/* Note: This pointer is only valid until next argh_get_param() call */
/* Attention: Do not try to free() it. */
extern const char *argh_get_string(void);

#ifdef __cplusplus
}
#endif

/* C language usage example: 

static int stereo=0;
static int Rate  =43200;
static char *host = NULL;

static void PrintVersionInfo(void)
{
    printf("erec - polydriving recording server v"VERSION" (C) 1992,2000 Bisqwit\n");
}

#include <argh.h>

int main(int argc, const char *const *argv)
{
    int Heelp = 0;
    
    argh_init();
    
    argh_add_long("stereo", '2'); argh_add_bool('2'); argh_add_desc('2', "Specifies stereo sound. Default is mono.", NULL);
    argh_add_long("mono",   'm'); argh_add_bool('m'); argh_add_desc('m', "Redundant. It's here for esd compatibility.", NULL);
    argh_add_long("rate",   'r'); argh_add_int('r',18,999999); argh_add_desc('r', "Specifies recording rate. 43200 is default.", "<num>");
    argh_add_long("device", 'd'); argh_add_string('d',1,1023); argh_add_desc('d', "Specify device.", "<file>");
    argh_add_long("help",   'h'); argh_add_bool('h'); argh_add_desc('h', "Help", NULL);
    argh_add_long("version",'V'); argh_add_bool('V'); argh_add_desc('V', "Version information", NULL);
    
    argh_start_parse(argc, argv);
    for(;;)
    {
        int c = argh_get_param();
        if(c == -1)break;
        switch(c)
        {
            case 'V':
                PrintVersionInfo();
                return 0;
            case 'h':
                Heelp = 1;
                break;
            case '2':
                if(argh_get_bool())++stereo;else stereo=0;
                break;
            case 'm':
                if(argh_get_bool())stereo = 0;else ++stereo;
                break;
            case 'r':
                Rate = argh_get_int();
                break;
            case 'd':
                strncpy(Device, argh_get_string(), sizeof Device);
                Device[sizeof(Device)-1] = 0;
                break;
            default:
            {
                const char *s = argh_get_string();
                host = (char *)malloc(strlen(s)+1);
                strcpy(host, s);
            }
        }
    }
    if(!host)host = (char *)"10.104.2.2";
    
    if(!argh_ok())return -1;
    
    if(Heelp)
    {
        PrintVersionInfo();
        printf(
            "\nAllows multiple applications request recorded data"
            "\nat the same time with different stream attributes.\n");
        printf(
            "Usage: erec [<options> [<...>]] [<host> | none]\n"
            "Options:\n");
        argh_list_options();
        printf("\n"
            "If <host> is other than none, the output will be sent with esdcat.\n");
        return 0;
    }
    
    ...
}

*/

#endif
