#define SUPPORT_THREADS 0
#define SUPPORT_FORK 1

#ifdef WIN32
#undef SUPPORT_THREADS
#define SUPPORT_THREADS 0
#undef SUPPORT_FORK
#define SUPPORT_FORK 0
#endif

#include <cstdlib>
#include <string>

#if SUPPORT_FORK
#include <sys/wait.h>
#include <unistd.h>
#else
#include <process.h>
#include <io.h>
#endif

#if SUPPORT_THREADS
#include <pthread.h>
#endif

#include "precompile.hh"
#include "assemble.hh"

/*
  Prior preprocessing:
   constants:
    # -> something else
   comments:
    ; -> //
   define:
    and &
    or  |
    shr >>
    shl <<
    xor ^
  After preprocessing:
   restore #
*/

namespace
{
    const char HashReplacementChar = 1;
    
    enum Methods
    {
#if SUPPORT_THREADS
        Thread,
#endif
#if SUPPORT_FORK
        Fork,
#endif
        TempFile
    };
    
#if SUPPORT_THREADS
    Methods DefaultMethod = Thread;
#else
    Methods DefaultMethod = TempFile;
#endif
    
    Methods AsmMethod = DefaultMethod;
    Methods GccMethod = DefaultMethod;

    void PostProcess(std::FILE *fp, std::FILE *fo)
    {
        // The precompiler escaped some #-characters.
        // This function restores them.
        
        //std::FILE *preprocess_log = std::fopen("precompile.log", "wt");
        
        for(;;)
        {
            char Buf[16384];
            if(!std::fgets(Buf, sizeof Buf, fp)) break;
            Buf[sizeof(Buf)-1] = '\0';
            
            for(char *s=Buf; *s; ++s)
                if(*s == HashReplacementChar)
                    *s = '#';
            
            //std::fprintf(preprocess_log, "%s", Buf);
            
            fprintf(fo, "%s", Buf);
        }
        
        //std::fclose(preprocess_log);
    }
    
    void FeedGcc(std::FILE *fp, std::FILE *fo)
    {
        static const std::string firstrules =
            "#define shl <<\n"
            "#define shr >>\n"
        //    "#define and &\n"     - problems!
            "#define or |\n"
            "#define xor ^\n"
            "#define not ~\n"
            "#line 1\n";

        fwrite(firstrules.data(), 1, firstrules.size(), fo);
        
        for(;;)
        {
            char Buf[16384];
            if(!std::fgets(Buf, sizeof Buf, fp)) break;
            Buf[sizeof(Buf)-1] = '\0';
            
            std::string resultline;
            
            for(const char *s=Buf; *s; ++s)
            {
                if(*s == ';')
                    resultline += "//";
                else if(*s == '#')
                {
                    #define AllowWord(str, length) \
                        if(!strncmp(s+1, str, length)) \
                        { \
                            resultline += std::string(s, length+1); \
                            s += length; \
                            continue; \
                        }
                
                    AllowWord("ifdef",   5);
                    AllowWord("define",  6);
                    AllowWord("ifndef",  6);
                    AllowWord("include", 7);
                    AllowWord("endif",   5);
                    AllowWord("else",    4);
                    AllowWord("undef",   5);
                    AllowWord("#",       1); // appendation
                    AllowWord("if ",     3);
                    AllowWord("if(",     3);
                    
                    resultline += HashReplacementChar;
                }
                else
                    resultline += *s;
            }
            fwrite(resultline.data(), 1, resultline.size(), fo);
        }
    }

#if SUPPORT_THREADS
    struct thread_param
    {
        std::FILE *read_cpp;
        std::FILE *write_file;
        pthread_t task;
    };

    void *Postprocessor(void *param)
    {
        // Takes a void* parameter because run as a thread.
        
        struct thread_param* postpo = (struct thread_param*) param;
        
        std::FILE *fp = postpo->read_cpp;
        std::FILE *fo = postpo->write_file;
        
        PostProcess(fp, fo);
        
        return NULL;
    }

    void *Preprocessor(void *param)
    {
        // Takes a void* parameter because run as a thread.
        
        struct thread_param* postpo = (struct thread_param*) param;
        
        std::FILE *fp = postpo->read_cpp;
        std::FILE *fo = postpo->write_file;
        
        Precompile(fp, fo);
        
        std::fclose(fo);
        
        return NULL;
    }
#endif
}

