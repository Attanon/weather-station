//
// Created by attanon on 28.6.22.
//

#ifndef LOGGING_H
#define LOGGING_H

#endif //LOGGING_H

#include <HardwareSerial.h>

#define DEBUG
//#define DEBUG_VERBOSE

#ifdef DEBUG
#define DEBUG_PROGRAM(...) Serial.print( __VA_ARGS__ )
#define DEBUG_PROGRAM_LN(...) Serial.println( __VA_ARGS__ )
#define DEBUG_PROGRAM_F(...) Serial.printf( __VA_ARGS__ )
#endif

#ifdef DEBUG_VERBOSE
#define DEBUG_PROGRAM_VERBOSE(...) Serial.print( __VA_ARGS__ )
#define DEBUG_PROGRAM_VERBOSE_F(...) Serial.printf( __VA_ARGS__ )
#define DEBUG_PROGRAM_VERBOSE_LN(...) Serial.println( __VA_ARGS__ )
#endif

#ifndef DEBUG_PROGRAM
#define DEBUG_PROGRAM(...)
#endif

#ifndef DEBUG_PROGRAM_LN
#define DEBUG_PROGRAM_LN(...)
#endif

#ifndef DEBUG_PROGRAM_F
#define DEBUG_PROGRAM_F(...)
#endif

#ifndef DEBUG_PROGRAM_VERBOSE_LN
#define DEBUG_PROGRAM_VERBOSE_LN(...)
#endif

#ifndef DEBUG_PROGRAM_VERBOSE
#define DEBUG_PROGRAM_VERBOSE(...)
#endif

#ifndef DEBUG_PROGRAM_VERBOSE_F
#define DEBUG_PROGRAM_VERBOSE_F(...)
#endif