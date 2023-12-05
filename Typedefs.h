#ifndef Typedefs
#define Typedefs

typedef void* (*CreateInterfaceFn)(const char *pName, int *pReturnCode);
typedef void* (*InstantiateInterfaceFn)();
typedef void* edict_t;
typedef void* KeyValues;
typedef unsigned char byte;
struct Vector
{
	float x, y, z;
};
#endif