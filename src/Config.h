//TODO: Add SMS config updates.

#pragma region Pinout
//Feel free to copy this pinout or change for your board if you want to use a different pinout.
#define MODEM_RST               GPIO_NUM_0
#define MODEM_TX                GPIO_NUM_3
#define MODEM_RX                GPIO_NUM_4
#define MODEM_SDA               GPIO_NUM_8
#define MODEM_SDL               GPIO_NUM_9
#define MODEM_PWKEY             GPIO_NUM_1
#define MODEM_PWRON             GPIO_NUM_2
#define GPS_TX                  GPIO_NUM_6
#define GPS_RX                  GPIO_NUM_7
#define GPS_PWR                 GPIO_NUM_10     //Optional - Comment out to disable putting GPS to sleep when not in use.
#define BATTERY_DISABLED_PIN    GPIO_NUM_5      //Optional - Comment out to disable battery power check.
#pragma endregion

#pragma region General
#define RESTORE_DEFAULTS_ON_FLASH           //Whether to restore defaults on flash. Comment out to disable.
#define UPDATE_INTERVAL             60      //The time in seconds between updates to the server.
#define BATTERY_UPDATE_INTERVAL     60 * 5  //Update interval in seconds when on battery. Only used if BATTERY_DISABLED_PIN is defined.
#define MAX_RETRIES                 10      //The number of times to retry upon failure before giving up.
#pragma endregion

#pragma region Modem
#define MODEM_APN               ""      //The APN to be used by the SIM card.
#define MODEM_USERNAME          ""      //The username for the SIM card.
#define MODEM_PASSWORD          ""      //The password for the SIM card.
// #define MODEM_PIN               ""      //The pin used to unlock device specific SIMs. Comment out to disable.
#define TINY_GSM_MODEM_SIM800           //The modem module installed. Leave as TINY_GSM_MODEM_SIM800 for SIM800L.
#define TINY_GSM_RX_BUFFER      1024    //Don't change.
#define RESPOND_TO_API_CALLS    false   //Whether to respond to API calls via SMS.
#pragma endregion

#pragma region HTTP
#define SERVER_FQDN                 "192.168.1.226"                         //The server to send data to.
#define SERVER_PORT                 8123                                    //The port on the server to send data to.
#define SERVER_PATH                 "/api/states/device_tracker.esp_gps"    //The path on the server to send data to.
#define SERVER_SSL                  false                                   //Whether to use SSL (HTTPS) or not.
#define SERVER_METHOD               1                                       //The HTTP method to use. 1 = POST.
#define SERVER_HEADER_CONTENT_TYPE  "application/json"                      //The content type to send to the server.
/* SERVER_HEADER_AUTHORIZATION:
 * The authorization header to send to the server.
 * Comment out to disable authorization.
 */
#define SERVER_HEADER_AUTHORIZATION "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiIwYzMwYjVmZmJlZjU0ZmZkYTRkMjQzMDg1MWMzMjUwZiIsImlhdCI6MTcyNDA3NTUxMiwiZXhwIjoyMDM5NDM1NTEyfQ.er9xlmVez8jMc0fYSbxZQ3pBv48MwahA-5iflVEZ3H4"
/* SERVER_DATA_FORMAT:
 * The format to send data to the server in.
 * Values from sensors should be wrapped by {{}}.
 * Avaliable sensors are: latitude, longitude, altitude, accuracy.
 * (Nested quotes must be escaped with a backslash).
 * Example: "{\"latitude\":{{latitude}},\"longitude\":{{longitude}},\"accuracy\":{{accuracy}}}"
 */
#define SERVER_DATA_FORMAT "{\"state\":\"unknown\",\"attributes\":{\"source_type\":\"gps\",\"latitude\":{{latitude}},\"longitude\":{{longitude}},\"gps_accuracy\":{{accuracy}}}}"
/* SERVER_ADDITIONAL_HEADERS:
 * Additional headers to send to the server.
 * Additonal headers should be separated by a newline character (\n).
 * Comment out to disable.
 */
// #define SERVER_ADDITIONAL_HEADERS ""
#pragma endregion
