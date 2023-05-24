#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_ELF_SYMBOLTABLE_MAPPING_H__
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_ELF_SYMBOLTABLE_MAPPING_H__ 1

#include "native_client/src/include/elf64.h"

struct SymbolTableMapEntry
{
  char* name;
  Elf64_Addr address;
};

struct SymbolTableMapping
{
  uint32_t symbolCount;
  struct SymbolTableMapEntry* symbolMap;
};

#endif