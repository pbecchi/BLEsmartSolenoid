/* OSBeeWiFi Firmware
 *
 * Program data structures and functions header file
 * December 2016 @ bee.opensprinkler.com
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
#ifndef nocomp
#include <limits.h>
#include <TimeLib.h>
#include "program.h"
#ifdef NRF52
#include <Nffs.h>
#define FILE_PROG
#endif
// Declare static data members
byte ProgramData::ntasks = 0;
byte ProgramData::nprogs = 0;
byte ProgramData::scheduled_ntasks = 0;
TaskStruct ProgramData::manual_tasks[MAX_NUMBER_ZONES];
ulong ProgramData::scheduled_stop_times[MAX_NUM_TASKS];
int8_t ProgramData::curr_prog_index;
int8_t ProgramData::curr_task_index;
ulong ProgramData::curr_prog_remaining_time;
ulong ProgramData::curr_task_remaining_time;

extern const char* prog_fname;

void ProgramData::reset_runtime() {
	for (byte i = 0; i<MAX_NUM_TASKS; i++) {
		scheduled_stop_times[i] = 0;
	}
	curr_task_index = -1;
	curr_prog_index = -1;
	ntasks = 0;
	scheduled_ntasks = 0;
	curr_prog_remaining_time = 0;
	curr_task_remaining_time = 0;
}

#ifdef FILE_PROG
// load station bits
 byte buff[MAX_NUM_PROGRAMS*PROGRAMSTRUCT_SIZE];
void writebuf( byte pos, byte * pointer, byte size) {
	for (byte i = 0; i < size; i++)
		buff[pos + i] = pointer[i];
}
void readbuf( byte pos, byte * result, byte size) {
	for (byte i = 0; i < size; i++)
		result[i] = buff[pos + i];
}
void writefile(NffsFile file) {
	if (file.exists()) {
		Nffs.remove(prog_fname);
		file.open(prog_fname, FS_ACCESS_WRITE);
	}
	file.write(buff, MAX_NUM_PROGRAMS*PROGRAMSTRUCT_SIZE);

}
void ProgramData::load_curr_task(TaskStruct *t) {
	if (!t)  return;
	if (curr_prog_index == MANUAL_PROGRAM_INDEX ||
		curr_prog_index == QUICK_PROGRAM_INDEX ||
		curr_prog_index == TESTZONE_PROGRAM_INDEX) {
		if (curr_task_index<0 || curr_task_index >= scheduled_ntasks) return;
		*t = manual_tasks[curr_task_index];
		return;
	}
	if (curr_prog_index<0 || curr_prog_index >= nprogs) return;
	if (curr_task_index<0 || curr_task_index >= scheduled_ntasks) return;
	NffsFile file;
	file.open(prog_fname, FS_ACCESS_READ);

	if (!file.exists()) return;
	file.read(buff, MAX_NUM_PROGRAMS*PROGRAMSTRUCT_SIZE);
	uint addr = sizeof(nprogs) + (uint)curr_prog_index * PROGRAMSTRUCT_SIZE;
	addr += offsetof(struct ProgramStruct, tasks);
	addr += (uint)curr_task_index * sizeof(TaskStruct);
//	file.seek(addr);
//	file.readBytes((char*)t, sizeof(TaskStruct));
	readbuf(addr,(byte *)t, sizeof(TaskStruct));
	file.close();
}

void ProgramData::load_count() {
	NffsFile file;
   file.open(prog_fname, FS_ACCESS_READ);
  if(!file.exists()) return;
  file.readBytes((char*)&nprogs, sizeof(nprogs));
  if(nprogs>MAX_NUM_PROGRAMS) nprogs=MAX_NUM_PROGRAMS;
  file.close();
}

void ProgramData::save_count(byte nprogs) {
	NffsFile file;
	file.open(prog_fname, FS_ACCESS_READ);
	if (!file.exists()) return;
//  file.seek(0);
//  file.write((const byte*)&nprogs, sizeof(nprogs));
	writebuf(0,( byte*)&nprogs, sizeof(nprogs));
	file.close();
}

void ProgramData::init() {
	reset_runtime();
	NffsFile file;
  if(!Nffs.testFile(prog_fname)) {
    // create program data file
    DEBUG_PRINTLN(F("create program data..."));
	file.open(prog_fname, FS_ACCESS_WRITE);
	if (!file.exists()) {
      DEBUG_PRINTLN(F("failed"));
      return;
    }
	writefile(file);
   // file.write(buff,MAX_NUM_PROGRAMS*PROGRAMSTRUCT_SIZE );
   // byte zero = 0;
   // uint size = PROGRAMSTRUCT_SIZE * MAX_NUM_PROGRAMS;
   // file.write((con, size);
    file.close();
    DEBUG_PRINTLN(F("ok"));
  } else {
    load_count();    
  }
}


// set all programs to empty
void ProgramData::eraseall() {
  nprogs = 0;
  ntasks = 0;
  save_count(nprogs);  
}

// read a program
void ProgramData::read(byte pid, ProgramStruct *buf, bool header_only) {
  if (pid >= nprogs) return;
  NffsFile file;
  file.open(prog_fname, FS_ACCESS_READ);
  if (!file.exists()) return;
  file.read(buff, MAX_NUM_PROGRAMS*PROGRAMSTRUCT_SIZE);
  uint addr = sizeof(nprogs) + (uint)pid * PROGRAMSTRUCT_SIZE;  
//  file.seek(addr);
//  file.readBytes((char*)buf, header_only?offsetof(struct ProgramStruct,tasks) : PROGRAMSTRUCT_SIZE);
  readbuf(addr, (byte*)buf, header_only ? offsetof(struct ProgramStruct, tasks) : PROGRAMSTRUCT_SIZE);
  ntasks=buf->ntasks;
  file.close();
}


// add a program
byte ProgramData::add(ProgramStruct *buf) {
  if (nprogs >= MAX_NUM_PROGRAMS)  return 0;
  NffsFile file;
  file.open(prog_fname, FS_ACCESS_WRITE);
  if (!file.exists()) return 0; 
  file.read(buff, MAX_NUM_PROGRAMS*PROGRAMSTRUCT_SIZE);
  uint addr = sizeof(nprogs) + (uint)nprogs * PROGRAMSTRUCT_SIZE;
//  file.seek(addr);
//  file.write((byte*)buf, PROGRAMSTRUCT_SIZE);
  writebuf(addr,(byte*)buf, PROGRAMSTRUCT_SIZE);
  nprogs ++;
//  file.seek(0);
//  file.write((const byte*)&nprogs, sizeof(nprogs));
  writebuf(0,( byte*)&nprogs, sizeof(nprogs));
//  file.write(buff, MAX_NUM_PROGRAMS*PROGRAMSTRUCT_SIZE);
  writefile(file);
  file.close();  
  return 1;
}

// modify a program
byte ProgramData::modify(byte pid, ProgramStruct *buf) {
  if (pid >= nprogs)  return 0;
  NffsFile file;
  file.open(prog_fname, FS_ACCESS_READ);
  if (!file.exists()) return 0; 
  
  uint addr = sizeof(nprogs) + (uint)pid * PROGRAMSTRUCT_SIZE;
//  file.seek(addr);
//  file.write((byte*)buf, PROGRAMSTRUCT_SIZE);
  writebuf(addr,(byte*)buf, PROGRAMSTRUCT_SIZE);
//  file.write(buff, MAX_NUM_PROGRAMS*PROGRAMSTRUCT_SIZE);
  writefile(file);
  file.close();
  return 1;
}

// delete program
byte ProgramData::del(byte pid) {
  if (pid >= nprogs)  return 0;
  if (nprogs == 0) return 0;
  ProgramStruct copy;
  NffsFile file;
  file.open(prog_fname, FS_ACCESS_READ);
  if (!file.exists()) return 0; 
  file.read(buff, MAX_NUM_PROGRAMS*PROGRAMSTRUCT_SIZE);
  uint addr = sizeof(nprogs) + (uint)(pid+1) * PROGRAMSTRUCT_SIZE;
  // erase by copying backward
  for (; addr < sizeof(nprogs) + nprogs * PROGRAMSTRUCT_SIZE; addr += PROGRAMSTRUCT_SIZE) {
 //   file.seek(addr);
 //   file.readBytes((char*)&copy, PROGRAMSTRUCT_SIZE);
	  readbuf(addr,(byte*)&copy, PROGRAMSTRUCT_SIZE);
//	  file.seek(addr-PROGRAMSTRUCT_SIZE);
//    file.write((byte*)&copy, PROGRAMSTRUCT_SIZE);
	  writebuf(addr - PROGRAMSTRUCT_SIZE,(byte*)&copy, PROGRAMSTRUCT_SIZE);
  }
  nprogs --;
//  file.seek(0);
//  file.write((const byte*)&nprogs, sizeof(nprogs));
  writebuf(0,( byte*)&nprogs, sizeof(nprogs));
//  file.write(buff, MAX_NUM_PROGRAMS*PROGRAMSTRUCT_SIZE);
  writefile(file);
  file.close();  
  return 1;
}
#else 
/*
byte nvm_read_byte(void * pos) { ; }
void nvm_write_byte(void * pos, byte val) { ; }
void nvm_read_block(void * t, const void * addr, byte size) { ; }
void nvm_write_block(const void * t, void * addr, byte size) { ; }
byte eeprom_read_byte(void * pos) { ; }
void eeprom_write_byte(void * pos, byte val) { ; }
void eeprom_read_block(void * t, const void * addr, byte size) { ; }
void eeprom_write_block(const void * t, void * addr, byte size) { ; }
*/
#define ADDR_PROGRAMCOUNTER 400
#define N10 ADDR_PROGRAMCOUNTER
#define ADDR_PROGRAMDATA 401
#define MAX_NUMBER_PROGRAMS 5

