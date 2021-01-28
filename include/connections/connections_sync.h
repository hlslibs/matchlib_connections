/**************************************************************************
 *                                                                        *
 *  HLS Connections Library                                               *
 *                                                                        *
 *  Software Version: 1.2                                                 *
 *                                                                        *
 *  Release Date    : Thu Jan 28 15:10:47 PST 2021                        *
 *  Release Type    : Production Release                                  *
 *  Release Build   : 1.2.2                                               *
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
//
// Define SyncIn, SyncOut, and SyncChannel classes.
// Based on reimplementation of p2p classes
// SYN abstraction_t used exclusively here
//
//
// Revision History:
//  1.2.1    - Corrected p2p_checker to be sync_checker
//  1.2.0    - Refactored "sync" connections from mc_connections.h
//
//*****************************************************************************************

#ifndef CONNECTIONS_SYNC_H
#define CONNECTIONS_SYNC_H

#if __cplusplus < 201103L
#error "The standard C++11 (201103L) or newer is required"
#endif

#include <systemc.h>

namespace Connections
{
  // Helper Classes for checking that reset was called and all ports are bound
  class sync_checker
  {
    mutable bool is_ok;
#ifndef __SYNTHESIS__
    const char *objname;
    std::stringstream error_string;
#endif

  public:
    sync_checker (const char *name, const char *func_name, const char *operation) :
      is_ok(false) {
#ifndef __SYNTHESIS__
      objname = name;
      error_string << "You must " << func_name << " before you can " << operation << ".";
#endif
    }

    inline void ok () {
      is_ok = true;
    }

    inline void test () const {
#ifndef __SYNTHESIS__
      if ( !is_ok ) {
        SC_REPORT_ERROR(objname, error_string.str().c_str());
        is_ok = true; // Only report error message one time
      }
#endif
    }
  };

  class conn_sync
  {
  public:
    class chan
    {
      sync_checker rd_chk, wr_chk;

    public:
      sc_signal <bool> vld ;
      sc_signal <bool> rdy ;
      static const unsigned int width=0;

      chan(sc_module_name name = sc_gen_unique_name("p2p_sync_chan"))
        : rd_chk(name, "call reset_sync_out()", "synchronize from this channel")
        , wr_chk(name, "call reset_sync_in()", "synchronize to this channel")
        , vld( ccs_concat(name,"vld") )
        , rdy( ccs_concat(name,"rdy") )
      {}

      void reset_sync_out() {
        vld.write(false);
        wr_chk.ok();
      }
      void reset_sync_in() {
        rdy.write(false);
        rd_chk.ok();
      }

#pragma design modulario<sync>
      bool nb_sync_in() {
        rd_chk.test();
        rdy.write(true);
        wait();
        rdy.write(false);
        return vld.read();
      }

#pragma design modulario<sync>
      void sync_in() {
        rd_chk.test();
        do {
          rdy.write(true) ;
          wait() ;
        } while (vld.read() != true );
        rdy.write(false);
      }

#pragma design modulario<sync>
      void sync_out() {
        wr_chk.test();
        do {
          vld.write(true);
          wait();
        } while (rdy.read() != true );
        vld.write(false);
      }
    };   // chan

    // SYN input port definition
    class in
    {
      sync_checker rd_chk;

    public:
      sc_in <bool> vld ;
      sc_out <bool> rdy ;
      static const unsigned int width=0;

      in(sc_module_name name = sc_gen_unique_name("p2p_sync_in"))
        : rd_chk(name, "call reset_sync_in()", "synchronize from this port")
        , vld( ccs_concat(name,"vld") )
        , rdy( ccs_concat(name,"rdy") )
      {}

      void reset_sync_in() {
        rdy.write(false);
        rd_chk.ok();
      }

#pragma design modulario<sync>
      bool nb_sync_in() {
        rd_chk.test();
        rdy.write(true);
        wait();
        rdy.write(false);
        return vld.read();
      }

#pragma design modulario<sync>
      void sync_in() {
        rd_chk.test();
        do {
          rdy.write(true) ;
          wait() ;
        } while (vld.read() != true );
        rdy.write(false);
      }

      template <class C>
      void bind (C &c) {
        vld(c.vld);
        rdy(c.rdy);
      }

      template <class C>
      void operator() (C &c) {
        bind(c);
      }
    };  // in

    class out
    {
      sync_checker wr_chk;

    public:
      sc_out <bool> vld ;
      sc_in <bool> rdy ;
      static const unsigned int width=0;

      out(sc_module_name name = sc_gen_unique_name("p2p_sync_out"))
        : wr_chk(name, "call reset_sync_out()", "synchronize to this port")
        , vld( ccs_concat(name,"vld") )
        , rdy( ccs_concat(name,"rdy") )
      {}

      void reset_sync_out() {
        vld.write(false);
        wr_chk.ok();
      }

#pragma design modulario<sync>
      void sync_out() {
        wr_chk.test();
        do {
          vld.write(true);
          wait();
        } while (rdy.read() != true );
        vld.write(false);
      }

      template <class C>
      void bind (C &c) {
        vld(c.vld);
        rdy(c.rdy);
      }

      template <typename C>
      void operator() (C &c) {
        bind(c);
      }
    };  // out

  };  // conn_sync

  class SyncOut : public conn_sync::out
  {
  private:
    typedef conn_sync::out Base;
#ifdef CONNECTIONS_SIM_ONLY
    std::string _name;
#endif

  public:
    SyncOut(sc_module_name name = sc_gen_unique_name("Connections::SyncOut"))
      : Base(name)  {
#ifdef CONNECTIONS_SIM_ONLY
      _name = name;
#endif
    }
    ~SyncOut() {}

    void SyncPush() { Base::sync_out(); }
    void sync_out() { Base::sync_out(); }
    void Reset() { Base::reset_sync_out(); }
    void reset_sync_out() { Base::reset_sync_out(); }

#ifdef CONNECTIONS_SIM_ONLY
    inline friend void sc_trace(sc_trace_file *tf, const SyncOut &v, const std::string &NAME ) {
      sc_trace(tf,v.rdy, NAME + ".rdy");
      sc_trace(tf,v.vld, NAME + ".vld");
    }
    std::string name() {
      std::string theName;
      if (sc_core::sc_get_current_process_b() && sc_core::sc_get_current_process_b()->get_parent_object()) {
        theName = std::string(sc_core::sc_get_current_process_b()->get_parent_object()->name()) + "." + _name;
      } else {
        theName = _name;
      }
      return (theName);
    }
#endif
  };

  class SyncIn : public conn_sync::in
  {
  private:
    typedef conn_sync::in Base;
#ifdef CONNECTIONS_SIM_ONLY
    std::string _name;
#endif

  public:
    SyncIn(sc_module_name name = sc_gen_unique_name("Connections::SyncIn"))
      : Base(name) {
#ifdef CONNECTIONS_SIM_ONLY
      _name = name;
#endif
    }
    ~SyncIn() {}
    void SyncPop() { Base::sync_in(); }
    void sync_in() { Base::sync_in(); }
    bool SyncPopNB() { return (Base::nb_sync_in());}
    bool nb_sync_in() { return (Base::nb_sync_in()); }
    void Reset() { Base::reset_sync_in(); }
    void reset_sync_in() { Base::reset_sync_in(); }

#ifdef CONNECTIONS_SIM_ONLY
    inline friend void sc_trace(sc_trace_file *tf, const SyncIn &v, const std::string &NAME ) {
      sc_trace(tf,v.rdy, NAME + ".rdy");
      sc_trace(tf,v.vld, NAME + ".vld");
    }
    std::string name() {
      std::string theName;
      if (sc_core::sc_get_current_process_b() && sc_core::sc_get_current_process_b()->get_parent_object()) {
        theName = std::string(sc_core::sc_get_current_process_b()->get_parent_object()->name()) + "." + _name;
      } else {
        theName = _name;
      }
      return (theName);
    }
#endif
  };

  class SyncChannel: public conn_sync::chan
  {
  private:
    typedef conn_sync::chan  Base;
#ifdef CONNECTIONS_SIM_ONLY
    std::string _name;
#endif
  public:
    SyncChannel(sc_module_name name = sc_gen_unique_name("Connections::SyncIn"))
      : Base(name)  {
#ifdef CONNECTIONS_SIM_ONLY
      _name = name;
#endif
    }

    void SyncPop() { Base::sync_in(); }
    void sync_in() { Base::sync_in(); }
    bool SyncPopNB() { return (Base::nb_sync_in()); }
    bool nb_sync_in() { return (Base::nb_sync_in()); }
    void SyncPush() { Base::sync_out(); }
    void sync_out() { Base::sync_out(); }

    void ResetRead() { Base::reset_sync_in(); }
    void reset_sync_in() { Base::reset_sync_in(); }
    void ResetWrite() { Base::reset_sync_out(); }
    void reset_sync_out() { Base::reset_sync_out(); }

#ifdef CONNECTIONS_SIM_ONLY
    inline friend void sc_trace(sc_trace_file *tf, const SyncChannel &v, const std::string &NAME ) {
      sc_trace(tf,v.rdy, NAME + ".rdy");
      sc_trace(tf,v.vld, NAME + ".vld");
    }
    std::string name() {
      std::string theName;
      if (sc_core::sc_get_current_process_b() && sc_core::sc_get_current_process_b()->get_parent_object()) {
        theName = std::string(sc_core::sc_get_current_process_b()->get_parent_object()->name()) + "." + _name;
      } else {
        theName = _name;
      }
      return (theName);
    }
#endif
  };

} //Connections namespace


#endif // CONNECTIONS_SYNC_H
