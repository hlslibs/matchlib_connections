/**************************************************************************
 *                                                                        *
 *  HLS Connections Library                                               *
 *                                                                        *
 *  Software Version: 2025.4                                              *
 *                                                                        *
 *  Release Date    : Tue Nov 11 18:46:00 PST 2025                        *
 *  Release Type    : Production Release                                  *
 *  Release Build   : 2025.4.0                                            *
 *                                                                        *
 *  Copyright 2020 Siemens                                                *
 *                                                                        *
 **************************************************************************
 *  Licensed under the Apache License, Version 2.0 (the "License");       *
 *  you may not use this file except in compliance with the License.      * 
 *  You may obtain a copy of the License at                               *
 *                                                                        *
 *      http://www.apache.org/licenses/LICENSE-2.0                        *
 *                                                                        *
 *  Unless required by applicable law or agreed to in writing, software   * 
 *  distributed under the License is distributed on an "AS IS" BASIS,     * 
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or       *
 *  implied.                                                              * 
 *  See the License for the specific language governing permissions and   * 
 *  limitations under the License.                                        *
 **************************************************************************
 *                                                                        *
 *  The most recent version of this package is available at github.       *
 *                                                                        *
 *************************************************************************/

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
