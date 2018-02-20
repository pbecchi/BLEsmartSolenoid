/* 
	Editor: http://www.visualmicro.com
			visual micro and the arduino ide ignore this code during compilation. this code is automatically maintained by visualmicro, manual changes to this file will be overwritten
			the contents of the Visual Micro sketch sub folder can be deleted prior to publishing a project
			all non-arduino files created by visual micro and all visual studio project or solution files can be freely deleted and are not required to compile a sketch (do not delete your own code!).
			note: debugger breakpoints are stored in '.sln' or '.asln' files, knowledge of last uploaded breakpoints is stored in the upload.vmps.xml file. Both files are required to continue a previous debug session without needing to compile and upload again
	
	Hardware: Adafruit Bluefruit nRF52 Feather, Platform=nrf52, Package=adafruit
*/

#if defined(_VMICRO_INTELLISENSE)

#ifndef _VSARDUINO_H_
#define _VSARDUINO_H_
#define F_CPU 64000000
#define ARDUINO 10804
#define ARDUINO_NRF52_FEATHER
#define ARDUINO_ARCH_NRF52
#define ARDUINO_FEATHER52
#define USE_LFXO
#define CFG_DEBUG 0
#define ARDUINO_NRF52_ADAFRUIT
#define NRF5
#define NRF52
#define S132
#define __cplusplus 201103L
#define __ARM__
#define __arm__
#define __inline__
#define __asm__(x)
#define __attribute__(x)
#define __extension__
typedef int __builtin_va_list;
#define __STATIC_INLINE static inline
#define __ASSEMBLY__
#undef _WIN32
#define __GNUC__ 0
#define __ICCARM__
#define __ARMCC_VERSION 400678

typedef unsigned long __INTPTR_TYPE__;
typedef unsigned long __intptr_t;
typedef unsigned long __INT32_TYPE__;
typedef unsigned long __SIZE_TYPE__;
typedef long __INT32_TYPE__;

#include <stdint.h>





#include <Arduino.h>
#include <pins_arduino.h> 
#include <variant.h> 
#include <variant.cpp> 
#undef cli
#define cli()
#include "SmartSolenoid.ino"
#endif
#endif
