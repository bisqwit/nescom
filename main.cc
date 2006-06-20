#include <cstdio>
#include <vector>
#include <string>

#include "assemble.hh"
#include "precompile.hh"
#include "warning.hh"

#include <getopt.h>

bool already_reprocessed = true; // affects jump length counting

namespace
{
    enum OutputFormat
    {
        IPSformat,
        O65format,
        RAWformat
    } format = O65format;

    void SetOutputFormat(const std::string& s)
    {
        if(s == "ips") format = IPSformat;
        else if(s == "o65") format = O65format;
        else if(s == "raw") format = RAWformat;
        else
        {
            std::fprintf(stderr, "Error: Unknown output format `%s'\n", s.c_str());
        }
    }
}

int main(int argc, char**argv)
{
    bool assemble = true;
    std::vector<std::string> files;
    
    std::FILE *output = NULL;
    std::string outfn;

    for(;;)
    {
        int option_index = 0;
        static struct option long_options[] =
        {
            {"help",     0,0,'h'},
            {"version",  0,0,'V'},
            {"output",   0,0,'o'},
            {"preprocess",0,0,'E'},
            {"compile",   0,0,'c'},
            {"submethod", 0,0,501},
            {"outformat", 0,0,'f'},
            {"out_ips",   0,0,'I'},
            {"warn",      0,0,'W'},
            {0,0,0,0}
        };
        int c = getopt_long(argc,argv, "hVo:EcJf:IW:", long_options, &option_index);
        if(c==-1) break;
        switch(c)
        {
            case 'V': //version
                printf(
                    "%s %s\n"
                    "Copyright (C) 1992,2006 Bisqwit (http://iki.fi/bisqwit/)\n"
                    "This is free software; see the source for copying conditions. There is NO\n"
                    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n",
                    argv[0], VERSION
                      );
                return 0;
            case 501: //submethod
            {
                const std::string method = optarg;
                if(method == "temp" || method == "temps")
                    UseTemps();
                else if(method == "thread" || method == "threads")
                    UseThreads();
                else if(method == "pipe" || method == "pipes"
                     || method == "fork" || method == "forks")
                    UseFork();
                else
                {
                    std::fprintf(stderr, "Error: --method requires 'pipe', 'thread' or 'temp'\n");
                    return -1;
                }
                break;
            }
            
            case 601:
            {
                const std::string format = optarg;
                if(format == "ips" || format == "o65")
                    SetOutputFormat(format);
                else
                {
                    std::fprintf(stderr, "Error: --format requires 'ips' or 'o65'\n");
                    return -1;
                }
                break;
            }
            case 'I': SetOutputFormat("ips"); break;
            
            case 'h':
                std::printf(
                    "6502 assembler\n"
                    "Copyright (C) 1992,2006 Bisqwit (http://iki.fi/bisqwit/)\n"
                    "\nUsage: %s [<option> [<...>]] <file> [<...>]\n"
                    "\nAssembles 6502 code.\n"
                    "\nOptions:\n"
                    " --help, -h            This help\n"
                    " --version, -V         Displays version information\n"
                    " -o <file>             Places the output into <file>\n"
                    " -E                    Preprocess only\n"
                    " -c                    Ignored for gcc-compatibility\n"
                    " --version             Displays version information\n"
                    " --submethod <method>  Select subprocess method: temp,thread,pipe\n"
                    " -f, --outformat <fmt> Select output format: ips,raw,o65 (default: o65)\n"
                    "                         -I is short for -fips\n"
                    " -W <type>             Enable warnings\n"
                    "\nNo warranty whatsoever.\n",
                    argv[0]);
                return 0;
            case 'E':
                assemble = false;
                break;
            case 'o':
            {
                outfn = optarg;
                if(outfn != "-")
                {
                    if(output)
                    {
                        std::fclose(output);
                    }
                    output = std::fopen(outfn.c_str(), "wb");
                    if(!output)
                    {
                        std::perror(outfn.c_str());
                        goto ErrorExit;
                    }
                }
                break;
            }
            case 'f':
            {
                SetOutputFormat(optarg);
                break;
            }
            case 'W':
            {
                EnableWarning(optarg);
                break;
            }
            case '?':
                goto ErrorExit;
        }
    }
    
    while(optind < argc)
        files.push_back(argv[optind++]);

    if(files.empty())
    {
        fprintf(stderr, "Error: Assemble what? See %s --help\n", argv[0]);
    ErrorExit:
        if(output)
        {
            fclose(output);
            remove(outfn.c_str());
        }
        return -1;
    }
    
    Object obj;
    
    /*
     *   TODO:
     *        - Read input
     *        - Write object
     *        - Verbose errors
     *        - Verbose errors
     *        - Verbose errors
     *        - Verbose errors
     *        - Verbose errors
     */
    
Reprocess:
    obj.ClearMost();
    
    for(unsigned a=0; a<files.size(); ++a)
    {
        std::FILE *fp = NULL;
        
        const std::string& filename = files[a];
        if(filename != "-" && !filename.empty())
        {
            fp = fopen(filename.c_str(), "rt");
            if(!fp)
            {
                std::perror(filename.c_str());
                continue;
            }
        }
        else
        {
        }
    
        if(assemble)
            PrecompileAndAssemble(fp ? fp : stdin, obj);
        else
            Precompile(fp ? fp : stdin, output ? output : stdout);
        
        if(fp)
            std::fclose(fp);
    }
    
    if(assemble)
    {
        obj.CloseSegments();
        
        std::FILE* stream = output ? output : stdout;
        
        switch(format)
        {
            case IPSformat:
                obj.WriteIPS(stream);
                break;
            case O65format:
                obj.WriteO65(stream);
                break;
            case RAWformat:
                obj.WriteRAW(stream);
                break;
        }
        obj.Dump();
    }
    
    if(output) fclose(output);
    
    return 0;
}
