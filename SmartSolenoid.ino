/* OSBeeWiFi Firmware
 *
 * Main loop wrapper for Arduino
 * June 2016 @ bee.opensprinkler.com
 *
 * This file is part of the OSBeeWiFi library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
//#define DIRECT        for Nffs directory printout
//#define no
#define  NRF_CLOCK_LFCLKSRC {.source        = NRF_CLOCK_LF_SRC_RC,\
	  .rc_ctiv = 16,\
	  .rc_temp_ctiv = 2,\
	  .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM }

#include <Arduino.h>
//#include "defines.h"
#include <Arduino_nRF5x_lowPower.h>
#ifndef no
#ifdef NRF52
#include <bluefruit.h>
#include <Nffs.h>
#endif
#include "BLESERVER.h"
#include <SPI.h>
//#include <LoRa.h>


#include "OSBeeWiFi.h"
#include "program.h"
#include <TimeLib.h>

#define SECS_PER_DAY 24*3600L

#define HTML_OK                0x00
#define HTML_SUCCESS           0x01
#define HTML_UNAUTHORIZED      0x02
#define HTML_MISMATCH          0x03
#define HTML_DATA_MISSING      0x10
#define HTML_DATA_OUTOFBOUND   0x11
#define HTML_PAGE_NOT_FOUND    0x20
#define HTML_FILE_NOT_FOUND    0x21
#define HTML_NOT_PERMITTED     0x30
#define HTML_UPLOAD_FAILED     0x40
#define HTML_WRONG_MODE        0x50
#define HTML_REDIRECT_HOME     0xFF

OSBeeWiFi osb;
ProgramData pd;
static bool time_set = false;


BLESERVER server;

#ifdef DIRECT
#include <bluefruit.h>
#include <Nffs.h>


#define MAX_LEVEL   2
void printTreeDir(const char* cwd, uint8_t level) {
	// Open the input folder
	NffsDir dir(cwd);

	// Print root
	if (level == 0) DEBUG_PRINTLN("root");

	// File Entry Information which hold file attribute and name
	NffsDirEntry dirEntry;

	// Loop through the directory
	while (dir.read(&dirEntry)) {
		// Indentation according to dir level
		for (int i = 0; i<level; i++) DEBUG_PRINT("|  ");

		DEBUG_PRINT("|_ ");

		char eName[64];
		dirEntry.getName(eName, sizeof(eName));

		char fullpath[256];
		strcpy(fullpath, cwd);
		strcat(fullpath, "/");
		strcat(fullpath, eName);

		DEBUG_PRINT(eName);

		if (dirEntry.isDirectory()) {
			DEBUG_PRINTLN("/");

			// ATTENTION recursive call to print sub folder with level+1 !!!!!!!!
			// High number of MAX_LEVEL can cause memory overflow
			if (level < MAX_LEVEL) {
				printTreeDir(fullpath, level + 1);
			}
		} else {
			// Print file size starting from position 50
			int pos = level * 3 + 3 + strlen(eName);

			// Print padding
			for (int i = pos; i<50; i++) DEBUG_PRINT(' ');

			// Print at least one extra space in case current position > 50
			DEBUG_PRINT(' ');

			NffsFile file(fullpath);

			DEBUG_PRINT(file.size());
			DEBUG_PRINTLN(" Bytes");

			file.close();
		}
	}

	dir.close();
}
#endif


static String scanned_ssids;
static bool curr_cloud_access_en = false;
static ulong restart_timeout = 0;
static byte disp_mode = DISP_MODE_IP;
static byte curr_mode;
static ulong& curr_utc_time = OSBeeWiFi::curr_utc_time;

void reset_zones();
void start_manual_program(byte, uint16_t);
void start_testzone_program(byte, uint16_t);
void start_quick_program(uint16_t[]);
void start_program(byte);

String two_digits(uint8_t x) {
	return String(x / 10) + (x % 10);
}

String toHMS(ulong t) {
	return two_digits(t / 3600) + ":" + two_digits((t / 60) % 60) + ":" + two_digits(t % 60);
}

void server_send_html(String html) {
	server.send(200, "text/html", html);
}

void server_send_result(byte code, const char* item = NULL) {
	String html = F("{\"result\":");
	html += code;
	html += F(",\"item\":\"");
	if (item) html += item;
	html += "\"";
	html += "}";
	server_send_html(html);
}

bool get_value_by_key(const char* key, long& val) {
	if (server.hasArg(key)) {
		val = server.arg(key).toInt();
		return true;
	} else {
		return false;
	}
}

bool get_value_by_key(const char* key, String& val) {
	if (server.hasArg(key)) {
		val = server.arg(key);
		return true;
	} else {
		return false;
	}
}

void append_key_value(String& html, const char* key, const ulong value) {
	html += "\"";
	html += key;
	html += "\":";
	html += value;
	html += ",";
	DEBUG_PRINT(html); DEBUG_PRINTLN(value);
}

void append_key_value(String& html, const char* key, const int16_t value) {
	html += "\"";
	html += key;
	html += "\":";
	html += value;
	html += ",";
}

void append_key_value(String& html, const char* key, const String& value) {
	html += "\"";
	html += key;
	html += "\":\"";
	html += value;
	html += "\",";
}
void append_key_value(String& html, const char* key, const char* value) {

	html += "\"";
	html += key;
	html += "\":\"";
	html += value;
	html += "\",";
}
char dec2hexchar(byte dec) {
	if (dec<10) return '0' + dec;
	else return 'A' + (dec - 10);
}

String get_mac() {
	static String hex;
	if (!hex.length()) {
		byte mac[6];
		//    WiFi.macAddress(mac);

		for (byte i = 0; i<6; i++) {
			hex += dec2hexchar((mac[i] >> 4) & 0x0F);
			hex += dec2hexchar(mac[i] & 0x0F);
		}
	}
	return hex;
}

String get_ap_ssid() {
	static String ap_ssid;
	if (!ap_ssid.length()) {
		byte mac[6];
		//    WiFi.macAddress(mac);
		ap_ssid += "OSB_";
		for (byte i = 3; i<6; i++) {
			ap_ssid += dec2hexchar((mac[i] >> 4) & 0x0F);
			ap_ssid += dec2hexchar(mac[i] & 0x0F);
		}
	}
	return ap_ssid;
}

bool verify_dkey() {
	//if(curr_mode == OSB_MOD_AP) {
	//  server_send_result(HTML_WRONG_MODE);
	//  return false;
	// }

	if (server.hasArg(F("dkey"))) {
		String comps = osb.options[OPTION_DKEY].sval;
		if (server.arg(F("dkey")) == comps)
			return true;
		server_send_result(HTML_UNAUTHORIZED);
		return false;
	}
	return true;
}

int16_t get_pid() {
	// if(curr_mode == OSB_MOD_AP) return -2;
	long v;
	if (get_value_by_key("pid", v)) {
		return v;
	} else {
		server_send_result(HTML_DATA_MISSING, "pid");
		return -2;
	}
}
/*
void on_home()
{
if(curr_mode == OSB_MOD_AP) {
server_send_html((connect_html));
} else {
server_send_html((index_html));
}
}

void on_sta_view_options() {
if(curr_mode == OSB_MOD_AP) return;
server_send_html((settings_html));
}

void on_sta_view_manual() {
if(curr_mode == OSB_MOD_AP) return;
server_send_html((manual_html));
}

void on_sta_view_logs() {
if(curr_mode == OSB_MOD_AP) return;
server_send_html((log_html));
}

void on_sta_view_program() {
if(curr_mode == OSB_MOD_AP) return;
server_send_html((program_html));
}

void on_sta_view_preview() {
if(curr_mode == OSB_MOD_AP) return;
server_send_html((preview_html));
}

void on_ap_scan() {
if(curr_mode == OSB_MOD_STA) return;
server_send_html(scanned_ssids);
}

void on_ap_change_config() {
if(curr_mode == OSB_MOD_STA) return;
if(server.hasArg("ssid")) {
osb.options[OPTION_SSID].sval = server.arg("ssid");
osb.options[OPTION_PASS].sval = server.arg("pass");
osb.options[OPTION_AUTH].sval = server.arg("auth");
if(osb.options[OPTION_SSID].sval.length() == 0) {
server_send_result(HTML_DATA_MISSING, "ssid");
return;
}
osb.options_save();
server_send_result(HTML_SUCCESS);
osb.state = OSB_STATE_TRY_CONNECT;
}
}
*/
String get_zone_names_json() {
	String str = F("\"zons\":[");
	for (byte i = 0; i<MAX_NUMBER_ZONES; i++) {
		str += "\"";
		str += osb.options[OPTION_ZON0 + i].sval;
		str += "\",";
	}
	str.remove(str.length() - 1);
	str += "]";
	return str;
}
long sleepTime = 0;
void on_sta_controller() {
	// if(curr_mode == OSB_MOD_AP) return;
	String html;
	html += "{";
	append_key_value(html, "fwv", (int16_t)osb.options[OPTION_FWV].ival);
	append_key_value(html, "sot", (int16_t)osb.options[OPTION_SOT].ival);
	append_key_value(html, "utct", curr_utc_time);
	append_key_value(html, "pid", (int16_t)pd.curr_prog_index);
	append_key_value(html, "tid", (int16_t)pd.curr_task_index);
	append_key_value(html, "np", (int16_t)pd.nprogs);
	append_key_value(html, "nt", (int16_t)pd.scheduled_ntasks);
	append_key_value(html, "mnp", (int16_t)MAX_NUM_PROGRAMS);
	append_key_value(html, "prem", pd.curr_prog_remaining_time);
	append_key_value(html, "trem", pd.curr_task_remaining_time);
	append_key_value(html, "zbits", (int16_t)osb.curr_zbits);
	append_key_value(html, "name", osb.options[OPTION_NAME].sval);
	append_key_value(html, "mac", get_mac());
	append_key_value(html, "sleep", (ulong)sleepTime);
	//  append_key_value(html, "rssi", (int16_t)WiFi.RSSI());
	html += get_zone_names_json();
	html += "}";
	server_send_html(html);
}

