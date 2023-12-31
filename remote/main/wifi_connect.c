

#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi_types.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "commands.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID		"ROVER"
#define EXAMPLE_ESP_WIFI_PASS		"kikimora"
#define EXAMPLE_ESP_MAXIMUM_RETRY	( 10 ) 

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT		BIT0
#define WIFI_FAIL_BIT			BIT1

static const char *TAG = "wifi station";

static int				s_retry_num = 0;
static int				connection_status = 0;
static int				connection_fd = -1;



static void stop_tcpip_connection()
{
	close( connection_fd );
	connection_fd = -1;
	connection_status = 0;
}

static void start_tcpip_connection()
{
	struct sockaddr_in		servaddr;
 
    // socket create and verification
	if ( -1 == ( connection_fd = socket( AF_INET, SOCK_STREAM, 0 ) ) )
	{
		printf( "socket creation failed [%d] %s\n", errno, strerror( errno ) );
		return;
	}
	else
		printf( "Socket successfully created..\n" );

	memset( &servaddr, 0, sizeof( servaddr ) );
 
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr( "192.168.4.1" );
	servaddr.sin_port = htons( 2222 );
 
	if ( 0 != connect( connection_fd, ( struct sockaddr * )&servaddr, sizeof( servaddr ) ) )
	{
		printf( "connection with the server failed [%d] %s\n", errno, strerror( errno ) );
		close( connection_fd );
		connection_fd = -1;
		return;
	}
	else
		printf( "connected to the server..\n" );

	connection_status = 1;

	int ret = send( connection_fd, "DEEDEE", 7, 0 );
	printf( "send result %d\n", ret );
}


void send_data( commands_t *cmd )
{
	printf( "%s\n", __FUNCTION__ );
	send( connection_fd, cmd, sizeof( commands_t ), 0 );
}

static void monitor_status( void *args )
{
	EventBits_t		bits = 0;

	while( 1 )
	{
		bits = xEventGroupGetBits( s_wifi_event_group );
		if ( ( bits & WIFI_CONNECTED_BIT ) > 0 )
		{
			if ( !connection_status )
				start_tcpip_connection();
		}
		else
		{
			if ( connection_status )
				stop_tcpip_connection();
		}
		vTaskDelay( 1000 / portTICK_PERIOD_MS );
	}

}


static void recv_task( void *args )
{
	int			ret;
	unsigned char		b[130];

	while( 1 )
	{
		if ( !connection_status )
		{
			vTaskDelay( 1000 / portTICK_PERIOD_MS );
			continue;
		}
		memset( b, 0, sizeof( b ) );
		ret = recv( connection_fd, b, sizeof( b ), 0);
		printf("received %s\n", b );
	}

}

static void event_handler(	void* arg, 
				esp_event_base_t event_base,
				int32_t event_id,
				void* event_data )
{
	if ( ( event_base == WIFI_EVENT ) && ( event_id == WIFI_EVENT_STA_START ) )
	{
		esp_wifi_connect();
	}
	else if ( ( event_base == WIFI_EVENT ) && ( event_id == WIFI_EVENT_STA_DISCONNECTED ) ) 
	{
		if ( s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY )
		{
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI( TAG, "retry to connect to the AP" );
		}
		else
		{
			xEventGroupSetBits( s_wifi_event_group, WIFI_FAIL_BIT );
		}
		ESP_LOGI( TAG,"connect to the AP fail" );
	}
	else if ( ( event_base == IP_EVENT ) && ( event_id == IP_EVENT_STA_GOT_IP ) )
	{
		ip_event_got_ip_t		*event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI( TAG, "got ip:" IPSTR, IP2STR( &event->ip_info.ip ) );
		s_retry_num = 0;
		xEventGroupSetBits( s_wifi_event_group, WIFI_CONNECTED_BIT );
	}
}

static void wifi_init_sta( void )
{
	wifi_init_config_t			cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_event_handler_instance_t		instance_any_id;
	esp_event_handler_instance_t		instance_got_ip;
	
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK( esp_netif_init() );
	ESP_ERROR_CHECK( esp_event_loop_create_default() );
	esp_netif_create_default_wifi_sta();

	ESP_ERROR_CHECK( esp_wifi_init( &cfg ) );

	ESP_ERROR_CHECK( esp_event_handler_instance_register(	WIFI_EVENT,
								ESP_EVENT_ANY_ID,
								&event_handler,
								NULL,
								&instance_any_id ) );
	ESP_ERROR_CHECK(esp_event_handler_instance_register(	IP_EVENT,
								IP_EVENT_STA_GOT_IP,
								&event_handler,
								NULL,
								&instance_got_ip ) );

	wifi_config_t			wifi_config = {
						.sta = {
							.ssid = EXAMPLE_ESP_WIFI_SSID,
							.password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
							.threshold.authmode = WIFI_AUTH_WPA2_PSK,
							.sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            //.sae_h2e_identifier = "", //EXAMPLE_H2E_IDENTIFIER,
						},
					};
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
	ESP_ERROR_CHECK( esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t			bits = xEventGroupWaitBits(	s_wifi_event_group,
									WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
									pdFALSE,
									pdFALSE,
									portMAX_DELAY );

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
	if ( bits & WIFI_CONNECTED_BIT ) 
	{
		ESP_LOGI( TAG, "connected to ap SSID:%s password:%s",
						EXAMPLE_ESP_WIFI_SSID,
						EXAMPLE_ESP_WIFI_PASS );
	} 
	else if ( bits & WIFI_FAIL_BIT ) 
	{
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
						EXAMPLE_ESP_WIFI_SSID, 
						EXAMPLE_ESP_WIFI_PASS );
	}
	else
	{
		ESP_LOGE( TAG, "UNEXPECTED EVENT" );
	}
}

void wifi_init( void )
{
	esp_err_t ret = nvs_flash_init();
	if ( (ret == ESP_ERR_NVS_NO_FREE_PAGES ) || ( ret == ESP_ERR_NVS_NEW_VERSION_FOUND ) )
	{
		ESP_ERROR_CHECK( nvs_flash_erase() );
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK( ret );

	ESP_LOGI( TAG, "ESP_WIFI_MODE_STA" );
	wifi_init_sta();
	xTaskCreatePinnedToCore( monitor_status, "monitor_status", 4096, NULL, 5, NULL, 1 );
	xTaskCreatePinnedToCore( recv_task, "recv_task", 4096, NULL, 5, NULL, 1 );
}



