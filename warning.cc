#include <set>
#include <string>
#include <algorithm>

#include "warning.hh"

namespace
{
    std::set<std::string> Allowed;
    
    const char *const WarningList[] =
    {
        // Sorted alphabetically!
        "jumps",
        "unused-label",
        "use32"
    };
    const unsigned NumWarnings = sizeof(WarningList) / sizeof(WarningList[0]);

    void AllWarnings(void (*const func)(const std::string&))
    {
        for(unsigned a=0; a<NumWarnings; ++a)
            func(WarningList[a]);
    }
    bool IsWarning(const std::string& name)
    {
        const char *const *p = std::lower_bound(WarningList, WarningList+NumWarnings, name);
        if(p == WarningList+NumWarnings) return false;
        return name == *p;
    }
}

bool MayWarn(const std::string& name)
{
    return Allowed.find(name) != Allowed.end();
}

void EnableWarning(const std::string& name)
{
    if(name.substr(0, 3) == "no-") { DisableWarning(name.substr(3)); return; }
    if(name == "all" || name.empty()) { AllWarnings(EnableWarning); return; }
    
    if(!IsWarning(name))
    {
        std::fprintf(stderr, "Error: Invalid warning option -W%s\n", name.c_str());
        return;
    }
    
    Allowed.insert(name);
}

void DisableWarning(const std::string& name)
{
    if(name.substr(0, 3) == "no-") { EnableWarning(name.substr(3)); return; }
    if(name == "all" || name.empty()) { AllWarnings(DisableWarning); return; }
    
    if(!IsWarning(name))
    {
        std::fprintf(stderr, "Error: Invalid warning option -Wno-%s\n", name.c_str());
        return;
    }
    
    Allowed.erase(name);
}
