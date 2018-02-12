#include "BLESERVER.h"
#include <bluefruit.h>
#include "defines.h"
// Peripheral uart service
BLEUart bleuart;

// Central uart client
BLEClientUart clientUart;


String argname[20];
String argval[20];
byte narg;

void startAdv(void) {
	// Advertising packet
	Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
	Bluefruit.Advertising.addTxPower();

	// Include bleuart 128-bit uuid
	Bluefruit.Advertising.addService(bleuart);

	// Secondary Scan Response packet (optional)
	// Since there is no room for 'Name' in Advertising packet
	Bluefruit.ScanResponse.addName();

	/* Start Advertising
	* - Enable auto advertising if disconnected
	* - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
	* - Timeout for fast mode is 30 seconds
	* - Start(timeout) with timeout = 0 will advertise forever (until connected)
	*
	* For recommended advertising interval
	* https://developer.apple.com/library/content/qa/qa1931/_index.html
	*/
	Bluefruit.Advertising.restartOnDisconnect(true);
	Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
	Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
	Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}


/*------------------------------------------------------------------*/
/* Peripheral
*------------------------------------------------------------------*/
void prph_connect_callback(uint16_t conn_handle) {
	char peer_name[32] = { 0 };
	Bluefruit.Gap.getPeerName(conn_handle, peer_name, sizeof(peer_name));

	DEBUG_PRINT("[Prph] Connected to ");
	DEBUG_PRINTLN(peer_name);
}

void prph_disconnect_callback(uint16_t conn_handle, uint8_t reason) {
	(void)conn_handle;
	(void)reason;

	DEBUG_PRINTLN();
	DEBUG_PRINTLN("[Prph] Disconnected");
}
char RXpointer[100]; byte iRX = 0;
void prph_bleuart_rx_callback(void) {
	// Forward data from Mobile to our peripheral
	char str[20 + 1] = { 0 };
	bleuart.read(str, 20);

	DEBUG_PRINT("[Prph] RX: ");
	DEBUG_PRINTLN(str);

	if (clientUart.discovered()) {
		for (byte i = 0; i < 20; i++) 
			RXpointer[iRX + i] = str[i];
		iRX+=20;
	//	clientUart.print(str);
		DEBUG_PRINTLN("sent to Client");
	} else {
		bleuart.print("[Prph] Central role-fagKDFKAjglajhdLH ");
	}
}

/*------------------------------------------------------------------*/
/* Central
*------------------------------------------------------------------*/
void scan_callback(ble_gap_evt_adv_report_t* report) {
	// Check if advertising contain BleUart service
	if (Bluefruit.Scanner.checkReportForService(report, clientUart)) {
		DEBUG_PRINTLN("BLE UART service detected. Connecting ... ");

		// Connect to device with bleuart service in advertising
		Bluefruit.Central.connect(report);
	}
}

void cent_connect_callback(uint16_t conn_handle) {
	char peer_name[32] = { 0 };
	Bluefruit.Gap.getPeerName(conn_handle, peer_name, sizeof(peer_name));

	DEBUG_PRINT("[Cent] Connected to ");
	DEBUG_PRINTLN(peer_name);;

	if (clientUart.discover(conn_handle)) {
		// Enable TXD's notify
		clientUart.enableTXD();
	} else {
		// disconect since we couldn't find bleuart service
		Bluefruit.Central.disconnect(conn_handle);
	}
}

void cent_disconnect_callback(uint16_t conn_handle, uint8_t reason) {
	(void)conn_handle;
	(void)reason;

	DEBUG_PRINTLN("[Cent] Disconnected");
}

/**
* Callback invoked when uart received data
* @param cent_uart Reference object to the service where the data
* arrived. In this example it is clientUart
*/
void cent_bleuart_rx_callback(BLEClientUart& cent_uart) {
	char str[20 + 1] = { 0 };
	cent_uart.read(str, 20);

	DEBUG_PRINT("[Cent] RX: ");
	DEBUG_PRINTLN(str);

	if (bleuart.notifyEnabled()) {
		// Forward data from our peripheral to Mobile
		bleuart.print(str);
	} else {
		// response with no prph message
		clientUart.println("[Cent]Peripheral ");
	}
}



BLESERVER::BLESERVER() {
}


BLESERVER::~BLESERVER() {
}

void BLESERVER::begin() {// Enable both peripheral and central
	Bluefruit.begin(true, true);
	// Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
	Bluefruit.setTxPower(4);
	Bluefruit.setName("duo1");

	// Callbacks for Peripheral
	Bluefruit.setConnectCallback(prph_connect_callback);
	Bluefruit.setDisconnectCallback(prph_disconnect_callback);

	// Callbacks for Central
	Bluefruit.Central.setConnectCallback(cent_connect_callback);
	Bluefruit.Central.setDisconnectCallback(cent_disconnect_callback);

	// Configure and Start BLE Uart Service
	bleuart.begin();

	bleuart.setRxCallback(prph_bleuart_rx_callback);

	// Init BLE Central Uart Serivce
	clientUart.begin();
	clientUart.setRxCallback(cent_bleuart_rx_callback);


	/* Start Central Scanning
	* - Enable auto scan if disconnected
	* - Interval = 100 ms, window = 80 ms
	* - Filter only accept bleuart service
	* - Don't use active scan
	* - Start(timeout) with timeout = 0 will scan forever (until connected)
	*/
	Bluefruit.Scanner.setRxCallback(scan_callback);
	Bluefruit.Scanner.restartOnDisconnect(true);
	Bluefruit.Scanner.setInterval(160, 80); // in unit of 0.625 ms
	Bluefruit.Scanner.filterUuid(bleuart.uuid);
	Bluefruit.Scanner.useActiveScan(false);
	Bluefruit.Scanner.start(0);                   // 0 = Don't stop scanning after n seconds

												  // Set up and start advertising
	startAdv();

}

void BLESERVER::handleClient() {
	if (iRX == 0)return;
	if (RXpointer[iRX] != 0)return;
	char * str;
	str = strtok(RXpointer, "?\0");
	if (str == NULL)return;
	str = strtok(NULL, "=\0");
	while (str != NULL) {
		argname[narg] = str;
		argval[narg++] = strtok(NULL, "&\0");
		str = strtok(NULL, "=\0");

	}
	iRX = 0;
	for (byte i = 0; i < Handler_count; i++) {
		for (byte j = 0; j < 3; j++) {
			if (Handler_u[i][j] != RXpointer[j])break;
			if (j == 3) {
				Handler_f[i]();
				return;
			}
		}
	}
}

void BLESERVER::on(char * uri, THandlerFunction handler) {
	if (Handler_count >= MAX_HANDLER) return;
	for (byte j = 0; j<3; j++) Handler_u[Handler_count][j] = uri[j];
	Handler_f[Handler_count] = handler;
	Handler_count++;
}

String BLESERVER::arg(String name) {
	for (byte i = 0; i < narg; i++)
		if (argname[i] == name) return argval[i];
	return String();
}

String BLESERVER::arg(int i) {
	return argval[i];
}

String BLESERVER::argName(int i) {
	return argname[i];
}

int BLESERVER::args() {
	return narg;
}

bool BLESERVER::hasArg(String name) {
	for (byte i = 0; i < narg; i++)
		if (argname[i] == name) return true;
	return false;
}

void BLESERVER::send(int code, const String & content_type, const String & content) {
}





