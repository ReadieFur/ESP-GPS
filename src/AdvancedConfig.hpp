#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER 1024

//The format to send data to the server in.
//Values from sensors should be wrapped by {{}}.
//Avaliable sensors are: latitude, longitude, altitude, accuracy.
#define SERVER_DATA_FORMAT "{\"state\":\"unknown\",\"attributes\":{\"source_type\":\"gps\",\"latitude\":{{latitude}},\"longitude\":{{longitude}},\"gps_accuracy\":{{accuracy}}}}"
