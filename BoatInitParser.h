#ifndef _BoatInitParser_h_
#define _BoatInitParser_h_

#include "Boat.h"

typedef struct
{
	Boat* boat;
	char* name;
} BoatInitEntry;


int BoatInitParser_start(const char* boatInitFilename, const char* sqliteDbFilename);
BoatInitEntry* BoatInitParser_getNext();


#endif // _BoatInitParser_h_
