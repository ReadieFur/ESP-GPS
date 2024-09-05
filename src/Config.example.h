#pragma region Includes (dont touch this)
#include "HTTP.hpp"
#if defined(ESP32)
#include <hal/gpio_types.h>
#elif defined(ESP8266)
#include <Arduino.h>
#endif
#pragma endregion

#pragma region Modem
#define MODEM_TX                D3                  //The pin that the modem TX line is connected to.
#define MODEM_RX                D2                  //The pin that the modem RX line is connected to.
#define MODEM_APN               ""                  //The APN to be used by the module.
#define MODEM_PIN               ""                  //The pin to unlock the SIM card.
#define MODEM_USERNAME          ""                  //The username to be used to connect to the GSM network, leave blank for none.
#define MODEM_PASSWORD          ""                  //The password to be used to connect to the GSM network, leave blank for none.
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER      1024
#pragma endregion

#pragma region GPS
#define GPS_TX                  D7                  //The pin that the GPS TX line is connected to.
#define GPS_RX                  D6                  //The pin that the GPS RX line is connected to.
#pragma endregion

#pragma region Data
#define SEND_INTERVAL           5000                //How long in milliseconds to wait between sending status updates.
#define SEND_GPS_LOCATION                           //Sends longitude, latitude, quality and age.
#define SEND_GPS_DATE                               //Date as seen by the GPS.
#define SEND_GPS_TIME                               //UTC time as seen by the GPS.
// #define SEND_GPS_SPEED                              //Estimated speed based on previous GPS values.
// #define SEND_GPS_COURSE                             //Estimated direction in degrees.
// #define SEND_GPS_ALTITUDE                           //Altitude in meters.
#define SEND_GPS_SATELLITES                         //Number of detected satellites.
// #define SEND_GPS_HDOP                               //GPS HDOP.
// #define SEND_BATTERY_SOC                            //Estimated battery state.
#define SEND_BATTERY_PERCENTAGE                     //Estimated battery percentage.
#define SEND_BATTERY_VOLTAGE                        //Estimated battery voltage.
// #define SEND_GSM_OPERATOR                           //The network that the module is connected to.
#define SEND_GSM_SIGNAL_STRENGTH                    //Signal strength to the connected cell tower in rssi.
// #define SEND_GSM_IP                                 //The assigned IP address of the module.
#pragma endregion

#pragma region Networking
#define NET_MQTT                                    //Enable MQTT networking.
#ifdef NET_MQTT
#define MQTT_DEVICE_ID          ""                  //The device ID to use when sending to the broker.
#define MQTT_BROKER             ""                  //MQTT broker IP or domain.
#define MQTT_PORT               1883                //MQTT port.
#define MQTT_USERNAME           ""                  //MQTT Username, leave blank for none.
#define MQTT_PASSWORD           ""                  //MQTT Password, leave blank for none.
#define MQTT_TOPIC              ""                  //The MQTT topic to subscribe to.
#endif

// #define NET_HTTP                                    //Enable HTTP networking.
#ifdef NET_HTTP
#define HTTP_ADDRESS            ""                  //HTTP server IP or domain.
#define HTTP_PORT               80                  //The server port.
#define HTTP_PATH               ""                  //The path to send the HTTP request to.
#define HTTP_METHOD             HTTP::EMethod::GET  //The HTTP method to use for the request.
#endif

//TODO: Websockets and UDP.
#pragma endregion

#pragma region Updates
#pragma endregion
