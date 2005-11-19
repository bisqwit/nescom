#include <cstdio>
#include <vector>
#include <string>

#include "assemble.hh"
#include "precompile.hh"
#include "warning.hh"

#include <argh.hh>

bool already_reprocessed = false;

namespace
{
    enum OutputFormat
    {
        IPSformat,
        O65format
    } format = O65format;

    void SetOutputFormat(const std::string& s)
    {
        if(s == "ips") format = IPSformat;
        if(s == "o65") format = O65format;
    }
}

int main(int argc, const char* const *argv)
{
    ParamHandler Argh;
    
    Argh.AddLong("help",       'h').SetBool().SetDesc("This help");
    Argh.AddLong("version",    500).SetBool().SetDesc("Displays version information");
    Argh.AddString(            'o').SetDesc("Place the output into <file>", "<file>");
    Argh.AddBool(              'E').SetDesc("Preprocess only");
    Argh.AddBool(              'c').SetDesc("Ignored for gcc-compatibility");

    Argh.AddLong("submethod", 501).SetString().
        SetDesc("Select subprocess method: temp,thread,pipe", "<method>");
    Argh.AddLong("temps", 502).SetBool().SetDesc("Short for --submethod=temp");
    Argh.AddLong("pipes", 503).SetBool().SetDesc("Short for --submethod=pipe");
    Argh.AddLong("threads", 504).SetBool().SetDesc("Short for --submethod=thread");

    Argh.AddLong("outformat", 601).SetString().
        SetDesc("Select output format: ips,o65", "<format>");
    Argh.AddBool('I').SetDesc("Short for --outformat=ips");

    Argh.AddString('W').SetDesc("Enable warnings", "<type>");
    
    Argh.StartParse(argc, argv);
    
    bool assemble = true;
    std::vector<std::string> files;
    
    std::FILE *output = NULL;
    std::string outfn;

    for(;;)
    {
        switch(Argh.GetParam())
        {
            case -1: goto EndArgList;
            
            case 500: //version
                printf(
                    "%s %s\n"
                    "Copyright (C) 1992,2005 Bisqwit (http://iki.fi/bisqwit/)\n"
                    "This is free software; see the source for copying conditions. There is NO\n"
                    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n",
                    Argh.ProgName().c_str(), VERSION
                      );
                return 0;
            case 501: //submethod
            {
                const std::string method = Argh.GetString();
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
            case 502: if(Argh.GetBool()) UseTemps(); break;
            case 503: if(Argh.GetBool()) UseFork(); break;
            case 504: if(Argh.GetBool()) UseThreads(); break;
            
            case 601:
            {
                const std::string format = Argh.GetString();
                if(format == "ips" || format == "o65")
                    SetOutputFormat(format);
                else
                {
                    std::fprintf(stderr, "Error: --format requires 'ips' or 'o65'\n");
                    return -1;
                }
                break;
            }
            case 'I': if(Argh.GetBool()) SetOutputFormat("ips"); break;
            
            case 'h':
                std::printf(
                    "6502 assembler\n"
                    "Copyright (C) 1992,2005 Bisqwit (http://iki.fi/bisqwit/)\n"
                    "\nUsage: %s [<option> [<...>]] <file> [<...>]\n"
                    "\nAssembles 6502 code.\n"
                    "\nOptions:\n",
                    Argh.ProgName().c_str());
                Argh.ListOptions();
                std::printf("\nNo warranty whatsoever.\n");
                return 0;
            case 'E':
                assemble = !Argh.GetBool();
                break;
            case 'o':
            {
                outfn = Argh.GetString();
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
                ErrorExit:
                        if(output)
                        {
                            fclose(output);
                            remove(outfn.c_str());
                        }
                        return -1;
                    }
                }
                break;
            }
            case 'W':
            {
                EnableWarning(Argh.GetString());
                break;
            }
            default:
                files.push_back(Argh.GetString());
        }
    }
EndArgList:
    if(!Argh.ok()) goto ErrorExit;
    if(!files.size())
    {
        fprintf(stderr, "Error: Assemble what? See %s --help\n",
            Argh.ProgName().c_str());
        goto ErrorExit;
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
            //stdin.
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
        }
        obj.Dump();
    }
    
    if(output) fclose(output);
    
    return 0;
}
