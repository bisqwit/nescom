#include <cstdio>

#include "object.hh"

void Precompile(std::FILE *fp, std::FILE *fo);
void PrecompileAndAssemble(std::FILE *fp, Object& obj);

void UseTemps();
void UseThreads();
void UseFork();
