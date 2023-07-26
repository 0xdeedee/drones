/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include "commands.h"


#define ESP_INTR_FLAG_DEFAULT	( 0 )



extern void wifi_init(void);

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO		( 38 ) //CONFIG_BLINK_GPIO
#define LED_RIGHT		( 27 ) //CONFIG_BLINK_GPIO
#define LED_LEFT		( 13 ) //CONFIG_BLINK_GPIO
#define BTN_RIGHT		( 19 )
#define BTN_LEFT		( 20 )
#define GPIO_INPUT_PIN_SEL	( ( 1ULL << BTN_RIGHT ) | ( 1ULL << BTN_LEFT ) )


static int		s_led_state = 0;
static int		led_right_state = 0;
static int		led_right_state_old = 0;
static int		led_right_status = 0;
static int		led_left_state = 0;
static int		led_left_state_old = 0;
static int		led_left_status = 0;
static QueueHandle_t	gpio_evt_queue = NULL;


//static const uint8_t A0 = 36;
//static const uint8_t A1 = 39;
//static const uint8_t A2 = 34;
//static const uint8_t A3 = 35;



#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1
#define _EXAMPLE_ADC_UNIT_STR(unit)         #unit
#define EXAMPLE_ADC_UNIT_STR(unit)          _EXAMPLE_ADC_UNIT_STR(unit)
#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_0
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH


#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type2.data)

#define EXAMPLE_READ_LEN                    256

static adc_channel_t channels[4] = { ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_6 };

static TaskHandle_t s_task_handle;
static const char *TAG = "EXAMPLE";

static commands_t		cmd;


static bool IRAM_ATTR s_conv_done_cb( adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data )
{
	BaseType_t			mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
	vTaskNotifyGiveFromISR( s_task_handle, &mustYield );
	return ( mustYield == pdTRUE );
}


static void continuous_adc_init( adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle )
{
	adc_continuous_handle_t		handle = NULL;

	adc_continuous_handle_cfg_t	adc_config = {
		.max_store_buf_size = 1024,
		.conv_frame_size = EXAMPLE_READ_LEN,
	};
	adc_continuous_config_t		dig_cfg = {
		.sample_freq_hz = 1 * 1000,
		.conv_mode = EXAMPLE_ADC_CONV_MODE,
		.format = EXAMPLE_ADC_OUTPUT_TYPE,
	};

	ESP_ERROR_CHECK( adc_continuous_new_handle( &adc_config, &handle ) );
	adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
	dig_cfg.pattern_num = channel_num;
	for ( int i = 0; i < channel_num; i++ )
	{
		adc_pattern[i].atten = ADC_ATTEN_DB_11;
		adc_pattern[i].channel = channel[i];
		adc_pattern[i].unit = 0;
		adc_pattern[i].bit_width = 12;

		ESP_LOGI( TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten );
		ESP_LOGI( TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel );
		ESP_LOGI( TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit );
	}
	dig_cfg.adc_pattern = adc_pattern;
	ESP_ERROR_CHECK( adc_continuous_config( handle, &dig_cfg ) );

	*out_handle = handle;
}

extern void send_data( commands_t *cmd );


