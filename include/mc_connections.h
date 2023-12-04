// INSERT_EULA_COPYRIGHT: 2020

//*****************************************************************************************
// File: mc_connections.h
//
// Description: Single-file include to bring in the Connections library for synthesis
//              with Catapult HLS
//
// Revision History:
//    1.2.0 -
//*****************************************************************************************

#ifndef MC_CONNECTIONS_H
#define MC_CONNECTIONS_H

#define HLS_CATAPULT 1

#if __cplusplus < 201103L
#error "The standard C++11 (201103L) or newer is required"
#endif

#ifndef __SYNTHESIS__
#if !defined(IEEE_1666_SYSTEMC) && !defined(SC_INCLUDE_DYNAMIC_PROCESSES)
#define SC_INCLUDE_DYNAMIC_PROCESSES
#endif
#endif

#include <systemc.h>
#include <ac_sysc_macros.h>
#include <ac_sysc_trace.h>
#include <connections/connections.h>
#include <connections/connections_sync.h>
#include <connections/connections_fifo.h>
#include <connections/port_scanner.h>

#endif