void ProgramData::load_curr_task(TaskStruct *t) {
	if (!t)  return;
	if (curr_prog_index == MANUAL_PROGRAM_INDEX ||
		curr_prog_index == QUICK_PROGRAM_INDEX ||
		curr_prog_index == TESTZONE_PROGRAM_INDEX) {
		if (curr_task_index<0 || curr_task_index >= scheduled_ntasks) return;
		*t = manual_tasks[curr_task_index];
		return;
	}
	if (curr_prog_index<0 || curr_prog_index >= nprogs) return;
	if (curr_task_index<0 || curr_task_index >= scheduled_ntasks) return;

	unsigned int addr = sizeof(nprogs) + (unsigned int)curr_prog_index * PROGRAMSTRUCT_SIZE;
	addr += offsetof(struct ProgramStruct, tasks);
	addr += (unsigned int)curr_task_index * sizeof(TaskStruct);
	eeprom_read_block((void *)t, (void *)addr, sizeof(TaskStruct));
//	file.seek(addr);
//	file.readBytes((char*)t, sizeof(TaskStruct));
//	file.close();
}
void::ProgramData::init(){}

/** Load program count from NVM */
void ProgramData::load_count() {
	byte nprograms = eeprom_read_byte((byte *)N10);
	nprograms = nvm_read_byte((byte *)ADDR_PROGRAMCOUNTER);
}

