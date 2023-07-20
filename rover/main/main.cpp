
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "tb6612.hpp"
#include "softap.h"

MOTOR front_left_motor  ( ( gpio_num_t ) 40, ( gpio_num_t ) 41, ( gpio_num_t ) 42, MOTOR0 ); // fl
MOTOR front_right_motor ( ( gpio_num_t ) 39, ( gpio_num_t ) 38, ( gpio_num_t ) 37, MOTOR1 ); // fr
MOTOR rear_right_motor  ( ( gpio_num_t ) 12, ( gpio_num_t ) 13, ( gpio_num_t ) 14, MOTOR2 ); // rr
MOTOR rear_left_motor   ( ( gpio_num_t ) 11, ( gpio_num_t ) 10, ( gpio_num_t )  9, MOTOR3 ); // rl

static int	left_speed = 100;
static int	right_speed = 100;



static void left_motors_stop()
{
	front_left_motor.brake();
	rear_left_motor.brake();
}

static void right_motors_stop()
{
	front_right_motor.brake();
	rear_right_motor.brake();
}

static void stop()
{
	left_motors_stop();
	right_motors_stop();
}

static void left_motors_forward()
{
	front_left_motor.setspeed( -left_speed );
	rear_left_motor.setspeed( -left_speed );
}

static void left_motors_backward()
{
	front_left_motor.setspeed( left_speed );
	rear_left_motor.setspeed( left_speed );
}

static void right_motors_forward()
{
	front_right_motor.setspeed( right_speed );
	rear_right_motor.setspeed( right_speed );
}

static void right_motors_backward()
{
	front_right_motor.setspeed( -right_speed );
	rear_right_motor.setspeed( -right_speed );
}

static void forward()
{
	left_motors_forward();
	right_motors_forward();
}

static void backward()
{
	left_motors_backward();
	right_motors_backward();
}

static void right()
{
	front_left_motor.setspeed( -left_speed );
	rear_left_motor.setspeed( left_speed );
	front_right_motor.setspeed( -right_speed );
	rear_right_motor.setspeed( right_speed );
}

static void left()
{
	front_left_motor.setspeed( left_speed );
	rear_left_motor.setspeed( -left_speed );
	front_right_motor.setspeed( right_speed );
	rear_right_motor.setspeed( -right_speed );
}

static void motor_task( void *pvParameters )
{
	while ( 1 )
	{
		ESP_LOGI("motor", "start");
		left();
		vTaskDelay(1000/portTICK_PERIOD_MS);

		ESP_LOGI("motor", "brake");
		stop();
		vTaskDelay(1000/portTICK_PERIOD_MS);

		ESP_LOGI("motor", "reverse direction");
		right();
		vTaskDelay(1000/portTICK_PERIOD_MS);

		ESP_LOGI("motor", "brake");
		stop();
		vTaskDelay(1000/portTICK_PERIOD_MS);
	}
}


void enable_power_motor_ctrl()
{
	gpio_config_t		io_conf = {};

	io_conf.intr_type	= GPIO_INTR_DISABLE;
	io_conf.mode		= GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask	= ( ( 1ULL << 4 ) | ( 1ULL << 5 ) );
	io_conf.pull_down_en	= ( gpio_pulldown_t ) 0;
	io_conf.pull_up_en	= ( gpio_pullup_t ) 0;
	gpio_config( &io_conf );

	gpio_set_level( ( gpio_num_t ) 4, 1 );
	gpio_set_level( ( gpio_num_t ) 5, 1 );
}

extern "C" void app_main()
{

	init_ap();
	enable_power_motor_ctrl();	

//	xTaskCreatePinnedToCore(&motor_task,"motor_task", 2*2048,NULL,5,NULL,0);

	while ( 1 )
	{
		vTaskDelay(5000/portTICK_PERIOD_MS);
	}
}





