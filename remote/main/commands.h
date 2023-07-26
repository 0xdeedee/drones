#ifndef __COMMANDS_H__
#define __COMMANDS_H__


typedef enum
{
	TYPE_CMD = 0,
	TYPE_VIDEO,
} remote_commands_type_t;

typedef enum
{
	LEFT_X_DATA = 0,
	LEFT_Y_DATA,
	RIGHT_X_DATA,
	RIGHT_Y_DATA,
	BTN_0_DATA,
	BTN_1_DATA,
	BTN_2_DATA,
	BTN_3_DATA,
	BTN_4_DATA,
	BTN_5_DATA,
	BTN_6_DATA,
	BTN_7_DATA,
	BTN_8_DATA,
	BTN_9_DATA,
	CMD_END
} remote_commands_t;


typedef struct commands
{
	unsigned int		cmd_id;
	unsigned int		cmd_type;
	unsigned int 		cmd_status;
	unsigned int		cmds[25];
} __attribute__( ( packed ) )commands_t;


#endif //__COMMANDS_H__