/** Save program count to NVM */
void ProgramData::save_count(byte nprograms) {
	
	nvm_write_byte((byte *)ADDR_PROGRAMCOUNTER, nprograms);
}

/** Erase all program data */
void ProgramData::eraseall() {

	byte nprograms = 0;
	save_count(nprograms);
}

/** Read a program from NVM*/
void ProgramData::read(byte pid, ProgramStruct *buf,boolean head) {
	byte nprograms = eeprom_read_byte((byte *)N10);
	if (pid >= nprograms) return;
	if (0) {
		// todo: handle SD card
	} else {
		unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)pid * PROGRAMSTRUCT_SIZE;
		nvm_read_block((void*)buf, (const void *)addr, PROGRAMSTRUCT_SIZE);
	}
}

/** Add a program */
byte ProgramData::add(ProgramStruct *buf) {
	byte nprograms = eeprom_read_byte((byte *)N10);
	if (0) {
		// todo: handle SD card
	} else {
		if (nprograms >= MAX_NUMBER_PROGRAMS)  return 0;
		unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)nprograms * PROGRAMSTRUCT_SIZE;
		nvm_write_block((const void*)buf, (void *)addr, PROGRAMSTRUCT_SIZE);
		nprograms++;
		save_count(nprograms);
	}
	return 1;
}

/** Modify a program */
byte ProgramData::modify(byte pid, ProgramStruct *buf) {
	byte nprograms = eeprom_read_byte((byte *)N10);
	if (pid >= nprograms)  return 0;
	if (0) {
		// handle SD card
	} else {
		unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)pid * PROGRAMSTRUCT_SIZE;
		nvm_write_block((const void*)buf, (void *)addr, PROGRAMSTRUCT_SIZE);
	}
	return 1;
}

