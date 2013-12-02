/*
 * input.h
 *
 *  Created on: 01/nov/2011
 *      Author: fhorse
 */

#ifndef INPUT_H_
#define INPUT_H_

#include "common.h"

#define SET_DECODE_EVENT(id, funct) input_decode_event[id] = funct
#define SET_ADD_EVENT(id, funct) input_add_event[id] = funct
#define SET_RD_REG(id, funct) input_rd_reg[id] = funct

#define TURBO_BUTTON_DELAY_DEFAULT 3
#define TURBO_BUTTON_DELAY_MAX     20

enum controller_types { CTRL_DISABLED, CTRL_STANDARD, CTRL_ZAPPER };
enum controller_buttons {
	BUT_A,
	BUT_B,
	SELECT,
	START,
	UP,
	DOWN,
	LEFT,
	RIGHT,
	TRB_A,
	TRB_B,
	MAX_STD_PAD_BUTTONS
};
enum turbo_buttons { TURBOA, TURBOB };
enum input_types { KEYBOARD, JOYSTICK };
enum button_states { RELEASED = 0x40, PRESSED = 0x41 };
enum port_controllers {
	PORT1,
	PORT2,
	//PORT3,
	//PORT4,
	PORT_MAX
};

typedef struct {
	BYTE permit_updown_leftright;
} _config_input;
typedef struct {
	BYTE value;
} _r4016;
typedef struct {
	BYTE frequency;
	BYTE active;
	BYTE counter;
} _turbo_button;
typedef struct {
	BYTE type;
	BYTE joy_id;
	/* standard controller */
	BYTE index;
	BYTE data[24];
	DBWORD input[2][24];
	/* turbo buttons */
	_turbo_button turbo[2];
	/* zapper */
	BYTE zapper;
} _port;
typedef struct {
	_port *port[PORT_MAX];
} _array_pointers_port;

_r4016 r4016;
_port port[PORT_MAX];

void input_init(void);

BYTE input_rd_reg_disabled(BYTE openbus, WORD **screen_index, _port *port);

BYTE input_decode_event_standard(BYTE mode, DBWORD event, BYTE type, _port *port);
void input_add_event_standard(BYTE index);
BYTE input_rd_reg_standard(BYTE openbus, WORD **screen_index, _port *port);

BYTE input_rd_reg_zapper(BYTE openbus, WORD **screen_index, _port *port);
BYTE input_zapper_is_connected(_port *array);

BYTE (*input_decode_event[PORT_MAX])(BYTE mode, DBWORD event, BYTE type, _port *port);
void (*input_add_event[PORT_MAX])(BYTE index);
BYTE (*input_rd_reg[PORT_MAX])(BYTE openbus, WORD **screen_index, _port *port);

#endif /* INPUT_H_ */