void on_sta_logs() {
	// if(curr_mode == OSB_MOD_AP) return;
	String html = "{";
	append_key_value(html, "name", osb.options[OPTION_NAME].sval);

	html += F("\"logs\":[");
	if (!osb.read_log_start()) {
		html += F("]}");
		server_send_html(html);
		return;
	}
	LogStruct l;
	bool remove_comma = false;
	for (uint16_t i = 0; i<MAX_LOG_RECORDS; i++) {
		if (!osb.read_log_next(l)) break;
		if (!l.tstamp) continue;
		html += "[";
		html += l.tstamp;
		html += ",";
		html += l.dur;
		html += ",";
		html += l.event;
		html += ",";
		html += l.zid;
		html += ",";
		html += l.pid;
		html += ",";
		html += l.tid;
		html += "],";
		remove_comma = true;
	}
	osb.read_log_end();
	if (remove_comma) html.remove(html.length() - 1); // remove the extra ,
	html += F("],");
	html += get_zone_names_json();
	html += "}";
	server_send_html(html);
}

void on_sta_change_controller() {
	if (!verify_dkey())  return;
	if (server.hasArg("t")) {
		long time;
		get_value_by_key("t", time);
		curr_utc_time = time - (ulong)osb.options[OPTION_TMZ].ival * 900L + 43200L;
		setTime(curr_utc_time);
		time_set = true;
	}
	if (server.hasArg(F("reset"))) {
		server_send_result(HTML_SUCCESS);
		reset_zones();
	}
	if (server.hasArg(F("FactReset"))) {
		server_send_result(HTML_SUCCESS);
		osb.options_reset;
		restart_timeout = millis() + 1000;
		osb.state = OSB_STATE_RESTART;
	}
	if (server.hasArg(F("reboot"))) {
		server_send_result(HTML_SUCCESS);
		restart_timeout = millis() + 1000;
		osb.state = OSB_STATE_RESTART;
	}

}

