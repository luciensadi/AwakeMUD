/* I got _reeeeal_ tired of writing these things out by hand, so now there are macros for them. */

#ifndef _strn_bullshit_h_
#define _strn_bullshit_h_

// ALWAYS MAKE SURE BUFFER IS NOT A POINTER. ex: myfunc(char *buf) {STRCAT(buf, thing);} will fail.
// Oh, you got this warning?
//   act.comm.cpp:693:12: warning: 'STRCPY' call operates on objects of type 'char' while the size is based on a different type 'char *' [-Wsizeof-pointer-memaccess]
// THEN YOU DIDN'T PAY ATTENTION TO THE ABOVE, DID YOU.

// strcat -> strncat. 
#define STRCAT(buffer, message) strncat((buffer), (message), sizeof((buffer)) - strlen((buffer)) - 1)

// strcpy -> strncpy.
#define STRCPY(buffer, message) strncpy((buffer), (message), sizeof(buffer))

// sprintf -> snprintf. DON'T YOU FUCKING DARE USE THIS WITH ENDOF.
#define SPRINTF(buffer, message, ...) snprintf((buffer), sizeof((buffer)), (message), ##__VA_ARGS__)

// sprintf(ENDOF()) -> snprintf(ENDOF())
// invoke as SPRINTF_ENDOF(buf, message, args) -- it'll handle the endof crap
#define SPRINTF_ENDOF(buffer, message, ...) snprintf(ENDOF((buffer)), sizeof((buffer)) - strlen((buffer)), (message), ##__VA_ARGS__)

#endif
