/**************************************************************************
 *                                                                        *
 *  HLS Connections Library                                               *
 *                                                                        *
 *  Software Version: 1.2                                                 *
 *                                                                        *
 *  Release Date    : Tue Feb  1 15:18:00 PST 2022                        *
 *  Release Type    : Production Release                                  *
 *  Release Build   : 1.2.8                                               *
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
// Localize sc_trace and logging utilities for Connections classes
//
//
// Revision History:
//  1.2.4    - CAT-26848: Add waveform tracing for Matchlib SyncChannel
//  1.2.0    - Refactored tracing from mc_connections.h
//
//*****************************************************************************************

#ifndef CONNECTIONS_TRACE_H
#define CONNECTIONS_TRACE_H
#include <systemc>

/**
 * Example of how to enable tracing for user defined structs:

struct MyType
{
    int info {0};
    bool flag {false};

    inline friend void sc_trace(sc_trace_file *tf, const MyType & v, const std::string& NAME ) {
      sc_trace(tf,v.info, NAME + ".info");
      sc_trace(tf,v.flag, NAME + ".flag");
    }
};

**/

#ifdef CONNECTIONS_SIM_ONLY
namespace Connections 
{
  // Used to mark and select Connections Sync and Combinational channels for tracing
  class sc_trace_marker
  {
  public:
    virtual void set_trace(sc_trace_file *trace_file_ptr) = 0;
    virtual bool set_log(std::ofstream *os, int &log_num, std::string &path_name) = 0;
  };
}
#endif

// Function: trace_hierarchy(sc_object* obj, sc_trace_file* file_ptr)
//  Trace all Connections signal types in the hierarchy
//
// Example usage in sc_main()
//
//  Top top("top");
//  sc_trace_file *trace_file_ptr = sc_trace_statice::setup_trace_file("trace");
//  trace_hierarchy(&top, trace_file_ptr);
//
static inline void trace_hierarchy( sc_object *obj, sc_trace_file *file_ptr )
{
#ifdef CONNECTIONS_SIM_ONLY
  if ( Connections::sc_trace_marker *p = dynamic_cast<Connections::sc_trace_marker *>(obj) ) {
    p->set_trace(file_ptr);
  }
  std::vector<sc_object *> children = obj->get_child_objects();
  for ( unsigned i = 0; i < children.size(); i++ ) {
    if ( children[i] ) {
      trace_hierarchy(children[i], file_ptr);
    }
  }
#endif
}

// Logging Connections channel data under a level of hierarchy.
// Object names are by default in channel_logs_names.txt
// Object values are by default in channel_logs_data.txt
// Change the file names by supplying the base name to the enable() method.
// Use the enable() method "unbuffered" arg to help debug when simulations hang or terminate abnormally.
//
// Example usage in sc_main()
//
//  Top top("top", test_num);
//
//  channel_logs logs;
//  logs.enable("log", true);
//  logs.log_hierarchy(top);
//

class channel_logs
{
public:
  bool enabled{false};
  std::string log_dir;
  int log_num{0};
  std::ofstream log_stream;
  std::ofstream log_names;

  channel_logs() {}

  int enable( std::string fname_base = "", bool unbuffered = false ) {
    if ( fname_base.empty() ) {
      fname_base = "channel_logs";
    }
    std::ostringstream nm_stream, nm_names;
    nm_stream << fname_base << "_data.txt";
    if ( unbuffered ) {
      log_stream.rdbuf()->pubsetbuf(0, 0);
    }
    log_stream.open(nm_stream.str());
    if ( !log_stream.is_open() ) {
      std::cerr << "Cannot open file '" << nm_stream.str() << "'" << std::endl;
      return 1;
    }
    nm_names << fname_base << "_names.txt";
    if ( unbuffered ) {
      log_names.rdbuf()->pubsetbuf(0, 0);
    }
    log_names.open(nm_names.str());
    if ( !log_names.is_open() ) {
      std::cerr << "Cannot open file '" << nm_names.str() << "'" << std::endl;
      return 1;
    }
    return 0;
  }

  void log_hier_helper( sc_object *obj ) {
#ifdef CONNECTIONS_SIM_ONLY
    if ( Connections::sc_trace_marker *p = dynamic_cast<Connections::sc_trace_marker *>(obj) ) {
      std::string path_name;
      if ( log_stream.is_open() && log_names.is_open() && p->set_log(&log_stream, log_num, path_name) ) {
        log_names << log_num << " " << path_name << "\n";
      }
    }
    std::vector<sc_object *> children = obj->get_child_objects();
    for ( unsigned i = 0; i < children.size(); i++ ) {
      if ( children[i] ) {
        log_hier_helper(children[i]);
      }
    }
#endif
  }

  void log_hierarchy( sc_object &sc_obj ) {
    log_hier_helper(&sc_obj);
  }

  ~channel_logs() {}
};

#endif // CONNECTIONS_TRACE_H
