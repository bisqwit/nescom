#include <string>

void MessageLinkingModules(unsigned count);

void MessageLoadingItem(const std::string& header);

void MessageWorking();

void MessageDone();

void MessageModuleWithoutAddress(const std::string& name);
void MessageUndefinedSymbol(const std::string& name);
void MessageDuplicateDefinition(const std::string& name, unsigned nmods, unsigned ndefs);
void MessageUndefinedSymbols(unsigned n);
