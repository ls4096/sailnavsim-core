#ifndef _BoatRegistry_h_
#define _BoatRegistry_h_

#include "Boat.h"


#define BoatRegistry_OK (0)
#define BoatRegistry_EXISTS (-1)
#define BoatRegistry_NOTEXISTS (-2)


typedef struct BoatEntry BoatEntry;

struct BoatEntry
{
	char* name;
	Boat* boat;
	BoatEntry* next;
};


int BoatRegistry_add(Boat* boat, const char* name);
Boat* BoatRegistry_get(const char* name);
Boat* BoatRegistry_remove(const char* name);
BoatEntry* BoatRegistry_getAllBoats(unsigned int* boatCount);


#endif // _BoatRegistry_h_