/** Delete program(s) */
byte ProgramData::del(byte pid) {
	byte nprograms = eeprom_read_byte((byte *)N10);
	if (pid >= nprograms)  return 0;
	if (nprograms == 0) return 0;
	if (0) {
		// handle SD card
	} else {
		ProgramStruct copy;
		unsigned int addr = ADDR_PROGRAMDATA + (unsigned int)(pid + 1) * PROGRAMSTRUCT_SIZE;
		// erase by copying backward
		for (; addr < ADDR_PROGRAMDATA + nprograms * PROGRAMSTRUCT_SIZE; addr += PROGRAMSTRUCT_SIZE) {
			nvm_read_block((void*)&copy, (const void *)addr, PROGRAMSTRUCT_SIZE);
			nvm_write_block((const void*)&copy, (void *)(addr - PROGRAMSTRUCT_SIZE), PROGRAMSTRUCT_SIZE);
		}
nprograms--;
		save_count(nprograms);
	}
	return 1;
}
#endif
// Check if a given time matches the program's start day
byte ProgramStruct::check_day_match(time_t t) {

  byte weekday_t = weekday(t);        // weekday ranges from [0,6] within Sunday being 1
  byte day_t = day(t);
  byte month_t = month(t);
  byte wd = (weekday_t+5)%7;
  byte dt = day_t;

  // check day match
  switch(daytype) {
    case DAY_TYPE_WEEKLY:
      // weekday match
      if (!(days[0] & (1<<wd)))
        return 0;
    break;

    case DAY_TYPE_INTERVAL:
      // this is an inverval program
      if (((t/SECS_PER_DAY)%days[1]) != days[0])  return 0;
    break;
  }

  // check odd/even day restriction
  if (!restr) { }
  else if (restr == 2) {
    // even day restriction
    if((dt%2)!=0)  return 0;
  } else if (restr == 1) {
    // odd day restriction
    // skip 31st and Feb 29
    if(dt==31)  return 0;
    else if (dt==29 && month_t==2)  return 0;
    else if ((dt%2)!=1)  return 0;
  }
  return 1;
}

// Check if a given time matches program's start time
// this also checks for programs that started the previous
// day and ran over night
byte ProgramStruct::check_match(time_t t) {

  // check program enable status
  if (!enabled) return 0;

  int16_t start = starttimes[0];
  int16_t repeat = starttimes[1];
  int16_t interval = starttimes[2];
  unsigned long current_minute = (t%86400L)/60;

  // first assume program starts today
  if (check_day_match(t)) {
    // t matches the program's start day
	 // DEBUG_PRINT(sttype); DEBUG_PRINT("day match"); DEBUG_PRINTLN(start);
    switch(sttype) {
      case STARTTIME_TYPE_NONE:
      {
        return (current_minute== start) ? 1 : 0;  
      }
      case STARTTIME_TYPE_FIXED:
      {
        // fixed start time type
        for(byte i=0;i<MAX_NUM_STARTTIMES;i++) {
          if (current_minute == starttimes[i])  return 1; // if curren_minute matches any of the given start time, return 1
        }
        return 0; // otherwise return 0
      }
      case STARTTIME_TYPE_REPEAT:
      {
        // repeating type
        // if current_minute matches start time, return 1
        if (current_minute == start) return 1;

        // otherwise, current_minute must be larger than start time, and interval must be non-zero
        if (current_minute > start && interval) {
          // check if we are on any interval match
          int16_t c = (current_minute - start) / interval;
          if ((c * interval == (current_minute - start)) && c <= repeat) {
            return 1;
          }
        }
        break;
      }
    }
  }
  // to proceed, program has to be repeating type, and interval and repeat must be non-zero
  if (sttype!=STARTTIME_TYPE_REPEAT || !interval)  return 0;

  // next, assume program started the previous day and ran over night
  if (check_day_match(t-86400L)) {
    // t-86400L matches the program's start day
    int16_t c = (current_minute - start + 1440) / interval;
    if ((c * interval == (current_minute - start + 1440)) && c <= repeat) {
      return 1;
    }
  }
  return 0;
}
#endif