// convert absolute remainder (reference time 1970 01-01) to relative remainder (reference time today)
// absolute remainder is stored in eeprom, relative remainder is presented to web
void drem_to_relative(byte days[2]) {
	byte rem_abs = days[0];
	byte inv = days[1];
	days[0] = (byte)((rem_abs + inv - (osb.curr_loc_time() / SECS_PER_DAY) % inv) % inv);
}

// relative remainder -> absolute remainder
void drem_to_absolute(byte days[2]) {
	byte rem_rel = days[0];
	byte inv = days[1];
	days[0] = (byte)(((osb.curr_loc_time() / SECS_PER_DAY) + rem_rel) % inv);
}

long parse_listdata(const String& s, uint16_t& pos) {
	uint16_t p;
	char tmp[13];
	tmp[0] = 0;
	// copy to tmp until a non-number is encountered
	for (p = pos; p<pos + 12; p++) {
		char c = s.charAt(p);
		if (c == '-' || c == '+' || (c >= '0'&&c <= '9'))
			tmp[p - pos] = c;
		else
			break;
	}
	tmp[p - pos] = 0;
	pos = p + 1;
	return atol(tmp);
}
#ifdef OS217
uint16_t sh_parse_listdata(char **p) {
	char tmp_buffer[50];
	char* pv;
	int i = 0;
	tmp_buffer[i] = 0;
	// copy to tmp_buffer until a non-number is encountered
	for (pv = (*p); pv< (*p) + 10; pv++) {
		if ((*pv) == '-' || (*pv) == '+' || ((*pv) >= '0' && (*pv) <= '9'))
			tmp_buffer[i++] = (*pv);
		else
			break;
	}
	tmp_buffer[i] = 0;
	*p = pv + 1;
	return (uint16_t)atol(tmp_buffer);
}

