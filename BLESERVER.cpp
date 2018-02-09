#include "BLESERVER.h"



BLESERVER::BLESERVER() {
}


BLESERVER::~BLESERVER() {
}

void BLESERVER::begin() {
}

void BLESERVER::handleClient() {
}

void BLESERVER::on(const char * uri, THandlerFunction handler) {
}

String BLESERVER::arg(String name) {
	return String();
}

String BLESERVER::arg(int i) {
	return String();
}

String BLESERVER::argName(int i) {
	return String();
}

int BLESERVER::args() {
	return 0;
}

bool BLESERVER::hasArg(String name) {
	return false;
}

void BLESERVER::send(int code, const String & content_type, const String & content) {
}
