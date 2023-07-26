/*  WiFi softAP Example
 
   This example code is in the Public Domain (or CC0 licensed, at your option.)
 
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
 
#include "lwip/err.h"
#include "lwip/sys.h"
 
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "lwip/sockets.h"
#include <lwip/netdb.h>
#include "apps/dhcpserver/dhcpserver.h"
#include "apps/dhcpserver/dhcpserver_options.h"


#include "commands.h"
 
/* The examples use WiFi configuration that you can set via project configuration menu.
 
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/

#define ESP_WIFI_SSID			"ROVER"
#define ESP_WIFI_PASS			"kikimora"
#define ESP_WIFI_CHANNEL		( 8 )
 
#define CONFIG_IPV4
#define PORT				( 2222 )
 
#define MY_IP_ADDR			"192.168.4.1"
 
 

#define MAX_CONNECTIONS			( 4 )
 
static const char		*TAG = "wifi softAP";
static unsigned int		current_conn = 0;
static esp_netif_t		*netif_hndl = NULL;


 
static void wifi_event_handler(		void			*arg,
					esp_event_base_t	event_base,
					int32_t			event_id,
					void			*event_data)
{
	if ( event_id == WIFI_EVENT_AP_STACONNECTED )
	{
		wifi_event_ap_staconnected_t		*event = ( wifi_event_ap_staconnected_t * ) event_data;
		ESP_LOGI( TAG, "station "MACSTR" join, AID=%d", MAC2STR( event->mac ), event->aid );
	}
	else if ( event_id == WIFI_EVENT_AP_STADISCONNECTED )
	{
		wifi_event_ap_stadisconnected_t		*event = ( wifi_event_ap_stadisconnected_t * ) event_data;
		ESP_LOGI( TAG, "station "MACSTR" leave, AID=%d", MAC2STR( event->mac ), event->aid );
	}
}

static esp_netif_t *__esp_netif_create_default_wifi_ap( void )
{
	esp_netif_config_t cfg = ESP_NETIF_DEFAULT_WIFI_AP();

//	cfg.base->flags = ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP;
	esp_netif_t *netif = esp_netif_new( &cfg );

	assert( netif );
	esp_netif_attach_wifi_ap( netif );
	esp_wifi_set_default_wifi_ap_handlers();
	return netif;
}


static void wifi_init_softap( void )
{
	esp_err_t			ret = ESP_FAIL;

	ESP_ERROR_CHECK( esp_netif_init() );
	ESP_ERROR_CHECK( esp_event_loop_create_default() );
	netif_hndl = __esp_netif_create_default_wifi_ap();
 
	wifi_init_config_t		cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init( &cfg ) );
 
	ESP_ERROR_CHECK( esp_event_handler_register( WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL ) );
 
	wifi_config_t	wifi_config = {
		.ap = {
			.ssid = ESP_WIFI_SSID,
			.password = ESP_WIFI_PASS,
			.ssid_len = strlen( ESP_WIFI_SSID ),
			.channel = ESP_WIFI_CHANNEL,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK,
			.ssid_hidden = 0,
			.max_connection = MAX_CONNECTIONS
		},
	};

	if ( strlen( ESP_WIFI_PASS ) == 0 )
	{
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	esp_netif_ip_info_t		ip_info;

	esp_netif_dhcpc_stop( netif_hndl );

	IP4_ADDR( &ip_info.ip, 192, 168, 4, 1 );
	IP4_ADDR( &ip_info.gw, 192, 168, 4, 1 );
	IP4_ADDR( &ip_info.netmask, 255, 255, 255, 0 );

	ret = esp_netif_set_ip_info( netif_hndl, &ip_info );
	printf( "set ip ret: %d\n", ret ); //set static IP

	ESP_ERROR_CHECK( esp_wifi_set_mode( WIFI_MODE_AP ) );
	ESP_ERROR_CHECK( esp_wifi_set_config( WIFI_IF_AP, &wifi_config ) );
	ESP_ERROR_CHECK( esp_wifi_start() );
/*
	dhcps_lease_t			dhcps_poll = {
						.enable = true
					};
	uint32_t			len = sizeof(dhcps_poll);

	IP4_ADDR( &dhcps_poll.start_ip, 192, 168, 200, 2 );
	IP4_ADDR( &dhcps_poll.end_ip, 192, 168, 200, 20 );
	esp_netif_dhcps_option( netif_hndl, ESP_NETIF_OP_SET, REQUESTED_IP_ADDRESS, &dhcps_poll, len );
*/
	esp_netif_dhcps_start( netif_hndl );
	ESP_LOGI( TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
				ESP_WIFI_SSID,
				ESP_WIFI_PASS,
				ESP_WIFI_CHANNEL );
}

extern void stop();
extern void forward();
extern void backward();
extern void right();
extern void left();