//byte server_change_program ( char *p )
byte OS_Prog(ProgramStruct &prog, char *p) {
	byte i;

	//   ProgramStruct prog;

	// parse program index
	//   if ( !findKeyVal ( p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ( "pid" ), true ) )
	//  {
	//       return HTML_DATA_MISSING;
	//   }
	//   int pid=atoi ( tmp_buffer );
	//   if ( ! ( pid>=-1 && pid< pd.nprograms ) ) return HTML_DATA_OUTOFBOUND;
	//
	// parse program name
	//   if ( findKeyVal ( p, tmp_buffer, TMP_BUFFER_SIZE, PSTR ( "name" ), true ) )
	//  {
	//        urlDecode ( tmp_buffer );
	//      strncpy ( prog.name, tmp_buffer, PROGRAM_NAME_SIZE );
	//  }
	// else
	// {
	//     strcpy_P ( prog.name, _str_program );
	//     itoa ( ( pid==-1 ) ? ( pd.nprograms+1 ) : ( pid+1 ), prog.name+8, 10 );
	// }

	// do a full string decoding
	//   urlDecode ( p );

	// parse ad-hoc v=[...
	// search for the start of v=[
	char *pv;
	boolean found = false;

	for (pv = p; (*pv) != 0 && pv<p + 100; pv++) {
		if (pv[0] == '[') {
			found = true;
			break;
		}
	}

	if (!found)  return HTML_DATA_MISSING;
	pv += 1;
	// parse headers
	//parse flags to be modified
	// * ( char* ) ( &prog ) 
	int v = sh_parse_listdata(&pv);
	// parse config bytes
	prog.enabled = v & 0x01;
	prog.sttype = (v >> 1) & 0x01;
	prog.restr = (v >> 2) & 0x03;
	prog.daytype = (v >> 4) & 0x03;
	//	prog.days[0] = (v >> 8) & 0xFF;
	//	prog.days[1] = (v >> 16) & 0xFF;
	//	byte u=(byte)prog;
	//	DEBUG_PRINTLN(u);
	prog.days[0] = sh_parse_listdata(&pv);
	prog.days[1] = sh_parse_listdata(&pv);
	DEBUG_PRINTLN(prog.days[0]);
	if (prog.daytype == DAY_TYPE_INTERVAL && prog.days[1] > 1) {
		drem_to_absolute(prog.days);
	}
	// parse start times
	pv++; // this should be a '['
	for (i = 0; i<MAX_NUM_STARTTIMES; i++) {
		prog.starttimes[i] = sh_parse_listdata(&pv) / 60;
		DEBUG_PRINTLN(prog.starttimes[i]);
	}
	pv++; // this should be a ','
	pv++; // this should be a '['
	for (i = 0; i < MAX_NUM_TASKS; i++) {
		if (strlen(pv) <= 2)break;

		prog.tasks[i].dur = sh_parse_listdata(&pv);
		DEBUG_PRINTLN(prog.tasks[i].dur);
		prog.tasks[i].zbits = 1;
	}
	prog.ntasks = i;
	pv++; // this should be a ']'
	pv++; // this should be a ']'
		  // parse program name

		  // i should be equal to os.nstations at this point
		  //   for ( ; i<MAX_NUM_TASKS; i++ )
		  //  {
		  //     prog.tasks[i].dur = 0;     // clear unused field
		  // }

		  // process interval day remainder (relative-> absolute)
		  //if ( prog.type == PROGRAM_TYPE_INTERVAL && prog.days[1] > 1 )
		  // {
		  //     pd.drem_to_absolute ( prog.days );
		  // }

		  //   if ( pid==-1 )
		  //   {
		  //       if ( !pd.add ( &prog ) )
		  //           return HTML_DATA_OUTOFBOUND;
		  //   }
		  //   else
		  //   {
		  //       if ( !pd.modify ( pid, &prog ) )
		  //           return HTML_DATA_OUTOFBOUND;
		  //   }
		  //   return HTML_SUCCESS;
}
#endif
void on_sta_change_program() {
	if (!verify_dkey())  return;
	int16_t pid = get_pid();
	if (!(pid >= -1 && pid<pd.nprogs)) {
		server_send_result(HTML_DATA_OUTOFBOUND, "pid");
		return;
	}

	ProgramStruct prog;
	long v; String json;
	String sv;
	// read /cp according to OpenSprinkler 2.1.7 API
	DEBUG_PRINTLN("/cp"); 
#ifdef OS217
	if (get_value_by_key("v", json)) {
		DEBUG_PRINTLN(json);
		byte res=OS_Prog( prog, (char *)json.c_str());
	}

	// read /cp for OS Bee 2.0 API
	else
#endif
	{
		DEBUG_PRINTLN(F("OS bee"));
		if (get_value_by_key("config", v)) {
			// parse config bytes
			prog.enabled = v & 0x01;
			prog.daytype = (v >> 1) & 0x01;
			prog.restr = (v >> 2) & 0x03;
			prog.sttype = (v >> 4) & 0x03;
			prog.days[0] = (v >> 8) & 0xFF;
			prog.days[1] = (v >> 16) & 0xFF;
			if (prog.daytype == DAY_TYPE_INTERVAL && prog.days[1] > 1) {
				drem_to_absolute(prog.days);
			}
		} else {
			server_send_result(HTML_DATA_MISSING, "config");
			return;
		}


		if (get_value_by_key("sts", sv)) {
			// parse start times
			uint16_t pos = 0;
			byte i;
			for (i = 0; i < MAX_NUM_STARTTIMES; i++) {
				prog.starttimes[i] = parse_listdata(sv, pos);

			}
			if (prog.starttimes[0] < 0) {
				server_send_result(HTML_DATA_OUTOFBOUND, "sts[0]");
				return;
			}
		} else {
			server_send_result(HTML_DATA_MISSING, "sts");
			return;
		}

		if (get_value_by_key("nt", v)) {
			if (!(v > 0 && v < MAX_NUM_TASKS)) {
				server_send_result(HTML_DATA_OUTOFBOUND, "nt");
				return;
			}
			prog.ntasks = v;
		} else {
			server_send_result(HTML_DATA_MISSING, "nt");
			return;
		}

		if (get_value_by_key("pt", sv)) {
			byte i = 0;
			uint16_t pos = 0;
			for (i = 0; i < prog.ntasks; i++) {
				ulong e = parse_listdata(sv, pos);
				prog.tasks[i].zbits = e & 0xFF;
				prog.tasks[i].dur = e >> 8;
			}
		} else {
			server_send_result(HTML_DATA_MISSING, "pt");
			return;
		}
	}
	if (!get_value_by_key("name", sv)) {
		sv = F("Program ");
		sv += (pid == -1) ? (pd.nprogs + 1) : (pid + 1);
	}
	strncpy(prog.name, sv.c_str(), PROGRAM_NAME_SIZE);
	prog.name[PROGRAM_NAME_SIZE - 1] = 0;

	if (pid == -1) pd.add(&prog);
	else pd.modify(pid, &prog);

	server_send_result(HTML_SUCCESS);
}

