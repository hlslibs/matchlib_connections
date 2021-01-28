/**************************************************************************
 *                                                                        *
 *  HLS Connections Library                                               *
 *                                                                        *
 *  Software Version: 1.2                                                 *
 *                                                                        *
 *  Release Date    : Thu Jan 28 15:19:09 PST 2021                        *
 *  Release Type    : Production Release                                  *
 *  Release Build   : 1.2.3                                               *
 *                                                                        *
 *  Copyright , Mentor Graphics Corporation,                     *
 *                                                                        *
 *  All Rights Reserved.                                                  *
 *  
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
#include <connections/connections_utils.h>
#include <connections/connections.h>
#include <connections/connections_trace.h>
#include <connections/connections_sync.h>
#include <connections/port_scanner.h>

#endif