static void do_stuff( void *pvParameters )
{
	int				len;
	int				*s = ( int * ) pvParameters;
	int				sock = ( int ) *s;
	unsigned char			rx_buffer[128];
 
	while ( 1 )
	{
		len = recv( sock, rx_buffer, ( sizeof( rx_buffer ) - 1 ), 0 );
		if ( len < 0 ) 
		{
			ESP_LOGE( TAG, "Error occurred during receiving %d : errno %d %s", sock, errno, strerror( errno ) );
		}
		else if (len == 0)
		{
			ESP_LOGW( TAG, "Connection closed" );
			break;
		}
		else 
		{
			commands_t	cmd;
			memset( &cmd, 0, sizeof( commands_t ) );
			memcpy( &cmd, rx_buffer, sizeof( commands_t ) );

			switch ( cmd.cmd_type )
			{
				case TYPE_CMD:
						if ( ( 0 == cmd.cmds[LEFT_X_DATA] ) && ( 0 == cmd.cmds[LEFT_Y_DATA] ) )
						{
			printf( "Received1\n" );
							stop();
						}
						if ( ( 0 != cmd.cmds[LEFT_X_DATA] ) && ( 0 == cmd.cmds[LEFT_Y_DATA] ) )
						{
			printf( "Received2\n" );
							if ( 0 > cmd.cmds[LEFT_X_DATA] )
								left();
							else
								right();
						}
						if ( ( 0 == cmd.cmds[LEFT_X_DATA] ) && ( 0 != cmd.cmds[LEFT_Y_DATA] ) )
						{
			printf( "Received3\n" );
							if ( 0 > cmd.cmds[LEFT_Y_DATA] )
								forward();
							else
								backward();
						}
					break;
				case TYPE_VIDEO:
					break;
				default:
					break;


			
			}
/*			rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
			ESP_LOGI( TAG, "Received %d bytes: %s", len, rx_buffer );
 
			int		to_write = len;
			while ( to_write > 0 )
			{
				int written = send( sock, ( rx_buffer + ( len - to_write ) ), to_write, 0 );
				if ( written < 0 )
				{
					ESP_LOGE( TAG, "Error occurred during sending: errno %d", errno );
				}
				to_write -= written;
			}
*/
		}
	}
        shutdown( sock, 0 );
        close( sock );
	vTaskDelete( NULL );
}
 
static void tcp_server_task( void *pvParameters )
{
	char				addr_str[128];
	int				addr_family;
	int				ip_protocol;
	int				listen_sock = -1;

	struct sockaddr_in		dest_addr;
	struct sockaddr_in		client_addr;

	//dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	dest_addr.sin_addr.s_addr = inet_addr( MY_IP_ADDR );
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons( PORT );
	addr_family = AF_INET;
	ip_protocol = IPPROTO_IP;
	inet_ntoa_r( dest_addr.sin_addr, addr_str, ( sizeof( addr_str ) - 1 ) );
	ESP_LOGI( TAG, "My IP : %s", addr_str );
 

	listen_sock = socket( addr_family, SOCK_STREAM, ip_protocol );
	if ( listen_sock < 0 )
	{
		ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
		vTaskDelete(NULL);
		return;
	}
	ESP_LOGI(TAG, "Socket created");
 
	int				err = -1;

	err = bind( listen_sock, ( struct sockaddr * )&dest_addr, sizeof( dest_addr ) );
	if ( err != 0 )
	{
		ESP_LOGE( TAG, "Socket unable to bind: errno %d", errno );
		goto CLEAN_UP;
	}
	ESP_LOGI( TAG, "Socket bound, port %d", PORT );
 
	err = listen( listen_sock, MAX_CONNECTIONS );
	if ( err != 0 )
	{
		ESP_LOGE( TAG, "Error occurred during listen: errno %d", errno );
		goto CLEAN_UP;
	}
 
	while ( 1 )
	{
 
		ESP_LOGI( TAG, "Socket listening" );

		int			addr_len = sizeof( client_addr );
		int			sock = -1;

		sock = accept( listen_sock, ( struct sockaddr * )&client_addr, ( socklen_t * )&addr_len );
		if ( sock < 0 ) 
		{
			ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
			continue;
		}
		inet_ntop( AF_INET, &client_addr.sin_addr.s_addr, addr_str, sizeof( addr_str ) );
		ESP_LOGI( TAG, "Server : %s client connected.\n", addr_str );

		xTaskCreatePinnedToCore( do_stuff, "do_stuff", 4096, ( void * ) &sock, 5, NULL, 1 );
	}
 
CLEAN_UP:
	close( listen_sock );
	vTaskDelete( NULL );
}
 
void init_ap()
{
	esp_err_t		ret = nvs_flash_init();

	if ( ( ret == ESP_ERR_NVS_NO_FREE_PAGES ) || ( ret == ESP_ERR_NVS_NEW_VERSION_FOUND ) )
	{
		ESP_ERROR_CHECK( nvs_flash_erase() );
		ret = nvs_flash_init();
	}

	ESP_ERROR_CHECK( ret );
 
	ESP_LOGI( TAG, "ESP_WIFI_MODE_AP" );
	wifi_init_softap();
	xTaskCreatePinnedToCore( tcp_server_task, "tcp_server", 4096, NULL, 5, NULL, 0 );
}