void on_sta_delete_program() {
	if (!verify_dkey())  return;
	int16_t pid = get_pid();
	if (!(pid >= -1 && pid<pd.nprogs)) { server_send_result(HTML_DATA_OUTOFBOUND, "pid"); return; }

	if (pid == -1) pd.eraseall();
	else pd.del(pid);
	server_send_result(HTML_SUCCESS);
}

void on_sta_run_program() {
	if (!verify_dkey())  return;
	int16_t pid = get_pid();
	long v = 0;
	switch (pid) {
	case MANUAL_PROGRAM_INDEX:
	{
		if (get_value_by_key("zbits", v)) {
			byte zbits = v;
			if (get_value_by_key("dur", v)) start_manual_program(zbits, (uint16_t)v);
			else { server_send_result(HTML_DATA_MISSING, "dur"); return; }
		} else { server_send_result(HTML_DATA_MISSING, "zbits"); return; }
	}
	break;

	case QUICK_PROGRAM_INDEX:
	{
		String sv;
		if (get_value_by_key("durs", sv)) {
			uint16_t pos = 1;
			uint16_t durs[MAX_NUMBER_ZONES];
			bool valid = false;
			for (byte i = 0; i<MAX_NUMBER_ZONES; i++) {
				durs[i] = (uint16_t)parse_listdata(sv, pos);
				if (durs[i]) valid = true;
			}
			if (!valid) { server_send_result(HTML_DATA_OUTOFBOUND, "durs"); return; } else start_quick_program(durs);
		} else { server_send_result(HTML_DATA_MISSING, "durs"); return; }
	}
	break;

	case TESTZONE_PROGRAM_INDEX:
	{
		if (get_value_by_key("zid", v)) {
			byte zid = v;
			if (get_value_by_key("dur", v))  start_testzone_program(zid, (uint16_t)v);
			else { server_send_result(HTML_DATA_MISSING, "dur"); return; }
		} else { server_send_result(HTML_DATA_MISSING, "zid"); return; }
	}
	break;

	default:
	{
		if (!(pid >= 0 && pid<pd.nprogs)) { server_send_result(HTML_DATA_OUTOFBOUND, "pid"); return; } else start_program(pid);
	}
	}
	server_send_result(HTML_SUCCESS);
}

