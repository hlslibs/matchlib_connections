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
//
// Define SyncIn, SyncOut, and SyncChannel classes.
// Based on reimplementation of p2p classes
// SYN abstraction_t used exclusively here
//
//
// Revision History:
//  1.2.4    - CAT-26848: Add waveform tracing for Matchlib SyncChannel
//  1.2.1    - Corrected conn_checker to be sync_checker
//  1.2.0    - Refactored "sync" connections from mc_connections.h
//
//*****************************************************************************************

#ifndef CONNECTIONS_SYNC_H
#define CONNECTIONS_SYNC_H

#if __cplusplus < 201103L
#error "The standard C++11 (201103L) or newer is required"
#endif

#include <systemc.h>
#include "connections_utils.h"
#include "connections_trace.h"

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

  // Dataless rdy/vld sync handshake
  class conn_sync
  {
  public:
    #pragma hls_ungroup
    class chan : public sc_module
    {
      sync_checker rd_chk, wr_chk;

    public:
      sc_signal <bool> vld;
      sc_signal <bool> rdy;
      static const unsigned int width=0;

      chan(sc_module_name name = sc_gen_unique_name("conn_sync_chan"))
        : sc_module(name)
        , rd_chk(name, "call reset_sync_out()", "synchronize from this channel")
        , wr_chk(name, "call reset_sync_in()", "synchronize to this channel")
        , vld( CONNECTIONS_CONCAT(name,"vld") )
        , rdy( CONNECTIONS_CONCAT(name,"rdy") )
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
          rdy.write(true);
          wait();
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
      sc_in <bool> vld;
      sc_out <bool> rdy;
      static const unsigned int width=0;

      in(sc_module_name name = sc_gen_unique_name("conn_sync_in"))
        : rd_chk(name, "call reset_sync_in()", "synchronize from this port")
        , vld( CONNECTIONS_CONCAT(name,"vld") )
        , rdy( CONNECTIONS_CONCAT(name,"rdy") )
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
          rdy.write(true);
          wait();
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
      sc_out <bool> vld;
      sc_in <bool> rdy;
      static const unsigned int width=0;

      out(sc_module_name name = sc_gen_unique_name("conn_sync_out"))
        : wr_chk(name, "call reset_sync_out()", "synchronize to this port")
        , vld( CONNECTIONS_CONCAT(name,"vld") )
        , rdy( CONNECTIONS_CONCAT(name,"rdy") )
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

  // Like conn_sync but just a vld signal
  class conn_event
  {
  public:
    #pragma hls_ungroup
    class chan : public sc_module
    {
      sync_checker wr_chk;
    public:
      sc_signal<bool> vld;

      chan(sc_module_name name = sc_gen_unique_name("conn_event_chan"))
        : wr_chk(name, "call reset_notify()", "send an event notification on this channel" )
        , vld( ccs_concat(name,"vld") )
        {}

      void reset_notify() {
        vld.write(false);
        wr_chk.ok();
      }
      void reset_wait_for() {}

      #pragma design modulario<sync>
      void wait_for() {
        do {
          wait();
        } while (vld.read() != true );
      }

      #pragma design modulario<sync>
      bool nb_valid() {
        wait();
        return vld.read();
      }

      #pragma design modulario<sync>
      void nb_notify () {
        wr_chk.test();
        vld.write(true);
        wait();
        vld.write(false);
      }
    }; //chan

    class in
    {
    public:
      sc_in<bool> vld;

      in(sc_module_name name = sc_gen_unique_name("conn_event_in")) :
        vld( ccs_concat(name,"vld") ) {}

      void reset_wait_for() {}

      #pragma design modulario<sync>
      void wait_for() {
        do {
          wait();
        } while (vld.read() != true );
      }

      #pragma design modulario<sync>
      bool nb_valid() {
        wait();
        return vld.read();
      }

      template <class C>
      void bind (C &c) {
        vld(c.vld);
      }

      template <class C>
      void operator() (C &c) {
        bind(c);
      }
    }; //in

    class out
    {
      sync_checker wr_chk;

    public:
      sc_out<bool> vld;

      out(sc_module_name name = sc_gen_unique_name("conn_event_out"))
        : wr_chk(name, "call reset_notify()", "send an event notification on this port" )
        , vld( ccs_concat(name,"vld") )
        {}

      void reset_notify() {
        vld.write(false);
        wr_chk.ok();
      }

      #pragma design modulario<sync>
      void nb_notify () {
        wr_chk.test();
        vld.write(true);
        wait();
        vld.write(false);
      }

      template <class C>
      void bind (C &c) {
        vld(c.vld);
      }

      template <class C>
      void operator() (C &c) {
        bind(c);
      }
    }; //out
  }; //conn_event

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
#ifdef CONNECTIONS_SIM_ONLY
        , public Connections::sc_trace_marker
#endif
  {
  private:
    typedef conn_sync::chan  Base;
#ifdef CONNECTIONS_SIM_ONLY
    std::string _name;
#endif
  public:
    SyncChannel(sc_module_name name = sc_gen_unique_name("Connections::SyncChannel"))
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

    virtual void set_trace(sc_trace_file* fp)
    {
      sc_trace(fp, this->rdy, this->rdy.name());
      sc_trace(fp, this->vld, this->vld.name());
    }

    virtual bool set_log(std::ofstream* os, int& log_num, std::string& path_name) { return false; }
#endif
  };

  class EventOut : public conn_event::out
  {
  private:
    typedef conn_event::out Base;
#ifdef CONNECTIONS_SIM_ONLY
    std::string _name;
#endif

  public:
    EventOut(sc_module_name name = sc_gen_unique_name("Connections::EventOut"))
      : Base(name)  {
#ifdef CONNECTIONS_SIM_ONLY
      _name = name;
#endif
    }
    ~EventOut() {}

    void EventPushNB() { Base::nb_notify(); }
    void nb_notify() { Base::nb_notify(); }
    void Reset() { Base::reset_notify(); }
    void reset_notify() { Base::reset_notify(); }

#ifdef CONNECTIONS_SIM_ONLY
    inline friend void sc_trace(sc_trace_file *tf, const EventOut &v, const std::string &NAME ) {
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

  class EventIn : public conn_event::in
  {
  private:
    typedef conn_event::in Base;
#ifdef CONNECTIONS_SIM_ONLY
    std::string _name;
#endif

  public:
    EventIn(sc_module_name name = sc_gen_unique_name("Connections::EventIn"))
      : Base(name) {
#ifdef CONNECTIONS_SIM_ONLY
      _name = name;
#endif
    }
    ~EventIn() {}
    void EventPop() { Base::wait_for(); }
    void wait_for() { Base::wait_for(); }
    bool EventPopNB() { return (Base::nb_valid()); }
    bool nb_valid() { return (Base::nb_valid()); }
    void Reset() { Base::reset_wait_for(); }
    void reset_wait_for() { Base::reset_wait_for(); }

#ifdef CONNECTIONS_SIM_ONLY
    inline friend void sc_trace(sc_trace_file *tf, const EventIn &v, const std::string &NAME ) {
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

  class EventChannel: public conn_event::chan
#ifdef CONNECTIONS_SIM_ONLY
        , public Connections::sc_trace_marker
#endif
  {
  private:
    typedef conn_event::chan Base;
#ifdef CONNECTIONS_SIM_ONLY
    std::string _name;
#endif
  public:
    EventChannel(sc_module_name name = sc_gen_unique_name("Connections::EventChannel"))
      : Base(name)  {
#ifdef CONNECTIONS_SIM_ONLY
      _name = name;
#endif
    }

    void EventPop() { Base::wait_for(); }
    void wait_for() { Base::wait_for(); }
    bool EventPopNB() { return Base::nb_valid(); }
    bool nb_valid() { return Base::nb_valid(); }
    void EventPushNB() { Base::nb_notify(); }
    void nb_notify() { Base::nb_notify(); }

    void ResetRead() { Base::reset_wait_for(); }
    void reset_wait_for() { Base::reset_wait_for(); }
    void ResetWrite() { Base::reset_notify(); }
    void reset_notify() { Base::reset_notify(); }

#ifdef CONNECTIONS_SIM_ONLY
    inline friend void sc_trace(sc_trace_file *tf, const EventChannel &v, const std::string &NAME ) {
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

    virtual void set_trace(sc_trace_file* fp)
    {
      sc_trace(fp, this->vld, this->vld.name());
    }

    virtual bool set_log(std::ofstream* os, int& log_num, std::string& path_name) { return false; }
#endif
  };

} //Connections namespace

#endif // CONNECTIONS_SYNC_H
