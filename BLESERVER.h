#pragma once



#ifndef BLESERVER_H
#define BLESERVER_H
#include <Arduino.h>

enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE, HTTP_OPTIONS };
enum HTTPUploadStatus {
	UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END,
	UPLOAD_FILE_ABORTED
};
enum HTTPClientStatus { HC_NONE, HC_WAIT_READ, HC_WAIT_CLOSE };

#define HTTP_DOWNLOAD_UNIT_SIZE 1460
#define HTTP_UPLOAD_BUFLEN 2048
#define HTTP_MAX_DATA_WAIT 1000 //ms to wait for the client to send the request
#define HTTP_MAX_POST_WAIT 1000 //ms to wait for POST data to arrive
#define HTTP_MAX_CLOSE_WAIT 2000 //ms to wait for the client to close the connection

#define CONTENT_LENGTH_UNKNOWN ((size_t) -1)
#define CONTENT_LENGTH_NOT_SET ((size_t) -2)



namespace fs {
	class FS;
}


class BLESERVER {
public:
	//	BLESERVER(String name, int port = 80);
	BLESERVER();
	~BLESERVER();

	void begin();
	void handleClient();

	typedef void(*THandlerFunction)(void);

	void on( char* uri, THandlerFunction handler);

	String arg(String name);        // get request argument value by name
	String arg(int i);              // get request argument value by number
	String argName(int i);          // get request argument name by number
	int args();                     // get arguments count
	bool hasArg(String name);       // check if argument exists


	void send(int code, const String& content_type, const String& content);


protected:
#define MAX_HANDLER 20
	byte Handler_count = 0;
	char   Handler_u[MAX_HANDLER][3];
	THandlerFunction Handler_f[MAX_HANDLER];

};
#endif //ESP8266WEBSERVER_H