void on_sta_change_options() {
	DEBUG_PRINTLN("change opt");
	if (!verify_dkey())  return;
	long ival = 0;
	String sval;
	byte i;
	OptionStruct *o = osb.options;

	// FIRST ROUND: check option validity
	// do not save option values yet
	for (i = 0; i<NUM_OPTIONS; i++, o++) {
		const char *key = o->name.c_str();
		// these options cannot be modified here
		if (i == OPTION_FWV || i == OPTION_MOD || i == OPTION_SSID ||
			i == OPTION_PASS || i == OPTION_DKEY)
			continue;

		if (o->max) {  // integer options
			if (get_value_by_key(key, ival)) {

				if (ival>o->max) {
					server_send_result(HTML_DATA_OUTOFBOUND, key);
					return;
				}
			}
		}

	}


	// Check device key change
	String nkey, ckey;
	const char* _nkey = "nkey";
	const char* _ckey = "ckey";

	if (get_value_by_key(_nkey, nkey)) {
		if (get_value_by_key(_ckey, ckey)) {
			if (!nkey.equals(ckey)) {
				server_send_result(HTML_MISMATCH, _ckey);
				return;
			}
		} else {
			server_send_result(HTML_DATA_MISSING, _ckey);
			return;
		}
	}

	// SECOND ROUND: change option values
	o = osb.options;
	for (i = 0; i<NUM_OPTIONS; i++, o++) {
		const char *key = o->name.c_str();
		// these options cannot be modified here
		if (i == OPTION_FWV || i == OPTION_MOD || i == OPTION_SSID ||
			i == OPTION_PASS || i == OPTION_DKEY)
			continue;

		if (o->max) {  // integer options
			if (get_value_by_key(key, ival)) {
				o->ival = ival;
				DEBUG_PRINTLN(ival);
			}
		} else {
			if (get_value_by_key(key, sval)) {
				o->sval = sval;
				DEBUG_PRINTLN(ival);
			}
		}
	}

	if (get_value_by_key(_nkey, nkey)) {
		osb.options[OPTION_DKEY].sval = nkey;
	}
#ifdef NFR52

	Nffs.remove(CONFIG_FNAME);
#endif  
	osb.options_save();
	server_send_result(HTML_SUCCESS);
}
void on_sta_options() {
	DEBUG_PRINTLN("set options");
	//  if(curr_mode == OSB_MOD_AP) return;
	String html = "{";
	OptionStruct *o = osb.options;
	for (byte i = 0; i<NUM_OPTIONS; i++, o++) {
		DEBUG_PRINTLN(o->name);
		if (!o->max) {
			if (i == OPTION_NAME || i == OPTION_AUTH) {  // only output selected string options
				DEBUG_PRINT("sv="); DEBUG_PRINTLN(o->sval);

				append_key_value(html, o->name.c_str(), o->sval);
			}
		} else {  // if this is a int option
			DEBUG_PRINT("iv="); DEBUG_PRINTLN(o->ival);
			append_key_value(html, o->name.c_str(), (ulong)o->ival);
		}
		DEBUG_PRINTLN(html);
	}
	// output zone names
	html += get_zone_names_json();
	html += "}";
	server_send_html(html);
}

void on_sta_program() {
	String html = "{";
	append_key_value(html, "tmz", (int16_t)osb.options[OPTION_TMZ].ival);
	html += F("\"progs\":[");
	ulong v;
	ProgramStruct prog;
	bool remove_comma = false;
	for (byte pid = 0; pid<pd.nprogs; pid++) {
		html += "{";
		pd.read(pid, &prog);

		v = *(byte*)(&prog);  // extract the first byte
		if (prog.daytype == DAY_TYPE_INTERVAL) {
			drem_to_relative(prog.days);
		}
		v |= ((ulong)prog.days[0] << 8);
		v |= ((ulong)prog.days[1] << 16);
		append_key_value(html, "config", (ulong)v);

		html += F("\"sts\":[");
		byte i;
		for (i = 0; i<MAX_NUM_STARTTIMES; i++) {
			html += prog.starttimes[i];
			html += ",";
		}
		html.remove(html.length() - 1);
		html += "],";

		append_key_value(html, "nt", (int16_t)prog.ntasks);

		html += F("\"pt\":[");
		for (i = 0; i<prog.ntasks; i++) {
			v = prog.tasks[i].zbits;
			v |= ((long)prog.tasks[i].dur << 8);
			DEBUG_PRINT(prog.tasks[i].dur); DEBUG_PRINT((long)prog.tasks[i].dur << 8); DEBUG_PRINTLN(v);
			html += v;
			html += ",";
		}
		html.remove(html.length() - 1);
		html += "],";

		append_key_value(html, "name", prog.name);
		html.remove(html.length() - 1);
		html += F("},");
		remove_comma = true;
	}
	if (remove_comma) html.remove(html.length() - 1);
	html += "]}";
	server_send_html(html);
}

void on_sta_delete_log() {
	if (!verify_dkey())  return;
	osb.log_reset();
	server_send_result(HTML_SUCCESS);
}

