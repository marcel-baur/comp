#ifndef comp_compiler_h
#define comp_compiler_h

#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* source);

#endif
