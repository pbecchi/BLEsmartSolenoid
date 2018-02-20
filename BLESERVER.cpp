

#include "BLESERVER.h"

//#define LORA
#include "defines.h"
#ifdef LORA
#include <SPI.h>              // include libraries
#include <LoRa.h>

const int csPin = 7;          // LoRa radio chip select
const int resetPin = 6;       // LoRa radio reset
const int irqPin = 1;         // change for your board; must be a hardware interrupt pin
#endif
// Peripheral uart service
String argname[10];
String argval[10];
byte narg;
char RXpointer[80]; byte iRX = 0;

#define SER 
#ifdef NRF52
#include <bluefruit.h>

BLEUart bleuart;

// Central uart client
BLEClientUart clientUart;


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

void prph_bleuart_rx_callback(void) {
	// Forward data from Mobile to our peripheral
	char str[20 + 1] = { 0 };
	bleuart.read(str, 20);

	DEBUG_PRINT("[Prph] RX: ");
	DEBUG_PRINTLN(str);

	//if (clientUart.discovered()) {
		for (byte i = 0; i < 20; i++) 
			RXpointer[iRX + i] = str[i];
		iRX+=20;
	//	clientUart.print(str);
	//	DEBUG_PRINTLN("sent to Client");
//	} else {
//		bleuart.print("[Prph] Central role-fagKDFKAjglajhdLH ");
//	}
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

#endif


BLESERVER::BLESERVER() {
}


BLESERVER::~BLESERVER() {
}

void BLESERVER::begin() {// Enable both peripheral and central
#ifdef NRF52
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
#endif
#ifdef LORA
	// override the default CS, reset, and IRQ pins (optional)
	LoRa.setPins(csPin, resetPin, irqPin);// set CS, reset, IRQ pin

	if (!LoRa.begin(915E6)) {             // initialize ratio at 915 MHz
		Serial.println("LoRa init failed. Check your connections.");
		while (true);                       // if failed, do nothing
	}

	Serial.println("LoRa init succeeded.");
#endif
}

void BLESERVER::handleClient() {
	
#ifdef SER
	while (Serial.available()) {
		RXpointer[iRX++] = Serial.read();
		DEBUG_PRINT(RXpointer[iRX - 1]);
	}
	
		if (RXpointer[iRX - 1] >= 20)return;//wait EOL or CR
		RXpointer[iRX] = 0;
		Serial.flush();
#endif
//  decode RX buffer----------------------------------------------------	
//	if (RXpointer[iRX] != 0)return;
		if (iRX == 0) return;
		DEBUG_PRINTLN(RXpointer);
		char * str = "\0";
	str = strtok(RXpointer, "?\0");
	if (str == NULL)return;
	DEBUG_PRINTLN(str);
	str = strtok(NULL, "=\0");
	while (str != NULL) {
		DEBUG_PRINTLN(str);
		argname[narg] = String(str);
		argval[narg++] = strtok(NULL, "&\0");
		str = strtok(NULL, "=\0");
		DEBUG_PRINT(argname[narg-1]); DEBUG_PRINT("="); DEBUG_PRINTLN(argval[narg-1]);
	}

	iRX = 0;
	for (byte i = 0; i < Handler_count; i++) {
		for (byte j = 0; j < 3; j++) {
			if (Handler_u[i][j] != RXpointer[j])break;
			if (j == 2) {
				DEBUG_PRINT("handler "); DEBUG_PRINTLN(i);
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
		if (argname[i] == name) {
			DEBUG_PRINT(i); DEBUG_PRINT(" arg="); DEBUG_PRINTLN(argval[ i ]);
			return argval[i];
		}
	return String();
}

String BLESERVER::arg(int i) {
	DEBUG_PRINT(i); DEBUG_PRINT(" arg="); DEBUG_PRINTLN(argval[i]);
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
	DEBUG_PRINTLN( content);
#ifdef NRF52
	bleuart.print( content);

#endif
#ifdef SER
	Serial.print(content);
#endif
}





