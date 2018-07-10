#include "define.h"
#ifndef OS217
#include "OSBeeWiFi.h"
#include "program.h"
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
byte OS_Prog(ProgramStruct prog, char *p) {
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