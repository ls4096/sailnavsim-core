#ifndef _Command_h_
#define _Command_h_


#define COMMAND_ACTION_INVALID (-1)

#define COMMAND_ACTION_STOP (0)
#define COMMAND_ACTION_START (1)
#define COMMAND_ACTION_COURSE (2)

#define COMMAND_ACTION_ADD_BOAT (3)
#define COMMAND_ACTION_REMOVE_BOAT (4)


typedef struct Command Command;

typedef union
{
	int i;
	double d;
} CommandValue;

struct Command
{
	char* name;
	int action;
	CommandValue values[3];

	Command* next;
};


int Command_init(const char* cmdsInputPath);
Command* Command_next();


#endif // _Command_h_