const char* weekday_name(ulong t) {
	t /= 86400L;
	t = (t + 3) % 7;  // Jan 1, 1970 is a Thursday
	static const char* weekday_names[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
	return weekday_names[t];
}

void reset_zones() {
	DEBUG_PRINTLN("reset zones");
	osb.clear_zbits();
	osb.apply_zbits();
	pd.reset_runtime();
}

void start_testzone_program(byte zid, uint16_t dur) {
	if (zid >= MAX_NUMBER_ZONES) return;
	pd.reset_runtime();
	TaskStruct *e = &pd.manual_tasks[0];
	e->zbits = (1 << zid);
	e->dur = dur;
	pd.scheduled_stop_times[0] = curr_utc_time + dur;
	pd.curr_prog_index = TESTZONE_PROGRAM_INDEX;
	pd.scheduled_ntasks = 1;
	osb.program_busy = 1;
}

void start_manual_program(byte zbits, uint16_t dur) {
	pd.reset_runtime();
	TaskStruct *e = &pd.manual_tasks[0];
	e->zbits = zbits;
	e->dur = dur;
	pd.scheduled_stop_times[0] = curr_utc_time + dur;
	pd.curr_prog_index = MANUAL_PROGRAM_INDEX;
	pd.scheduled_ntasks = 1;
	osb.program_busy = 1;
}

void schedule_run_program() {
	byte tid;
	ulong start_time = curr_utc_time;
	for (tid = 0; tid<pd.ntasks; tid++) {
		pd.scheduled_stop_times[tid] = start_time + pd.scheduled_stop_times[tid];
		start_time = pd.scheduled_stop_times[tid];
	}
	pd.scheduled_ntasks = pd.ntasks;
	osb.program_busy = 1;
}

void start_quick_program(uint16_t durs[]) {
	pd.reset_runtime();
	TaskStruct *e = pd.manual_tasks;
	byte nt = 0;
	ulong start_time = curr_utc_time;
	for (byte i = 0; i<MAX_NUMBER_ZONES; i++) {
		if (durs[i]) {
			e[i].zbits = (1 << i);
			e[i].dur = durs[i];
			pd.scheduled_stop_times[i] = start_time + e[i].dur;
			start_time = pd.scheduled_stop_times[i];
			nt++;
		}
	}
	if (nt>0) {
		pd.curr_prog_index = QUICK_PROGRAM_INDEX;
		pd.scheduled_ntasks = nt;
		osb.program_busy = 1;
	} else {
		pd.reset_runtime();
	}
}

void start_program(byte pid) {
	ProgramStruct prog;
	byte tid;
	uint16_t dur;
	if (pid >= pd.nprogs) return;
	pd.reset_runtime();
	pd.read(pid, &prog);
	if (!prog.ntasks) return;
	for (tid = 0; tid<prog.ntasks; tid++) {
		dur = prog.tasks[tid].dur;
		pd.scheduled_stop_times[tid] = dur;
	}
	pd.curr_prog_index = pid;
	schedule_run_program();
}

void check_status() {
	static ulong checkstatus_timeout = 0;
	if (curr_utc_time > checkstatus_timeout) {
		if (curr_cloud_access_en /*&& Blynk.connected()*/) {
			byte i, zbits;
			for (i = 0; i<MAX_NUMBER_ZONES; i++) {
				zbits = osb.curr_zbits;
				//  if((zbits>>i)&1) blynk_leds[i]->on();
				// else blynk_leds[i]->off();
			}
			if (osb.program_busy) {
				String str = F("Prog ");
				if (pd.curr_prog_index == TESTZONE_PROGRAM_INDEX) {
					str += F("T");
				} else if (pd.curr_prog_index == QUICK_PROGRAM_INDEX) {
					str += F("Q");
				} else if (pd.curr_prog_index == MANUAL_PROGRAM_INDEX) {
					str += F("M");
				} else {
					str += (pd.curr_prog_index + 1);
				}
				str += F(": ");
				str += toHMS(pd.curr_prog_remaining_time);
				//     blynk_lcd.print(0, 0, str);

				str = F("Task ");
				str += (pd.curr_task_index + 1);
				str += F(": ");
				str += toHMS(pd.curr_task_remaining_time);
				//    blynk_lcd.print(0, 1, str);
			} else {
				//  blynk_lcd.print(0, 0, "[Idle]          ");
				//  blynk_lcd.print(0, 1, get_ip(WiFi.localIP())+F("      "));
			}
		}
		if (osb.program_busy)  checkstatus_timeout = curr_utc_time + 2;  // when program is running, update more frequently
		else checkstatus_timeout = curr_utc_time + 5;
	}
}
#endif
#ifdef TAKEITOUT
//---------------------------
#ifdef NRF52
#include <bluefruit.h>
#include <Nffs.h>
#endif
#include "BLESERVER.h"

BLESERVER server;
void on_prova() { ; }
//-----------------------------
#endif

static ulong connecting_timeout;
byte onoff = 0;
static ulong last_time = 0;
static ulong last_minute = 0;

void setup() {

	{DEBUG_BEGIN(115200); }
	DEBUG_PRINTLN(F("SmartSolenoid"));
#ifndef no
	//while (!Serial.available()) delay(100);
	osb.begin();
	delay(1000);
	DEBUG_PRINTLN("Opt setup");
	osb.options_setup();
	// close all zones at the beginning.
	//for (byte i = 0; i<MAX_NUMBER_ZONES; i++) osb.close(i);


	DEBUG_PRINTLN("PD setup");
	pd.init();
	curr_mode = osb.get_mode();
	delay(1000);
	//curr_cloud_access_en = osb.get_cloud_access_en();

	//  led_blink_ms = LED_FAST_BLINK;  
	//  server.on("/", on_home);
	//  server.on("/index.html", on_home);
	//  server.on("/settings.html", on_sta_view_options);
	//  server.on("/log.html", on_sta_view_logs);
	//  server.on("/manual.html", on_sta_view_manual);
	//  server.on("/program.html", on_sta_view_program);
	//  server.on("/preview.html", on_sta_view_preview);
	// server.on("/update.html", HTTP_GET, on_sta_update);
	//  server.on("/update", HTTP_POST, on_sta_upload_fin, on_sta_upload);

	server.on("/jc", on_sta_controller);
	server.on("/jo", on_sta_options);
	server.on("/jl", on_sta_logs);
	server.on("/jp", on_sta_program);
	server.on("/cc", on_sta_change_controller);
	server.on("/co", on_sta_change_options);
	server.on("/cp", on_sta_change_program);
	server.on("/dp", on_sta_delete_program);
	server.on("/rp", on_sta_run_program);
	server.on("/dl", on_sta_delete_log);

 //   server.on("2", on_prova);
	DEBUG_PRINTLN("BLE setup");
	nRF5x_lowPower.powerMode(POWER_MODE_LOW_POWER);
	DELAYTEST
	server.begin();

	DEBUG_PRINTLN(F("BLE READY"));
#ifdef DIRECT
	printTreeDir("/", 0);
#endif

#endif
#ifdef no
	byte i = 1;
	for (i = 0; i < 10; i++) {
		delay(2000);
		DEBUG_PRINTLN(i);
		pinMode(1, INPUT);
		//digitalWrite(i, 0);
		//delay(5000);
		//digitalWrite(i, 1);

		//pinMode(i, INPUT);
}
#ifdef MYPIN_LED2
	pinMode(MYPIN_LED2, OUTPUT);
#endif
	/*
	while (true) {
#ifdef MYPIN_LED2
		digitalWrite(MYPIN_LED2, onoff & 0x01);
		onoff += 1;
#endif
	
	}
	*/
#endif
	nRF5x_lowPower.powerMode(POWER_MODE_LOW_POWER);       //funziona ma non necessario!!
	DELAYTEST
	DELAYTEST
    DELAYTEST
	//	nRF5x_lowPower.powerMode(POWER_MODE_CONSTANT_LATENCY); //non funziona!!!!!!
		nRF5x_lowPower.enableDCDC();     //riduce a .10 mA (era .25)
	DELAYTEST
		DELAYTEST
		DELAYTEST
		DEBUG_PRINTLN(F("END setup"));
	//nRF5x_lowPower.disableDCDC();   //dovuto a loop stretto ritorna a 7mA ????? deleted ritorna a 3.5???
}
void loop() {
	//delay(100);
	
#ifdef MYPIN_LED2
	digitalWrite(MYPIN_LED2, onoff & 0x01);
	onoff += 1;
#endif
	
	

#ifdef LORA
	server.LoRaReceiver();
#else
	server.handleClient();
	nRF5x_lowPower.powerMode(POWER_MODE_LOW_POWER);
#endif
	curr_utc_time = now();
	//  if(curr_mode == OSB_MOD_STA) {
	// time_keeping();
	check_status();
	// }
	// process_button();
	delay(500);
#ifdef nono

	if (last_time != curr_utc_time) {
		last_time = curr_utc_time;
		//waitForEvent();
		//sleepTime += now() - last_time;
		//  process_display();
		//--------- 1 second granularity------------------------
		// ==== Schedule program data ===
		ulong curr_minute = curr_utc_time / 60;
		byte pid;
		ProgramStruct prog;
		//----------1 minute granularity---------------------------
		if (curr_minute != last_minute) {
			last_minute = curr_minute;
			if (!osb.program_busy) {
				for (pid = 0; pid<pd.nprogs; pid++) {
					pd.read(pid, &prog, true);

					if (prog.check_match(osb.curr_loc_time())) {
						//pd.read(pid, &prog);
						start_program(pid);
						DEBUG_PRINTLN("START");
						break;
					}
				}
			}
		}
		//----------------------1 second granularity-------------------------
		// ==== Run program tasks ====
		// Check there is any program running currently
		// If do, do zone book keeping
		if (osb.program_busy) {
			DEBUG_PRINTLN("RUNNING");
#ifdef  MYPIN_LED2
			digitalWrite(MYPIN_LED2, onoff & 0x01);
#endif
			// check stop time
			if (pd.curr_task_index == -1 ||
				curr_utc_time >= pd.scheduled_stop_times[pd.curr_task_index]) {
				// move on to the next task
				pd.curr_task_index++;
				if (pd.curr_task_index >= pd.scheduled_ntasks) {
					// the program is now over
					DEBUG_PRINTLN("program finished");
					reset_zones();
					osb.program_busy = 0;
					DEBUG_PRINTLN("STOP");

				} else {
					TaskStruct e;
					pd.load_curr_task(&e);
					osb.next_zbits = e.zbits;
					pd.curr_prog_remaining_time = pd.scheduled_stop_times[pd.scheduled_ntasks - 1] - curr_utc_time;
					pd.curr_task_remaining_time = pd.scheduled_stop_times[pd.curr_task_index] - curr_utc_time;
				}
			} else {
				pd.curr_prog_remaining_time = pd.scheduled_stop_times[pd.scheduled_ntasks - 1] - curr_utc_time;
				pd.curr_task_remaining_time = pd.scheduled_stop_times[pd.curr_task_index] - curr_utc_time;
			}
		}
		osb.apply_zbits();
	}
#endif
}