void adc_reader_handler( void *arg )
{
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[EXAMPLE_READ_LEN] = {0};
	unsigned char		to_send = 0;

    memset(result, 0xcc, EXAMPLE_READ_LEN);

    s_task_handle = xTaskGetCurrentTaskHandle();

    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channels, sizeof(channels) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

	while( 1 )
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		char unit[] = EXAMPLE_ADC_UNIT_STR(EXAMPLE_ADC_UNIT);

		while ( 1 )
		{
			to_send = 0;
			ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
			if (ret == ESP_OK)
			{
//				ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32, ret, ret_num);
				for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES)
				{
					adc_digi_output_data_t		*p = (void*)&result[i];
					uint32_t			chan_num = EXAMPLE_ADC_GET_CHANNEL(p);
					uint32_t			data = EXAMPLE_ADC_GET_DATA(p);
                    /* Check the channel number validation, the data is invalid if the channel num exceed the maximum channel */
					if ( chan_num < SOC_ADC_CHANNEL_NUM( EXAMPLE_ADC_UNIT ) ) 
					{
						float		ff = 6.5 - ( data * ( 3.3 / 1023.0 ) );
						int		dd;

						dd = 0;
						if ( -3.0 > ff )
							dd = -1;
						if ( 3.0 < ff )
							dd = 1;

						switch ( chan_num )
						{
							case 3:
								printf( "%s\n\n%d %d %f\n\n", 
									__FUNCTION__, 
									dd, 
									cmd.cmds[LEFT_X_DATA],
									ff );
								if ( cmd.cmds[LEFT_X_DATA] != dd )
								{
									printf( "%s========\n\n\n\n", __FUNCTION__ );
									cmd.cmds[LEFT_X_DATA] = dd;
									to_send = 1;
								}
							break;
							case 4:
								if ( cmd.cmds[LEFT_Y_DATA] != dd )
								{
									cmd.cmds[LEFT_Y_DATA] = dd;
									to_send = 1;
								}
							break;
							case 5:
								if ( cmd.cmds[RIGHT_X_DATA] != dd )
								{
									cmd.cmds[RIGHT_X_DATA] = dd;
									to_send = 1;
								}
							break;
							case 6:
								if ( cmd.cmds[RIGHT_Y_DATA] != dd )
								{
									cmd.cmds[RIGHT_Y_DATA] = dd;
									to_send = 1;
								}
							break;
						}
//						ESP_LOGI(TAG, "Unit: %s, Channel: %"PRIu32", Value: %04f", 
//								unit, chan_num, data * ( 3.3 / 1023.0 ) );
					}
					else
					{
						ESP_LOGW(TAG, "Invalid data [%s_%"PRIu32"_%"PRIx32"]", unit, chan_num, data);
                    
					}
				
				}
				printf( "=======%d========\n", to_send );
				if ( to_send )
					send_data( &cmd );
				vTaskDelay(1);
			}
			else if (ret == ESP_ERR_TIMEOUT) 
			{
				break;
			}
		}
	}


    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
}






static void IRAM_ATTR gpio_isr_handler( void* arg )
{
	uint32_t		gpio_num = ( uint32_t ) arg;

//	printf("1"); 
	xQueueSendFromISR( gpio_evt_queue, &gpio_num, NULL );
}

static void gpio_task_example( void* arg )
{
	uint32_t		io_num;

	for( ; ; ) 
	{
		if( xQueueReceive( gpio_evt_queue, &io_num, portMAX_DELAY ) ) 
		{
			if ( BTN_LEFT == io_num )
			{
				led_left_state = gpio_get_level( io_num );
				if ( led_left_state_old == 0 && led_left_state == 1)
				{
					led_left_status = !led_left_status;
					gpio_set_level( LED_LEFT, led_left_status );
				}
				led_left_state_old = led_left_state;
			}
			if ( BTN_RIGHT == io_num )
			{
				led_right_state = gpio_get_level( io_num );
				if ( led_right_state_old == 0 && led_right_state == 1 )
				{
					led_right_status = !led_right_status;
					gpio_set_level( LED_RIGHT, led_right_status );
				}
				led_right_state_old = led_right_state;
			}
		}
	}
}

static void blink_led(void)
{
	gpio_set_level( BLINK_GPIO , s_led_state );
}

static void configure_led( unsigned int __gpio )
{
	gpio_reset_pin( __gpio );
	gpio_set_direction( __gpio, GPIO_MODE_OUTPUT );
}


void app_main(void)
{
/*
	gpio_config_t 		io_conf = {};

//	configure_led( BLINK_GPIO );
//	configure_led( LED_RIGHT );
//	configure_led( LED_LEFT );

	gpio_evt_queue = xQueueCreate( 10, sizeof( uint32_t ) );
	xTaskCreate( gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL );

	io_conf.intr_type = GPIO_INTR_POSEDGE;
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 0;
	gpio_config( &io_conf );

	gpio_set_intr_type( BTN_RIGHT, GPIO_INTR_ANYEDGE );
	gpio_set_intr_type( BTN_LEFT, GPIO_INTR_ANYEDGE );

	gpio_install_isr_service( ESP_INTR_FLAG_DEFAULT );
	gpio_isr_handler_add( BTN_LEFT, gpio_isr_handler, ( void* ) BTN_LEFT );
	gpio_isr_handler_add( BTN_RIGHT, gpio_isr_handler, ( void* ) BTN_RIGHT );

*/
	xTaskCreatePinnedToCore( adc_reader_handler, "adc_reader_handler", 4096, NULL, 1, NULL, 1 );

	wifi_init();

	while ( 1 ) 
	{
//		blink_led();
//		s_led_state = !s_led_state;
		vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
	}
}
