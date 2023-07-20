#include <stdio.h>
#include <stdlib.h>
#include "tb6612.hpp"

MOTOR::MOTOR(	gpio_num_t		_a1,
		gpio_num_t		_a2,
		gpio_num_t		_pwm,
		mcpwm_unit_t		_unit,
		mcpwm_timer_t		_timer,
		mcpwm_io_signals_t	_iosig,
		mcpwm_operator_t	_op )
{
	a1		= _a1;
	a2		= _a2;
	pwmport		= _pwm;
	unit		= _unit;
	timer		= _timer;
	iosig		= _iosig;
	op		= _op;

	gpio_config_t io_conf = {};
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = ( ( 1ULL << a1 ) | ( 1ULL << a2 ) );
	io_conf.pull_down_en = ( gpio_pulldown_t ) 0;
	io_conf.pull_up_en = ( gpio_pullup_t ) 0;
	gpio_config(&io_conf);

	mcpwm_gpio_init( unit, iosig, pwmport );
	mcpwm_config_t config;
	config.frequency = 10000;
	config.cmpr_a = 0;
	config.cmpr_b = 0;
	config.counter_mode = MCPWM_UP_COUNTER;
	config.duty_mode = MCPWM_DUTY_MODE_0;
	mcpwm_init( unit, timer, &config );
}

MOTOR::~MOTOR()
{
	gpio_set_level( a1, 0 );
	gpio_set_level( a2, 0 );
	mcpwm_stop( unit, timer );
}

void MOTOR::setspeed( float speed )
{
	if( speed >= 0 ) 
	{
		gpio_set_level( a1, 0 );
		gpio_set_level( a2, 1 );
	}
	else
	{
		gpio_set_level( a1, 1 );
		gpio_set_level( a2, 0 );
	}
	mcpwm_set_duty( unit, timer, op, abs( speed ) );
}

void MOTOR::stop()
{
	gpio_set_level( a1, 0 );
	gpio_set_level( a2, 0 );
}

void MOTOR::brake()
{
	gpio_set_level( a1, 1 );
	gpio_set_level( a2, 1 );
}