void Precompile(std::FILE *fp, std::FILE *fo)
{
    if(!fp || !fo) return;
    
    switch(GccMethod)
    {
#if SUPPORT_THREADS
        case Thread:
        {
            int to_cpp[2];   pipe(to_cpp);
            int from_cpp[2]; pipe(from_cpp);
            int cpp_pid = fork();
            
            if(cpp_pid == 0)
            {
                dup2(to_cpp[0],   0); // input: FeedGcc()
                dup2(from_cpp[1], 1); //output: Postprocessor()
                close(to_cpp[1]);
                close(from_cpp[0]);
                execlp("gcc", "gcc", "-E", "-", NULL);
                _exit(-1);
            }
            close(to_cpp[0]);
            close(from_cpp[1]);
            
            FILE *cpp    = fdopen(to_cpp[1], "wt");
            FILE *result = fdopen(from_cpp[0], "rt");
            
            // Create a thread for reading the pipe.
            struct thread_param postpo;
            postpo.read_cpp   = result; // input: gcc
            postpo.write_file = fo;     //output: outputfile
            pthread_create(&postpo.task, NULL, Postprocessor, (void *)&postpo);
            
            FeedGcc(fp, cpp);           // input: inputfile
            std::fclose(cpp);           //output: gcc
            
            pthread_join(postpo.task, NULL);
            
            std::fclose(result);
            
            wait(NULL);
            break;
        }
#endif

#if SUPPORT_FORK
        case Fork:
        {
            int to_cpp[2];   pipe(to_cpp);
            int from_cpp[2]; pipe(from_cpp);
            int cpp_pid = fork();
            
            if(cpp_pid == 0)
            {
                dup2(to_cpp[0],   0); // input: FeedGcc()
                dup2(from_cpp[1], 1); //output: Postprocessor()
                close(to_cpp[1]);
                close(from_cpp[0]);
                execlp("gcc", "gcc", "-E", "-", NULL);
                _exit(-1);
            }
            close(to_cpp[0]);
            close(from_cpp[1]);
            
            FILE *cpp    = fdopen(to_cpp[1], "wt");
            FILE *result = fdopen(from_cpp[0], "rt");
            
            int feed_pid = fork();
            if(!feed_pid)
            {
                std::fclose(stdin);  // not used
                std::fclose(stdout); // not used
                std::fclose(result);
                FeedGcc(fp, cpp);           // input: inputfile
                std::fclose(cpp);           //output: gcc
                _exit(0);
            }
            std::fclose(cpp);
            
            PostProcess(result, fo);    // input: gcc
            std::fclose(result);        //output: outputfile
            
            wait(NULL); // wait for gcc    to die
            wait(NULL); // wait for feeder to die
            
            break;
        }
#endif
        case TempFile:
        {
            // 1: Preprocess for gcc
            std::FILE *temp = std::tmpfile();
            FeedGcc(fp, temp);         // input: inputfile
            rewind(temp);              //output: tmp1
            
            // 2: Precompile with gcc
            std::FILE *temp2 = std::tmpfile();
#if SUPPORT_FORK
            int cpp_pid = fork();
            if(cpp_pid == 0)
            {
                dup2(fileno(temp),  0); // input: tmp1
                dup2(fileno(temp2), 1); //output: tmp2
                std::fclose(temp);
                std::fclose(temp2);
                execlp("gcc", "gcc", "-E", "-", NULL);
                _exit(-1);
            }
            std::fclose(temp);
            wait(NULL);
#else
            int org_stdin = dup(0);  dup2(fileno(temp), 0);
            int org_stdout = dup(1); dup2(fileno(temp2), 1);
            fclose(temp);
            int pid = _spawnlp(_P_WAIT, "gcc", "gcc", "-E", "-", NULL);
            dup2(org_stdin, 0); close(org_stdin);
            dup2(org_stdout, 1); close(org_stdout);
            
            if(pid == -1)
            {
                perror("_spawnlp");
                return;
            }
#endif
            
            // 3: Postprocess
            std::rewind(temp2);
            PostProcess(temp2, fo);  // input: tmp2
            std::fclose(temp2);      //output: outputfile
            break;
        }
    }
}

void PrecompileAndAssemble(std::FILE *fp, Object& obj)
{
    switch(AsmMethod)
    {
#if SUPPORT_THREADS
        case Thread:
        {
            /* THREAD VERSION */
            int pip[2]; pipe(pip); // For communication.
            
            struct thread_param prepo;
            
            prepo.read_cpp   = fp;
            prepo.write_file = fdopen(pip[1], "wt");
            
            pthread_create(&prepo.task, NULL, Preprocessor, (void*)&prepo);

            std::FILE *result = fdopen(pip[0], "rt");
            
            AssemblePrecompiled(result, obj);
            
            fclose(result);
            
            pthread_join(prepo.task, NULL);
            break;
        }
#endif

#if SUPPORT_FORK
        case Fork:
        {
            /* PIPE&FORK VERSION */
            int pip[2]; pipe(pip); // For communication.
            
            int pid = fork();
            if(!pid)
            {
                std::fclose(stdin);  // not used
                std::fclose(stdout); // not used
                
                /* Start a precompiler as a child process */
                close(pip[0]);
                std::FILE *result = fdopen(pip[1], "wt");
                Precompile(fp, result);
                std::fclose(result);
                _exit(0);
            }
            
            std::FILE *result = fdopen(pip[0], "rt");
            close(pip[1]);
            
            AssemblePrecompiled(result, obj);
            
            fclose(result);
            wait(NULL);
            break;
        }
#endif
        case TempFile:
        {
            std::FILE *temp = std::tmpfile();
            if(!temp)
            {
                std::perror("Error: Unable to create a temp file");
                return;
            }
            
            Precompile(fp, temp);
            
            rewind(temp);
            
            AssemblePrecompiled(temp, obj);
            
            fclose(temp);
            
            break;
        }
    }
}

void UseTemps()
{
    AsmMethod = TempFile;
    GccMethod = TempFile;
}

void UseThreads()
{
#if SUPPORT_THREADS
    AsmMethod = Thread;
    GccMethod = Thread;
#else
    std::fprintf(stderr, "Warning: Thread support not built in, using forks instead\n");
    UseFork();
#endif
}

void UseFork()
{
#if SUPPORT_FORK
    AsmMethod = Fork;
    GccMethod = Fork;
#else
    std::fprintf(stderr, "Warning: Fork support not built in, using tempfiles instead\n");
    UseTemps();
#endif
}
