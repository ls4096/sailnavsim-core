#ifndef _Boat_h_
#define _Boat_h_

#include <stdbool.h>

#include <proteus/GeoVec.h>
#include <proteus/GeoPos.h>


typedef struct
{
	proteus_GeoPos pos;
	proteus_GeoVec v;

	double desiredCourse;

	int boatType;

	bool stop;
	bool sailsDown;
	bool movingToSea;

	bool setImmediateDesiredCourse;
} Boat;


int Boat_init();

Boat* Boat_new(double lat, double lon, int boatType);
void Boat_advance(Boat* b, double s);
bool Boat_isHeadingTowardWater(Boat* b);


#endif // _Boat_h_
