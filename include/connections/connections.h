


/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//*****************************************************************************************
// connections.h
//
// Revision History:
//   2.2.3   - CAT-39436 - Add signal debug for custom types in simulation
//   2.2.2   - CAT-38997 - Add simulation support for In/Out/Combinational::PeekNB()
//           - CAT-39113 - Enforce Reset() is called for simulation
//   2.2.1   - CAT-38188 - Updates to support SystemC 3.0 and fixes
//   2.2.0   - CAT-34924 - use DIRECT_PORT by default for pre-HLS simulation
//             CAT-37259 - Add macro guard around include of sc_reset.h
//   2.1.1   - CAT-31705 - free dynamically allocated memory
//             CAT-35251 - applied missing '#pragma builtin modulario' to DIRECT_PORT methods
//             CAT-34936 - support for trace/log for DIRECT_PORT
//   2.1.0   - CAT-34971 - clean up compiler warnings about uninitialized values
//   1.3.0   - CAT-31235 - fix waveform tracing bug in pre-HLS matchlib model
//   1.2.6   - CAT-29206 - fix for waveform tracing
//             CAT-29244 - improved error checking
//   1.2.4   - Default to CONNECTIONS_ACCURATE_SIM. Add CONNECTIONS_SYN_SIM instead of
//             relying on an implicit mode with no sim setting.
//           - CAT-26848: Add waveform tracing for Matchlib SyncChannel
//   1.2.0   - Added support for multiple clocks
//
//*****************************************************************************************

#ifndef __CONNECTIONS__CONNECTIONS_H__
#define __CONNECTIONS__CONNECTIONS_H__

#if !defined(__SYNTHESIS__)
// Simulation modes:
// CONNECTIONS_ACCURATE_SIM Cycle-accurate view of Connections port and channel code. (default)
// CONNECTIONS_FAST_SIM     Faster TLM view of Connections port and channel code.
// CONNECTIONS_SYN_SIM      Synthesis view of Connections port and combinational code. Debug only mode.

#if !(defined(CONNECTIONS_ACCURATE_SIM) || defined(CONNECTIONS_FAST_SIM) || defined(CONNECTIONS_SYN_SIM))
#define CONNECTIONS_ACCURATE_SIM
#endif

#if defined(CONNECTIONS_ACCURATE_SIM) && defined(CONNECTIONS_FAST_SIM)
#error "Both CONNECTIONS_ACCURATE_SIM and CONNECTIONS_FAST_SIM are defined. Define one or the other."
#endif

#if defined(CONN_RAND_STALL) && !(defined(CONNECTIONS_ACCURATE_SIM) || defined(CONNECTIONS_FAST_SIM))
#warning "Warning: CONN_RAND_STALL only works in CONNECTIONS_ACCURATE_SIM and CONNECTIONS_FAST_SIM modes!"
#endif

#if defined(CONNECTIONS_ACCURATE_SIM) || defined(CONNECTIONS_FAST_SIM)
#define CONNECTIONS_SIM_ONLY
#if defined(CONNECTIONS_SYN_SIM)
#warning "CONNECTIONS_SYN_SIM cannot be used with CONNECTIONS_ACCURATE_SIM or CONNECTIONS_FAST_SIM"
#undef CONNECTIONS_SYN_SIM
#endif
#endif

#if defined(CONNECTIONS_SYN_SIM)
#warning "Caution: Synthesis simulation mode is not cycle accurate with multiple wait() statements across IO"
#endif
#endif //__SYNTHESIS__

#ifdef CONNECTIONS_ASSERT_ON_QUERY
#define QUERY_CALL() \
  error "Empty/Peek/Full functions are currently not supported in HLS"
#else
#define QUERY_CALL()
#endif

#include <systemc.h>
#if defined(CONNECTIONS_SIM_ONLY) && !defined(SC_INCLUDE_DYNAMIC_PROCESSES)
#if !defined(NC_SYSTEMC) && !defined(XM_SYSTEMC)
#warning "SC_INCLUDE_DYNAMIC_PROCESSES is being defined and reentrant <systemc> header included"
#define SC_INCLUDE_DYNAMIC_PROCESSES
#include <systemc>
#else
#error "Make sure SC_INCLUDE_DYNAMIC_PROCESSES is defined BEFORE first systemc.h include"
#endif
#endif

#include "marshaller.h"
#include "connections_utils.h"
#include "connections_trace.h"
#include "message.h"

#ifdef CONNECTIONS_SIM_ONLY
#include <iomanip>
#include <vector>
#include <map>
#include <type_traits>
#include <tlm.h>
#if !defined(NC_SYSTEMC) && !defined(XM_SYSTEMC) && !defined(NO_SC_RESET_INCLUDE)
#include <sysc/kernel/sc_reset.h>
#define HAS_SC_RESET_API
#endif
#include "Pacer.h"
#endif

/**
 * \brief Connections is a latency-insensitive communications library for MatchLib.
 * \ingroup Connections
 *
 * \par Overview
 *
 *   MatchLib Connections library is designed to address several limitations of
 *   existing Catapult p2p library:
 *     -# p2p ports are channel specific: when the channel changes, ports and
 *     modules that include the ports also need to change;
 *     -# No support for network channels that packetize a message into and
 *     extract from a network;
 *     -# Lack of Peek() for input ports;
 *     -# Lack of a default constructor that calls sc_gen_unique_name();
 *     -# Cannot be used to implement latency insensitive channels;
 *
 *   MatchLib Connections have separate definitions of Ports and Channels, both of
 *   which are based on the Modular IO methodology.
 *     - Ports supported methods:
 *       - In<T>: Peek(), Pop(), Empty(), Reset()
 *       - Out<T> : Push(), Full(), Reset()
 *     - Channel types:
 *       - Combinational<T>: Combinationally connects ports;
 *       - Bypass<T>: Enables DEQ when empty;
 *       - Pipeline<T>: Enables ENQ when full;
 *       - Buffer<T>: FIFO channel;
 *       - InNetwork<T>, OutNetwork<T>: Network channels;
 *
 *     - Ports blocking and non-blocking:
 *       - In<T>:
 *         - value = in.Pop(); //Blocking read
 *         - if (!in.Empty()) value = in.Pop(); //non-blocking read
 *       - Out<T>:
 *         - out.Push(value); //Blocking write
 *         - if (!out.Full()) out.Push(value); //non-blocking write
 *
 *     Channels:
 *       - Combinational: basically wires;
 *       - Pipeline: Added an internal state inside the channel so that the
 * communication is pipelined with a depth of one.
 *       - Bypass: Can buffer one msg when outgoing port is not available but
 *       incoming msg has arrived.
 *       - Buffer: Added a FIFO inside a channel.
 */

namespace Connections
{

// Analogous to Mentor Catapult's abstraction settings, but also used to loosely and tightly define bit ordering of ports and channels
// Note: ConManager can't be a preprocessor thing, since we run into trouble along interface boundary and VCS wrappers.
//  SYN_PORT - Marshalled and never includes ConManager.
//  MARSHALL_PORT - Marshalled and uses ConManager if not synthesis.
//  DIRECT_PORT - Marshaller disabled and no type conversion needed at interfaces, uses ConManager.
//  TLM_PORT - Like DIRECT_PORT, but interchange is through tlm_fifo. Event-based, does not use ConManager.
//
//  AUTO_PORT - TLM for normal SystemC and cosimulation (except binding to wrappers). SYN during HLS to present correct bit order.
  enum connections_port_t {SYN_PORT = 0, MARSHALL_PORT = 1, DIRECT_PORT = 2, TLM_PORT=3};


// Default mapping for AUTO_PORT
  /**
   * \brief Sets simulation port type
   * \ingroup Connections
   *
   * \par Set this to one of four port simulation types. From slowest (most accurate) to fastest (least accurate):
   *   - SYN_PORT: Actual SystemC modulario code given to catapult. Many wait() statements leads to
   *     timing inaccuracy in SystemC simulation.
   *   - MARSHALL_PORT: Like SYN_PORT, except when CONNECTIONS_SIM_ONLY is defined will use a cycle-based simulator
   *     to time the Push() and Pop() commands, which should approximate performance of real RTL when HLS'd with a
   *     pipeline init interval of 1. All input and output msg ports are "marshalled" into a sc_lv bitvectors.
   *   - DIRECT_PORT: Like MARSHALL_PORT, except without marshalling of msg ports into sc_lv bitvector, to save
   *     on simulation time.
   *   - TLM_PORT: rdy/val/msg ports do not exist, instead a shared tlm_fifo is used for each channel and simulation is
   *     event-based through an evaluate <-> update delta cycle loop. Best performance, but not cycle accurate for complex
   *     data dependencies and ports are not readibly viewable in a waveform viewer.
   *
   * All port types can bind to SYN_PORT, so that MARSHALL_PORT, DIRECT_PORT, and TLM_PORT can be used for the SystemC code during
   * RTL + SystemC co-simulation, while the co-simulation wrapper type maintains the RTL accurate SYN_PORT type.
   *
   * Default port types if no AUTO_PORT define is given are dependent on context. During HLS, SYN_PORT is always used (except
   * when overriden by FORCE_AUTO_PORT define). During SystemC simulation, TLM_PORT is used if CONNECTIONS_FAST_SIM is set, otherwise
   * DIRECT_PORT is used.
   *
   * \code
   *      #define AUTO_PORT MARSHALL_PORT
   *      #include <connections/connections.h>
   * \endcode
   *
   * \par
   *
   * The FORCE_AUTO_PORT define can also be used, overrides all other AUTO_PORT settings, including forcing to a different port type
   * during HLS. Useful if a specific port type is desired for targetted unit tests. Example code:
   *
   * \code
   *      #define FORCE_AUTO_PORT Connections::MARSHALL_PORT
   *      #include <connections/connections.h>
   * \endcode
   *
   * \par
   * In practice, neither of these are defined in the SystemC code, but added to CFLAGS or Catapult's Input/CompilerFlags options on
   * a per simulation or HLS run basis.
   *
   * \par
   *
   */
#ifndef AUTO_PORT

#if defined(__SYNTHESIS__)
#define AUTO_PORT Connections::SYN_PORT
#define AUTO_PORT_VAL 0
#elif !defined(CONNECTIONS_SIM_ONLY)
#define AUTO_PORT Connections::DIRECT_PORT
#define AUTO_PORT_VAL 2
#else

// Switch if we are in CONNECTIONS_SIM_ONLY
#if defined(CONNECTIONS_FAST_SIM)
#define AUTO_PORT Connections::TLM_PORT
#define AUTO_PORT_VAL 3
#else
#define AUTO_PORT Connections::DIRECT_PORT
#define AUTO_PORT_VAL 2
#endif

#endif // defined(__SYNTHESIS__)

#endif // ifndef AUTO_PORT


// If __SYNTHESIS__ always force SYN_PORT, even if
// manually defined in a Makefile or go_hls.tcl.
#if defined(__SYNTHESIS__)
#undef AUTO_PORT
#define AUTO_PORT Connections::SYN_PORT
#undef AUTO_PORT_VAL
#define AUTO_PORT_VAL 0
#endif // defined(__SYNTHESIS__)

  /**
   * \brief Forces simulation port type
   * \ingroup Connections
   *
   * \par See AUTO_PORT macro. This
   * \par
   *
   */

// FORCE_AUTO_PORT has precedence, even over __SYNTHESIS__
#if defined(FORCE_AUTO_PORT)
#undef AUTO_PORT
#define AUTO_PORT FORCE_AUTO_PORT
#undef AUTO_PORT_VAL
#define AUTO_PORT_VAL 2	  // Not always accurate, but OK..
#endif // defined(FORCE_AUTO_PORT)

#if defined(CONNECTIONS_SYN_SIM)
  static_assert(AUTO_PORT != TLM_PORT, "Connections::TLM_PORT not supported in Synthesis simulation mode");
#endif

#if AUTO_PORT_VAL == 1
#warning "Warning: Use of MARSHALL_PORT is deprecated. Use DIRECT_PORT instead."
#endif

#if AUTO_PORT_VAL == 0
#warning "Warning: Use of SYN_PORT is deprecated. Use DIRECT_PORT instead."
#endif

// Forward declarations
// These represent what SystemC calls "Ports" (which are basically endpoints)
  template <typename Message>
  class InBlocking_abs;
  template <typename Message>
  class OutBlocking_abs;
  template <typename Message, connections_port_t port_marshall_type = AUTO_PORT>
  class InBlocking;
  template <typename Message, connections_port_t port_marshall_type = AUTO_PORT>
  class OutBlocking;
  template <typename Message, connections_port_t port_marshall_type = AUTO_PORT>
  class In;
  template <typename Message, connections_port_t port_marshall_type = AUTO_PORT>
  class Out;
// These represent what SystemC calls "Channels" (which are connections)
  template <typename Message, connections_port_t port_marshall_type = AUTO_PORT>
  class Combinational;
  template <typename Message, connections_port_t port_marshall_type = AUTO_PORT>
  class Bypass;
  template <typename Message, connections_port_t port_marshall_type = AUTO_PORT>
  class Pipeline;
  template <typename Message, unsigned int NumEntries, connections_port_t port_marshall_type = AUTO_PORT>
  class Buffer;

  class Connections_BA_abs;
}  // namespace Connections

// Declarations used in the Marshaller
SpecialWrapperIfc(Connections::In);
SpecialWrapperIfc(Connections::Out);

#if defined(CONNECTIONS_NAMING_ORIGINAL)
#define _VLDNAME_ val
#define _RDYNAME_ rdy
#define _DATNAME_ msg
#define _VLDNAMESTR_ "val"
#define _RDYNAMESTR_ "rdy"
#define _DATNAMESTR_ "msg"

#define _VLDNAMEIN_ in_val
#define _RDYNAMEIN_ in_rdy
#define _DATNAMEIN_ in_msg
#define _VLDNAMEINSTR_ "in_val"
#define _RDYNAMEINSTR_ "in_rdy"
#define _DATNAMEINSTR_ "in_msg"

#define _VLDNAMEOUT_ out_val
#define _RDYNAMEOUT_ out_rdy
#define _DATNAMEOUT_ out_msg
#define _VLDNAMEOUTSTR_ "out_val"
#define _RDYNAMEOUTSTR_ "out_rdy"
#define _DATNAMEOUTSTR_ "out_msg"

#define _COMBVLDNAMESTR_ "comb_val"
#define _COMBRDYNAMESTR_ "comb_rdy"
#define _COMBDATNAMESTR_ "comb_msg"

#define _COMBVLDNAMEINSTR_ "comb_in_val"
#define _COMBRDYNAMEINSTR_ "comb_in_rdy"
#define _COMBDATNAMEINSTR_ "comb_in_msg"
#define _COMBVLDNAMEOUTSTR_ "comb_out_val"
#define _COMBRDYNAMEOUTSTR_ "comb_out_rdy"
#define _COMBDATNAMEOUTSTR_ "comb_out_msg"

#else
//  Default is catapult styl - rdy/vld/dat
#define _VLDNAME_ vld
#define _RDYNAME_ rdy
#define _DATNAME_ dat
#define _VLDNAMESTR_ "vld"
#define _RDYNAMESTR_ "rdy"
#define _DATNAMESTR_ "dat"

#define _VLDNAMEIN_ in_vld
#define _RDYNAMEIN_ in_rdy
#define _DATNAMEIN_ in_dat
#define _VLDNAMEINSTR_ "in_vld"
#define _RDYNAMEINSTR_ "in_rdy"
#define _DATNAMEINSTR_ "in_dat"

#define _VLDNAMEOUT_ out_vld
#define _RDYNAMEOUT_ out_rdy
#define _DATNAMEOUT_ out_dat
#define _VLDNAMEOUTSTR_ "out_vld"
#define _RDYNAMEOUTSTR_ "out_rdy"
#define _DATNAMEOUTSTR_ "out_dat"

#define _COMBVLDNAMESTR_ "comb_vld"
#define _COMBRDYNAMESTR_ "comb_rdy"
#define _COMBDATNAMESTR_ "comb_dat"

#define _COMBVLDNAMEINSTR_ "comb_in_vld"
#define _COMBRDYNAMEINSTR_ "comb_in_rdy"
#define _COMBDATNAMEINSTR_ "comb_in_dat"
#define _COMBVLDNAMEOUTSTR_ "comb_out_vld"
#define _COMBRDYNAMEOUTSTR_ "comb_out_rdy"
#define _COMBDATNAMEOUTSTR_ "comb_out_dat"
#endif

#if !defined(__SYNTHESIS__) && defined(CONNECTIONS_DEBUG)
#define DBG_CONNECT(x) std::cout << x << "\n";
#else
#define DBG_CONNECT(x);
#endif

namespace Connections
{

  // Enable debug for custom types in simulation
  // primary template
  template <typename T, typename = void>
  class dbg_signal : public sc_signal<T>
  {
  public:
    dbg_signal() {}
    dbg_signal(const char* s) : sc_signal<T>(s) {}
  };

#if defined(CONNECTIONS_CUSTOM_DEBUG) && !defined(__SYNTHESIS__)
  // Helper trait to detect .Marshall() method
  template <typename T, typename = void>
  struct has_Marshall_method : std::false_type {};

  template <typename T>
  struct has_Marshall_method<T,
    decltype(std::declval<T>().Marshall(std::declval<Marshaller<Wrapped<T>::width>&>()), void())>
    : std::true_type {};

  // specialization for types that have a marshall method (needs custom debug callback in vsim)
  template <typename T>
  class dbg_signal<T,  typename std::enable_if<has_Marshall_method<T>::value>::type>
    : public sc_signal<T>
  {
  public:
    dbg_signal() { do_reg(); }
    dbg_signal(const char* s) : sc_signal<T>(s) { do_reg(); }

    static const int maxlen = 100;

    static void debug_cb(void* var, char* mti_value, char format_str)
    {
      T* p = reinterpret_cast<T*>(var);
      std::ostringstream ss;
      ss << std::hex << *p;
      strncpy(mti_value, ss.str().c_str(), maxlen - 2);
      mti_value[maxlen-1] = 0;
    }

    void do_reg() {
#ifdef SC_MTI_REGISTER_CUSTOM_DEBUG
     sc_signal<T>* sig = this;
     SC_MTI_REGISTER_CUSTOM_DEBUG(sig, maxlen, debug_cb);
#endif
    }
  };
#endif // CONNECTIONS_CUSTOM_DEBUG

  template <class T>
  T
  static convert_from_lv(sc_lv<Wrapped<T>::width> lv) {
    Wrapped<T> result;
    Marshaller<Wrapped<T>::width> marshaller(lv);
    result.Marshall(marshaller);
    return result.val;
  };

  template <class T>
  sc_lv<Wrapped<T>::width>
  static convert_to_lv(T v) {
    Marshaller<Wrapped<T>::width> marshaller;
    Wrapped<T> wm(v);
    wm.Marshall(marshaller);
    return marshaller.GetResult();
  };

  class ResetChecker
  {
  protected:
    bool is_reset;
#ifndef __SYNTHESIS__
    const char *name;
    bool is_val_name;
#endif

  public:
    ResetChecker(const char *name_)
      : is_reset(false)
#ifndef __SYNTHESIS__
      ,name(name_)
      ,is_val_name(false)
#endif
    {}

    void reset(bool non_leaf_port) {
      is_reset = true;

      if (non_leaf_port) {
#ifndef __SYNTHESIS__
        std::string name = this->name;
        if (is_val_name) {
          std::string nameSuff = "_";
          nameSuff += _VLDNAMESTR_;
          unsigned int suffLen = nameSuff.length();
          if (name.substr(name.length() - suffLen,suffLen) == nameSuff) { name.erase(name.length() - suffLen,suffLen); }
        } else {
          // Add in hierarchcy to name
          name = std::string(sc_core::sc_get_current_process_b()->get_parent_object()->name()) + "." + this->name;
        }
        SC_REPORT_ERROR("CONNECTIONS-102", ("Port " + name +
              " was reset but it is a non-leaf port. In thread or process '" 
              + std::string(sc_core::sc_get_current_process_b()->basename()) + "'.").c_str());
#endif
      }
    }

#ifndef __SYNTHESIS__
    std::string report_name() {
        std::string name = this->name;
        if (is_val_name) {
          std::string nameSuff = "_";
          nameSuff += _VLDNAMESTR_;
          unsigned int suffLen = nameSuff.length();
          if (name.substr(name.length() - suffLen,suffLen) == nameSuff) { name.erase(name.length() - suffLen,suffLen); }
        } else {
          // Add in hierarchcy to name
          name = std::string(sc_core::sc_get_current_process_b()->get_parent_object()->name()) + "." + this->name;
        }
        return name;
    }
#endif

    bool check() {
      if (!is_reset) {
#ifndef __SYNTHESIS__
        std::string name = report_name();
        SC_REPORT_WARNING("CONNECTIONS-101", ("Port or channel " + name +
            " wasn't reset! In thread or process '" 
            + std::string(sc_core::sc_get_current_process_b()->basename()) + "'.").c_str());
#endif
        is_reset = true;
        return 1;
      }
      return 0;
    }

    void set_val_name(const char *name_) {
#ifndef __SYNTHESIS__
      name = name_;
      is_val_name = true;
#endif
    }
  };

  class SimConnectionsClk;
  SimConnectionsClk &get_sim_clk();
  void set_sim_clk(sc_clock *clk_ptr);

#ifdef CONNECTIONS_SYN_SIM
  inline void set_sim_clk(sc_clock *clk_ptr) {
    DBG_CONNECT("Connections sim clock disabled for synthesis simulation mode");
  }
#endif

  class ConManager;
  ConManager &get_conManager();

#ifdef CONNECTIONS_SIM_ONLY

// Always enable __CONN_RAND_STALL_FEATURE, but stalling itself is
// disabled by default unless explicitly turned on by the variable
// rand_stall_enable() or by CONN_RAND_STALL
#define __CONN_RAND_STALL_FEATURE

  class SimConnectionsClk
  {
  public:
    SimConnectionsClk() {
#ifndef NO_DEFAULT_CONNECTIONS_CLOCK
      // clk_vector.push_back(new sc_clock("default_sim_clk", 1, SC_NS, 0.5, 0, SC_NS, true));
#endif
    }

    void set(sc_clock *clk_ptr_) {
#ifndef HAS_SC_RESET_API
      clk_info_vector.push_back(clk_ptr_);
#endif
    }

    void pre_delay(int c) const {
      wait(adjust_for_edge(get_period_delay(c)-epsilon, c).to_seconds(),SC_SEC);
    }

    void post_delay(int c) const {
      wait(adjust_for_edge(epsilon, c).to_seconds(),SC_SEC);
    }

    inline void post2pre_delay(int c) const {
      wait(clk_info_vector[c].post2pre_delay);
    }

    inline void pre2post_delay() const {
      static sc_time delay(((2*epsilon).to_seconds()), SC_SEC);
      //caching it to optimize wall runtime
      wait(delay);
    }

    inline void period_delay(int c) const {
      wait(clk_info_vector[c].period_delay);
    }

    struct clk_info {
      clk_info(sc_clock *cp) {
        clk_ptr = cp;
        do_sync_reset = 0;
        do_async_reset = 0;
      }
      sc_clock *clk_ptr;
      sc_time post2pre_delay;
      sc_time period_delay;
      sc_time clock_edge; // time of next active edge during simulation, per clock
      bool do_sync_reset;
      bool do_async_reset;
    };

    std::vector<clk_info> clk_info_vector;

    struct clock_alias_info {
      const sc_event *sc_clock_event;
      const sc_event *alias_event;
    };
    std::vector<clock_alias_info> clock_alias_vector;

    void add_clock_alias(const sc_event &sc_clock_event, const sc_event &alias_event) {
      clock_alias_info cai;
      cai.sc_clock_event = &sc_clock_event;
      cai.alias_event = &alias_event;
      clock_alias_vector.push_back(cai);
    }

    void find_clocks(sc_object *obj) {
      sc_clock *clk = dynamic_cast<sc_clock *>(obj);
      if (clk) {
        clk_info_vector.push_back(clk);
        std::cout << "Connections Clock: " << clk->name() << " Period: " << clk->period() << std::endl;
      }

      std::vector<sc_object *> children = obj->get_child_objects();
      for ( unsigned i = 0; i < children.size(); i++ ) {
        if ( children[i] ) { find_clocks(children[i]); }
      }
    }

    void start_of_simulation() {
      // Set epsilon to default value at start of simulation, after time resolution has been set
      epsilon = sc_time(0.01, SC_NS);
      const std::vector<sc_object *> tops = sc_get_top_level_objects();
      for (unsigned i=0; i < tops.size(); i++) {
        if (tops[i]) { find_clocks(tops[i]); }
      }

      for (unsigned c=0; c < clk_info_vector.size(); c++) {
        clk_info_vector[c].post2pre_delay = (sc_time((get_period_delay(c)-2*epsilon).to_seconds(), SC_SEC));
        clk_info_vector[c].period_delay = (sc_time((get_period_delay(c).to_seconds()), SC_SEC));
        clk_info_vector[c].clock_edge = adjust_for_edge(SC_ZERO_TIME,c);
      }
    }

    inline void check_on_clock_edge(int c) {
      if (clk_info_vector[c].clock_edge != sc_time_stamp()) {
        sc_process_handle h = sc_get_current_process_handle();
        std::ostringstream ss;
        ss << "Push or Pop called outside of active clock edge. \n";
        ss << "Process: " << h.name() << "\n";
        ss << "Current simulation time: " << sc_time_stamp() << "\n";
        ss << "Active clock edge: " << clk_info_vector[c].clock_edge << "\n";
        SC_REPORT_ERROR("CONNECTIONS-113", ss.str().c_str());
      }
    }

  private:

    sc_core::sc_time epsilon;

    inline sc_time get_period_delay(int c) const {
      return clk_info_vector.at(c).clk_ptr->period();
    }

    double get_duty_ratio(int c) const {
      return clk_info_vector.at(c).clk_ptr->duty_cycle();
    }

    sc_time adjust_for_edge(sc_time t, int c) const {
      if (!clk_info_vector.at(c).clk_ptr->posedge_first()) {
        return t + get_duty_ratio(c)*get_period_delay(c);
      }

      return t;
    }
  };

// this is an abstract class for both blocking connections
// it is used to allow a containter of pointers to any blocking connection
  class Blocking_abs
  {
  public:
    Blocking_abs() {
       DBG_CONNECT("Blocking_abs CTOR: " << std::hex << (void*)this );
    };
    virtual ~Blocking_abs() {}
    virtual bool Post() {return false;};
    virtual bool Pre()  {return false;};
    virtual bool PrePostReset()  {return false;};
    virtual std::string full_name() { return "unnamed"; }
    bool clock_registered{0};
    bool non_leaf_port{0};
    bool disable_spawn_true{0};
    virtual void disable_spawn() {}
    int  clock_number{0};
    virtual bool do_reset_check() {return 0;}
    virtual std::string report_name() {return std::string("unnamed"); }
    Blocking_abs *sibling_port{0};
  };


  class ConManager
  {
  public:
    ConManager() { }

    ~ConManager() {
      for (std::vector<std::vector<Blocking_abs *>*>::iterator it=tracked_per_clk.begin(); it!=tracked_per_clk.end(); ++it) {
        delete *it;
      }
      tracked_per_clk.clear();
    }

    std::vector<Blocking_abs *> tracked;
    std::vector<Connections_BA_abs *> tracked_annotate;

    void add(Blocking_abs *c) {
      tracked.push_back(c);
    }

    std::map<const Blocking_abs *, const sc_event *> map_port_to_event;
    std::map<const sc_event *, int> map_event_to_clock; // value is clock # + 1, so that 0 is error

    struct process_reset_info {
      process_reset_info() {
        process_ptr = 0;
        async_reset_level = false;
        sync_reset_level = false;
        async_reset_sig_if = 0;
        sync_reset_sig_if = 0;
        clk = 0;
      }
      unsigned clk;
      sc_process_b *process_ptr;
      bool        async_reset_level;
      bool        sync_reset_level;
      const sc_signal_in_if<bool> *async_reset_sig_if;
      const sc_signal_in_if<bool> *sync_reset_sig_if;
      bool operator == (const process_reset_info &rhs) {
        return (async_reset_sig_if == rhs.async_reset_sig_if) &&
               ( sync_reset_sig_if == rhs.sync_reset_sig_if) &&
               (async_reset_level  == rhs.async_reset_level) &&
               ( sync_reset_level  == rhs.sync_reset_level);
      }

      std::string to_string() {
        std::ostringstream ss;
        ss << "Process name: " << process_ptr->name();
        if (async_reset_sig_if) {
          const char *nm = "unknown";
          const sc_object *ob = dynamic_cast<const sc_object *>(async_reset_sig_if);
          sc_assert(ob);
          nm = ob->name();
          ss << " async reset signal: " << std::string(nm) << " level: " << async_reset_level ;
        }

        if (sync_reset_sig_if) {
          const char *nm = "unknown";
          const sc_object *ob = dynamic_cast<const sc_object *>(sync_reset_sig_if);
          sc_assert(ob);
          nm = ob->name();
          ss << " sync reset signal: " << std::string(nm) << " level: " << sync_reset_level ;
        }

        return ss.str();
      }
    };
    std::map<int, process_reset_info> map_clk_to_reset_info;
    std::map<sc_process_b *, process_reset_info> map_process_to_reset_info;
    bool sim_clk_initialized;

    std::vector<std::vector<Blocking_abs *>*> tracked_per_clk;

    void init_sim_clk() {
      if (sim_clk_initialized) { return; }

      sim_clk_initialized = true;

      get_sim_clk().start_of_simulation();

      for (unsigned c=0; c < get_sim_clk().clk_info_vector.size(); c++) {
        map_event_to_clock[&(get_sim_clk().clk_info_vector[c].clk_ptr->posedge_event())] = c + 1; // add +1 encoding
        std::ostringstream ss, ssync;
        ss << "connections_manager_run_" << c;
        sc_spawn(sc_bind(&ConManager::run, this, c), ss.str().c_str());
        tracked_per_clk.push_back(new std::vector<Blocking_abs *>);
        ssync << ss.str();
        ss << "async_reset_thread";
        sc_spawn(sc_bind(&ConManager::async_reset_thread, this, c), ss.str().c_str());
        ssync << "sync_reset_thread";
        sc_spawn(sc_bind(&ConManager::sync_reset_thread, this, c), ssync.str().c_str());
      }

      for (unsigned c=0; c < get_sim_clk().clock_alias_vector.size(); c++) {
        int resolved = map_event_to_clock[get_sim_clk().clock_alias_vector[c].sc_clock_event];
        if (resolved) {
          map_event_to_clock[get_sim_clk().clock_alias_vector[c].alias_event] = resolved;
        } else {
          SC_REPORT_ERROR("CONNECTIONS-225", "Could not resolve alias clock!");
        }
      }

      sc_spawn(sc_bind(&ConManager::check_registration, this, true), "check_registration");
    }

    void async_reset_thread(int c) {
      wait(10, SC_PS);
      process_reset_info &pri = map_clk_to_reset_info[c];

      if (pri.async_reset_sig_if == 0) { return; }

      while (1) {
        wait(pri.async_reset_sig_if->value_changed_event());
        // std::cout << "see async reset val now: " << pri.async_reset_sig_if->read() << "\n";
        get_sim_clk().clk_info_vector[c].do_async_reset =
          (pri.async_reset_sig_if->read() == pri.async_reset_level);
      }
    }

    void sync_reset_thread(int c) {
      wait(10, SC_PS);
      process_reset_info &pri = map_clk_to_reset_info[c];

      if (pri.sync_reset_sig_if == 0) { return; }

      while (1) {
        wait(pri.sync_reset_sig_if->value_changed_event());
        // std::cout << "see sync reset val now: " << pri.sync_reset_sig_if->read() << "\n";
        get_sim_clk().clk_info_vector[c].do_sync_reset =
          (pri.sync_reset_sig_if->read() == pri.sync_reset_level);
      }
    }


    void check_registration(bool b) {
      wait(50, SC_PS); // allow all Reset calls to complete, so that add_clock_event calls are all done

      bool error{0};

      // first produce list of all warnings
      for (unsigned i=0; i < tracked.size(); i++) { error |= tracked[i]->do_reset_check(); }

      if (error) {
         SC_REPORT_ERROR("CONNECTIONS-125",
           std::string("Unable to resolve clock on port - check and fix any prior warnings about missing Reset() on ports: ").c_str());
         sc_stop();
      }

      for (unsigned i=0; i < tracked.size(); i++) {
        if (!!tracked[i] && (!tracked[i]->clock_registered || (map_port_to_event[tracked[i]] == 0))) {
          DBG_CONNECT("check_registration: unreg port " << std::hex << tracked[i] << " (" << tracked[i]->full_name() << ")");

          bool resolved = false;
          int clock_number = 0;

          Blocking_abs* sib = 0;
          for (sib = tracked[i]; sib->sibling_port; sib = sib->sibling_port) {
            DBG_CONNECT("  sibling port traversal to: " << std::hex << sib->sibling_port << " (" << sib->sibling_port->full_name() << ")");
          }

          resolved = sib->clock_registered;

          if (resolved)
            clock_number = sib->clock_number;

          DBG_CONNECT("  resolution and clock_number: " << resolved << " " << clock_number);

          if (tracked[i]->sibling_port) {
            tracked[i]->clock_number = clock_number;
            tracked[i]->clock_registered = true;
            tracked_per_clk[tracked[i]->clock_number]->push_back(tracked[i]);
            continue;
          }

          // See Combinational_SimPorts_abs::do_reset_check() for explanation
          if (tracked[i]->full_name() == "Combinational_SimPorts_abs")  continue; 

          const sc_object *ob = dynamic_cast<const sc_object *>(tracked[i]);
          std::string nm(ob?ob->name():"unnamed");

          if (!resolved && (get_sim_clk().clk_info_vector.size() > 1)) {
            SC_REPORT_ERROR("CONNECTIONS-125",
                          (std::string("Unable to resolve clock on port - check and fix any prior warnings about missing Reset() on ports: "
                                       + nm + " " + tracked[i]->full_name() + " (" + tracked[i]->report_name() + ")").c_str()));
          }
        }
      }

      for (unsigned i=0; i < get_sim_clk().clk_info_vector.size(); i++) {
        std::vector<process_reset_info> v;
        decltype(map_process_to_reset_info)::iterator it;
        for (it = map_process_to_reset_info.begin(); it != map_process_to_reset_info.end(); it++) {
          if (it->second.clk == i) { v.push_back(it->second); }
        }

        if (v.size() > 1)
          for (unsigned u=0; u<v.size(); u++)
            if (!(v[0] == v[u])) {
              std::ostringstream ss;
              ss << "Two processes using same clock have different reset specs: \n"
                 << v[0].to_string() << "\n"
                 << v[u].to_string() << "\n";
              SC_REPORT_WARNING("CONNECTIONS-212", ss.str().c_str());
            }
      }
    }

    void add_clock_event(Blocking_abs *c) {
      init_sim_clk();

      if (c->clock_registered) { return; }

      c->clock_registered = true;

      class my_process : public sc_process_b
      {
      public:
#ifdef HAS_SC_RESET_API
        // Code written in restricted style to work on OSCI/Accellera sim as well as Questa and VCS
        // when SYSTEMC_HOME/sysc/kernel/sc_reset.h is available
        int static_events_size() { return m_static_events.size(); }
        const sc_event *first_event() { return m_static_events[0]; }
        std::vector<sc_reset *> &get_sc_reset_vector() { return m_resets; }
#else
        // Remove dependency on sc_reset.h, but requires user call Connections::set_sim_clk(&clk) before sc_start() 
        int static_events_size() { return 1; }
        const sc_event *first_event() { 
          SimConnectionsClk::clk_info &ci = get_sim_clk().clk_info_vector[0];
          return &(ci.clk_ptr->posedge_event());
        }
#endif
      };

      sc_process_handle h = sc_get_current_process_handle();
      sc_object *o = h.get_process_object();
      sc_process_b *b = dynamic_cast<sc_process_b *>(o);
      sc_assert(b);
      my_process *m = static_cast<my_process *>(b);
      if (m->static_events_size() != 1)
        SC_REPORT_ERROR("CONNECTIONS-112",
                        (std::string("Process does not have static sensitivity to exactly 1 event: " +
                                     std::string(h.name())).c_str()));

      const sc_event *e = m->first_event();
      map_port_to_event[c] = e;

      int clk = map_event_to_clock[e];
      if (clk <= 0) {
        SC_REPORT_ERROR("CONNECTIONS-111",
                        (std::string("Failed to find sc_clock for process: " + std::string(h.name())).c_str()));
        SC_REPORT_ERROR("CONNECTIONS-111", "Stopping sim due to fatal error.");
        sc_stop();
        return;
      }

      --clk; // undo +1 encoding for errors

      tracked_per_clk[clk]->push_back(c);
      c->clock_number = clk;
      DBG_CONNECT("add_clock_event: port " << std::hex << c << " clock_number " << clk << " process " << h.name());

      sc_clock* clk_ptr = get_sim_clk().clk_info_vector[clk].clk_ptr;
      if (!clk_ptr->posedge_first()) {
                std::ostringstream ss;
                ss << "clk posedge_first() != true : process: "
                   << h.name() << " "
                   << "\n";
                SC_REPORT_ERROR("CONNECTIONS-303", ss.str().c_str());
      }

      if (clk_ptr->start_time().value() % clk_ptr->period().value()) {
                std::ostringstream ss;
                ss << "clk start_time is not a multiple of clk period: process: "
                   << h.name() << " "
                   << "\n";
                SC_REPORT_ERROR("CONNECTIONS-304", ss.str().c_str());
      }

#ifdef HAS_SC_RESET_API
      class my_reset : public sc_reset
      {
      public:
        const sc_signal_in_if<bool> *get_m_iface_p() { return m_iface_p; }
        std::vector<sc_reset_target> &get_m_targets() { return m_targets; }
      };

      // Here we enforce that all processes using a clock have:
      //  - consistent async resets (at most 1)
      //  - consistent sync resets  (at most 1)
      // Note that this applies to entire SC model (ie both DUT and TB)

      for (unsigned r=0; r < m->get_sc_reset_vector().size(); r++) {
        my_reset *mr = static_cast<my_reset *>(m->get_sc_reset_vector()[r]);
        sc_assert(mr);
        const sc_object *ob = dynamic_cast<const sc_object *>(mr->get_m_iface_p());
        sc_assert(ob);

        for (unsigned t=0; t < mr->get_m_targets().size(); t++)
          if (mr->get_m_targets()[t].m_process_p == b) {
            bool level = mr->get_m_targets()[t].m_level;

            process_reset_info &pri = map_process_to_reset_info[b];
            pri.process_ptr = b;
            pri.clk = clk;

            if (mr->get_m_targets()[t].m_async) {
              if (pri.async_reset_sig_if == 0) {
                pri.async_reset_sig_if = mr->get_m_iface_p();
                pri.async_reset_level = level;
              }

              if (pri.async_reset_sig_if != mr->get_m_iface_p()) {
                std::ostringstream ss;
                ss << "Mismatching async reset signal objects for same process: process: "
                   << h.name() << " "
                   << "\n";
                SC_REPORT_ERROR("CONNECTIONS-212", ss.str().c_str());
              }

              if (pri.async_reset_level != level) {
                std::ostringstream ss;
                ss << "Mismatching async reset signal level for same process: process: "
                   << h.name() << " "
                   << "\n";
                SC_REPORT_ERROR("CONNECTIONS-212", ss.str().c_str());
              }
            } else {
              if (pri.sync_reset_sig_if == 0) {
                pri.sync_reset_sig_if = mr->get_m_iface_p();
                pri.sync_reset_level = level;
              }

              if (pri.sync_reset_sig_if != mr->get_m_iface_p()) {
                std::ostringstream ss;
                ss << "Mismatching sync reset signal objects for same process: process: "
                   << h.name() << " "
                   << "\n";
                SC_REPORT_ERROR("CONNECTIONS-212", ss.str().c_str());
              }

              if (pri.sync_reset_level != level) {
                std::ostringstream ss;
                ss << "Mismatching sync reset signal level for same process: process: "
                   << h.name() << " "
                   << "\n";
                SC_REPORT_ERROR("CONNECTIONS-212", ss.str().c_str());
              }
            }

            map_clk_to_reset_info[clk] = pri;
          }
      }
#endif //HAS_SC_RESET_API
    }

    void add_annotate(Connections_BA_abs *c) {
      tracked_annotate.push_back(c);
    }

    void remove(Blocking_abs *c) {
      for (std::vector<Blocking_abs *>::iterator it = tracked.begin(); it!=tracked.end(); ++it) {
        if (*it == c) {
          tracked.erase(it);
          return;
        }
      }

      CONNECTIONS_ASSERT_MSG(0, "Couldn't find port to remove from ConManager sim accurate tracking!");
    }

    void remove_annotate(Connections_BA_abs *c) {
      for (std::vector<Connections_BA_abs *>::iterator it = tracked_annotate.begin(); it!=tracked_annotate.end(); ++it) {
        if (*it == c) {
          tracked_annotate.erase(it);
          return;
        }
      }

      CONNECTIONS_ASSERT_MSG(0, "Couldn't find port to remove from ConManager back-annotation tracking!");
    }

    void run(int clk) {
      get_sim_clk().post_delay(clk);  // align to occur just after the cycle

      SimConnectionsClk::clk_info &ci = get_sim_clk().clk_info_vector[clk];

      while (1) {
        // Post();
        for (std::vector<Blocking_abs *>::iterator it=tracked_per_clk[clk]->begin(); it!=tracked_per_clk[clk]->end(); ) {
          if ((*it)->Post()) {
            ++it;
          } else {
            tracked_per_clk[clk]->erase(it);
          }
        }

        get_sim_clk().post2pre_delay(clk);

        //Pre();
        for (std::vector<Blocking_abs *>::iterator it=tracked_per_clk[clk]->begin(); it!=tracked_per_clk[clk]->end(); ) {
          if ((*it)->Pre()) {
            ++it;
          } else {
            tracked_per_clk[clk]->erase(it);
          }
        }
        ci.clock_edge += ci.period_delay;

        if (ci.do_sync_reset || ci.do_async_reset) {
          for (std::vector<Blocking_abs *>::iterator it=tracked_per_clk[clk]->begin();
               it!=tracked_per_clk[clk]->end(); ++it) {
            (*it)->PrePostReset();
          }
        }

        get_sim_clk().pre2post_delay();

        if (ci.do_sync_reset || ci.do_async_reset) {
          for (std::vector<Blocking_abs *>::iterator it=tracked_per_clk[clk]->begin();
               it!=tracked_per_clk[clk]->end(); ++it) {
            (*it)->PrePostReset();
          }
        }
      }
    }
  };

// See: https://stackoverflow.com/questions/18860895/how-to-initialize-static-members-in-the-header
  template <class Dummy>
  struct ConManager_statics {
    static SimConnectionsClk sim_clk;
    static ConManager conManager;
    static bool rand_stall_enable;
    static bool rand_stall_print_debug_enable;
    static unsigned int rand_stall_seed;
    static bool rand_stall_seed_init;
    static bool set_rand_stall_seed() { 
      if ( rand_stall_enable && !!rand_stall_seed ) {
        srand(rand_stall_seed);
        return true;
      }
      return false;
    }
  };

  inline void set_sim_clk(sc_clock *clk_ptr)
  {
#ifndef HAS_SC_RESET_API
    ConManager_statics<void>::sim_clk.clk_info_vector.push_back(clk_ptr);
#endif
  }

  template <class Dummy>
  SimConnectionsClk ConManager_statics<Dummy>::sim_clk;
  template <class Dummy>
  ConManager ConManager_statics<Dummy>::conManager;

  inline SimConnectionsClk &get_sim_clk()
  {
#ifndef HAS_SC_RESET_API
    CONNECTIONS_ASSERT_MSG(ConManager_statics<void>::sim_clk.clk_info_vector.size() > 0, "You must call Connections::set_sim_clk(&clk) before sc_start()");
#endif
    return ConManager_statics<void>::sim_clk;
  }

  inline ConManager &get_conManager()
  {
    return ConManager_statics<void>::conManager;
  }

#ifdef __CONN_RAND_STALL_FEATURE

#ifdef CONN_RAND_STALL
  template <class Dummy>
  bool ConManager_statics<Dummy>::rand_stall_enable = true;
#else
  template <class Dummy>
  bool ConManager_statics<Dummy>::rand_stall_enable = false;
#endif // ifdef CONN_RAND_STALL

#ifdef CONN_RAND_STALL_PRINT_DEBUG
  template <class Dummy>
  bool ConManager_statics<Dummy>::rand_stall_print_debug_enable = true;
#else
  template <class Dummy>
  bool ConManager_statics<Dummy>::rand_stall_print_debug_enable = false;
#endif // ifdef CONN_RAND_STALL_PRINT_DEBUG

#if defined(RAND_SEED)
  template <class Dummy>
  unsigned int ConManager_statics<Dummy>::rand_stall_seed = static_cast<unsigned int>(RAND_SEED);
#elif defined(USE_TIME_RAND_SEED)
  template <class Dummy>
  unsigned int ConManager_statics<Dummy>::rand_stall_seed = static_cast<unsigned int>(time(NULL));
#endif
  template <class Dummy>
  bool ConManager_statics<Dummy>::rand_stall_seed_init = ConManager_statics<Dummy>::set_rand_stall_seed();

  inline bool &get_rand_stall_enable()
  {
    return ConManager_statics<void>::rand_stall_enable;
  }

  inline bool &get_rand_stall_print_debug_enable()
  {
    return ConManager_statics<void>::rand_stall_print_debug_enable;
  }

  /**
   * \brief Enable global random stalling support.
   * \ingroup Connections
   *
   * Enable random stalling globally, valid in CONNECTIONS_ACCURATE_SIM and
   * CONNECTIONS_FAST_SIM modes. Random stalling is non engaged by default,
   * unless CONN_RAND_STALL is set.
   *
   * Random stalling is used to randomly stall In<> ports, creating random
   * back pressure in a design to assist catching latency-sentitive bugs.
   *
   * Can also use Connections::In<>.enable_local_rand_stall() to enable
   * on a per-port basis.
   *
   * \par A Simple Example
   * \code
   *      #include <connections/connections.h>
   *
   *      int sc_main(int argc, char *argv[])
   *      {
   *      ...
   *      Connections::enable_global_rand_stall();
   *      ...
   *      }
   * \endcode
   * \par
   *
   */
  inline void enable_global_rand_stall()
  {
    ConManager_statics<void>::rand_stall_enable = true;
  }

  /**
   * \brief Disable global random stalling support.
   * \ingroup Connections
   *
   * Disable random stalling globally, valid in CONNECTIONS_ACCURATE_SIM and
   * CONNECTIONS_FAST_SIM modes. Random stalling is non engaged by default,
   * unless CONN_RAND_STALL is set.
   *
   * Random stalling is used to randomly stall In<> ports, creating random
   * back pressure in a design to assist catching latency-sentitive bugs.
   *
   * Can also use Connections::In<>.disable_local_rand_stall() to disable
   * on a per-port basis.
   *
   * \par A Simple Example
   * \code
   *      #include <connections/connections.h>
   *
   *      int sc_main(int argc, char *argv[])
   *      {
   *      ...
   *      Connections::disable_global_rand_stall();
   *      ...
   *      }
   * \endcode
   * \par
   *
   */
  inline void disable_global_rand_stall()
  {
    ConManager_statics<void>::rand_stall_enable = false;
  }

  /**
   * \brief Enable global random stalling debug print statements.
   * \ingroup Connections
   *
   * Enable random stalling print statements globally. Must already have
   * global random stalling enabled.
   *
   * Print statements will report when stall is entered, exited, and cycle
   * count in stall.
   *
   * Can also use Connections::In<>.enable_local_rand_stall_print_debug() to enable
   * on a per-port basis.
   *
   * \par A Simple Example
   * \code
   *      #include <connections/connections.h>
   *
   *      int sc_main(int argc, char *argv[])
   *      {
   *      ...
   *      Connections::enable_global_rand_stall();
   *      Connections::enable_global_rand_stall_print_debug();
   *      ...
   *      }
   * \endcode
   * \par
   *
   */
  inline void enable_global_rand_stall_print_debug()
  {
    ConManager_statics<void>::rand_stall_print_debug_enable = true;
  }

  /**
   * \brief Disable global random stalling debug print statements.
   * \ingroup Connections
   *
   * Disable random stalling print statements globally. Must already have
   * global random stalling enabled.
   *
   * Can also use Connections::In<>.disable_local_rand_stall_print_debug() to enable
   * on a per-port basis.
   *
   * \par A Simple Example
   * \code
   *      #include <connections/connections.h>
   *
   *      int sc_main(int argc, char *argv[])
   *      {
   *      ...
   *      Connections::disable_global_rand_stall_print_debug();
   *      ...
   *      }
   * \endcode
   * \par
   *
   */
  inline void disable_global_rand_stall_print_debug()
  {
    ConManager_statics<void>::rand_stall_print_debug_enable = false;
  }

#endif //__CONN_RAND_STALL_FEATURE

#endif //CONNECTIONS_SIM_ONLY

//------------------------------------------------------------------------
// InBlocking MARSHALL_PORT
//------------------------------------------------------------------------

// For safety, disallow DIRECT_PORT <-> MARSHALL_PORT binding helpers during HLS.
#ifndef __SYNTHESIS__

  template <typename Message>
  SC_MODULE(MarshalledToDirectOutPort)
  {
    typedef Wrapped<Message> WMessage;
    static const unsigned int width = WMessage::width;
    typedef sc_lv<WMessage::width> MsgBits;
    sc_signal<MsgBits> msgbits;
    sc_out<Message> _DATNAME_;
    sc_in<bool> _VLDNAME_;
#ifdef CONNECTIONS_SIM_ONLY
    Blocking_abs *sibling_port = 0; 
    /** stuart: TODO
        This module probably should inherit from Blocking_abs so that the 
        sibling_port->sibling_port traversal in check_registration fully works
    **/
#endif
    void do_marshalled2direct() {
      if (_VLDNAME_) {
        MsgBits mbits = msgbits.read();
        Marshaller<WMessage::width> marshaller(mbits);
        WMessage result;
        result.Marshall(marshaller);
        _DATNAME_.write(result.val);
      }
    }

    SC_CTOR(MarshalledToDirectOutPort) {
      SC_METHOD(do_marshalled2direct);
      sensitive << msgbits;
      sensitive << _VLDNAME_;
    }
  };

  template <typename Message>
  SC_MODULE(MarshalledToDirectInPort)
  {
    typedef Wrapped<Message> WMessage;
    static const unsigned int width = WMessage::width;
    typedef sc_lv<WMessage::width> MsgBits;
    sc_in<MsgBits> msgbits;
    sc_in<bool> _VLDNAME_;
    sc_signal<Message> _DATNAME_;
#ifdef CONNECTIONS_SIM_ONLY
    Blocking_abs *sibling_port = 0;
    /** stuart: TODO
        This module probably should inherit from Blocking_abs so that the 
        sibling_port->sibling_port traversal in check_registration fully works
    **/
#endif
    void do_marshalled2direct() {
      if (_VLDNAME_) {
        MsgBits mbits = msgbits.read();
        Marshaller<WMessage::width> marshaller(mbits);
        WMessage result;
        result.Marshall(marshaller);
        _DATNAME_.write(result.val);
      }
    }

    SC_CTOR(MarshalledToDirectInPort) {
      SC_METHOD(do_marshalled2direct);
      sensitive << msgbits;
      sensitive << _VLDNAME_;
    }
  };

  template <typename Message>
  SC_MODULE(DirectToMarshalledInPort)
  {
    typedef Wrapped<Message> WMessage;
    static const unsigned int width = WMessage::width;
    typedef sc_lv<WMessage::width> MsgBits;
    sc_in<Message> _DATNAME_;
    sc_signal<MsgBits> msgbits;
#ifdef CONNECTIONS_SIM_ONLY
    Blocking_abs *sibling_port = 0;
    /** stuart: TODO
        This module probably should inherit from Blocking_abs so that the 
        sibling_port->sibling_port traversal in check_registration fully works
    **/
#endif
    void do_direct2marshalled() {
      Marshaller<WMessage::width> marshaller;
      //WMessage wm(msg);
      WMessage wm(_DATNAME_.read());
      wm.Marshall(marshaller);
      MsgBits bits = marshaller.GetResult();
      msgbits.write(bits);
    }

    SC_CTOR(DirectToMarshalledInPort) {
      SC_METHOD(do_direct2marshalled);
      sensitive << _DATNAME_;
    }
  };

  template <typename Message>
  SC_MODULE(DirectToMarshalledOutPort)
  {
    typedef Wrapped<Message> WMessage;
    static const unsigned int width = WMessage::width;
    typedef sc_lv<WMessage::width> MsgBits;
    sc_signal<Message> _DATNAME_;
    sc_out<MsgBits> msgbits;
#ifdef CONNECTIONS_SIM_ONLY
    Blocking_abs *sibling_port = 0;
    /** stuart: TODO
        This module probably should inherit from Blocking_abs so that the 
        sibling_port->sibling_port traversal in check_registration fully works
    **/
#endif
    void do_direct2marshalled() {
      Marshaller<WMessage::width> marshaller;
      WMessage wm(_DATNAME_);
      wm.Marshall(marshaller);
      MsgBits bits = marshaller.GetResult();
      msgbits.write(bits);
    }

    SC_CTOR(DirectToMarshalledOutPort) {
      SC_METHOD(do_direct2marshalled);
      sensitive << _DATNAME_;
    }
  };


// TLM
#if defined(CONNECTIONS_SIM_ONLY)

  template <typename Message>
  class TLMToDirectOutPort : public Blocking_abs
  {
  public:
    sc_out<bool> _VLDNAME_;
    sc_in<bool> _RDYNAME_;
    sc_out<Message> _DATNAME_;

    // Default constructor
    explicit TLMToDirectOutPort(tlm::tlm_fifo<Message> &fifo)
      : _VLDNAME_(sc_gen_unique_name(_VLDNAMEOUTSTR_)),
        _RDYNAME_(sc_gen_unique_name(_RDYNAMEOUTSTR_ )),
        _DATNAME_(sc_gen_unique_name(_DATNAMEOUTSTR_)) {

      Init_SIM(sc_gen_unique_name("out", fifo));

    }

    // Constructor
    explicit TLMToDirectOutPort(const char *name, tlm::tlm_fifo<Message> &fifo)
      : _VLDNAME_(CONNECTIONS_CONCAT(name, _VLDNAMESTR_)),
        _RDYNAME_(CONNECTIONS_CONCAT(name, _RDYNAMESTR_)),
        _DATNAME_(CONNECTIONS_CONCAT(name, _DATNAMESTR_)) {
      Init_SIM(name, fifo);
    }

    virtual ~TLMToDirectOutPort() {}

  protected:
    // Protected member variables
    tlm::tlm_fifo<Message> *fifo;

    // Initializer
    void Init_SIM(const char *name, tlm::tlm_fifo<Message> &fifo) {
      this->fifo = &fifo;
      get_conManager().add(this);
    }

    std::string full_name() { return "TLMToDirectOutPort"; }

    // Blocking_abs functions
    bool Pre() {
      if (_VLDNAME_.read() && _RDYNAME_.read()) {
        CONNECTIONS_ASSERT_MSG(fifo->nb_can_get(), "Vld and rdy indicated data was available, but no data was available!");
        fifo->get(); // Discard, we've already peeked it. Just incrementing the head here.
      }
      return true;
    }

    bool PrePostReset() {
      Message blank_m;
      while (fifo->nb_get(blank_m));
      return true;
    }

    bool Post() {
      if (fifo->nb_can_peek()) {
        _VLDNAME_.write(true);
        _DATNAME_.write(fifo->peek());
      } else {
        Message blank_m;
        _VLDNAME_.write(false);
        _DATNAME_.write(blank_m);
      }
      return true;
    }
  };

  template <typename Message>
  class DirectToTLMInPort : public Blocking_abs
  {
  public:
    sc_in<bool> _VLDNAME_;
    sc_out<bool> _RDYNAME_;
    sc_in<Message> _DATNAME_;

    // Default constructor
    explicit DirectToTLMInPort(tlm::tlm_fifo<Message> &fifo)
      : _VLDNAME_(sc_gen_unique_name(_VLDNAMEINSTR_)),
        _RDYNAME_(sc_gen_unique_name(_RDYNAMEINSTR_)),
        _DATNAME_(sc_gen_unique_name(_DATNAMEINSTR_)) {

      Init_SIM(sc_gen_unique_name("in", fifo));

    }

    // Constructor
    explicit DirectToTLMInPort (const char *name, tlm::tlm_fifo<Message> &fifo)
      : _VLDNAME_(CONNECTIONS_CONCAT(name, _VLDNAMESTR_)),
        _RDYNAME_(CONNECTIONS_CONCAT(name, _RDYNAMESTR_)),
        _DATNAME_(CONNECTIONS_CONCAT(name, _DATNAMESTR_)) {
      Init_SIM(name, fifo);
    }
    
    virtual ~DirectToTLMInPort() {}

  protected:
    // Protected member variables
    tlm::tlm_fifo<Message> *fifo;

    // Initializer
    void Init_SIM(const char *name, tlm::tlm_fifo<Message> &fifo) {
      this->fifo = &fifo;
      get_conManager().add(this);
    }

    std::string full_name() { return "DirectToTLMInPort"; }

    bool Pre() {
      if (_RDYNAME_.read() && _VLDNAME_.read()) {
        Message data;
        data = _DATNAME_.read();
        CONNECTIONS_ASSERT_MSG(fifo->nb_can_put(), "Vld and rdy indicated data was available, but no data was available!");
        fifo->put(data);
      }
      return true;
    }

    bool PrePostReset() {
      // The get() side clears the fifo, nothing to do on the put() side here.
      return true;
    }

    bool Post() {
      if (fifo->nb_can_put()) {
        _RDYNAME_.write(true);
      } else {
        _RDYNAME_.write(false);
      }
      return true;
    }
  };

  class in_port_marker : public sc_object
  {
  public:

    unsigned w;
    bool named;
    sc_in<bool> *_VLDNAME_;
    sc_out<bool> *_RDYNAME_;
    sc_port_base *_DATNAME_;
    sc_object *bound_to;
    bool top_port;

    in_port_marker() {
      named = false;
      _VLDNAME_ = 0;
      _RDYNAME_ = 0;
      _DATNAME_ = 0;
      bound_to = 0;
      top_port = false;
    }

    in_port_marker(const char *name, unsigned _w, sc_in<bool> *_val, sc_out<bool> *_rdy, sc_port_base *_msg)
      : sc_object(name) {
      named = true;
      _VLDNAME_ = _val;
      _RDYNAME_ = _rdy;
      _DATNAME_ = _msg;
      bound_to = 0;
      top_port = false;

      w = _w;
    }

    void end_of_elaboration() {
      if (_VLDNAME_) { bound_to = dynamic_cast<sc_object *>(_VLDNAME_->operator->()); }
    }
  };

  class out_port_marker : public sc_object
  {
  public:

    unsigned w;
    bool named;
    sc_out<bool> *_VLDNAME_;
    sc_in<bool> *_RDYNAME_;
    sc_port_base *_DATNAME_;
    sc_object *bound_to;
    bool top_port;

    out_port_marker() {
      named = false;
      _VLDNAME_ = 0;
      _RDYNAME_ = 0;
      _DATNAME_ = 0;
      bound_to = 0;
      top_port = false;
    }

    out_port_marker(const char *name, unsigned _w, sc_out<bool> *_val, sc_in<bool> *_rdy, sc_port_base *_msg)
      : sc_object(name) {
      named = true;
      _VLDNAME_ = _val;
      _RDYNAME_ = _rdy;
      _DATNAME_ = _msg;
      bound_to = 0;
      top_port = false;

      w = _w;
    }

    void end_of_elaboration() {
      if (_VLDNAME_) { bound_to = dynamic_cast<sc_object *>(_VLDNAME_->operator->()); }
    }
  };

#endif // defined(CONNECTIONS_SIM_ONLY)

#endif // __SYNTHESIS__

  // Collect dynamically allocated objects
  class CollectAllocs
  {
  public:
    CollectAllocs() {}
    virtual ~CollectAllocs() {
#ifndef __SYNTHESIS__
      for (std::vector<sc_module*>::iterator it=sc_mod_alloc.begin(); it!=sc_mod_alloc.end(); ++it) {
        delete *it;
      }
      sc_mod_alloc.clear();
#ifdef CONNECTIONS_SIM_ONLY
      for (std::vector<Blocking_abs*>::iterator it=con_obj_alloc.begin(); it!=con_obj_alloc.end(); ++it) {
        delete *it;
      }
      con_obj_alloc.clear();
#endif
#endif
    }
#ifndef __SYNTHESIS__
    std::vector<sc_module*> sc_mod_alloc;
#ifdef CONNECTIONS_SIM_ONLY
    std::vector<Blocking_abs*> con_obj_alloc;
#endif
#endif
  };


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  template <typename Message>
#ifdef CONNECTIONS_SIM_ONLY
  class InBlocking_abs : public Blocking_abs, public CollectAllocs
#else
  class InBlocking_abs : public CollectAllocs
#endif
  {
  public:

    // Protected because generic abstract class
  protected:

    ResetChecker read_reset_check;

    // Default constructor
    InBlocking_abs()
      :
#ifdef CONNECTONS_SIM_ONLY
      Blocking_abs(),
#endif
      read_reset_check("unnamed_in") {}

    // Constructor
    explicit InBlocking_abs(const char *name)
      :
#ifdef CONNECTONS_SIM_ONLY
      Blocking_abs(),
#endif
      read_reset_check(name) {}

  public:
    virtual ~InBlocking_abs() {}

    // Reset read
    virtual void Reset() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }

// Pop
#pragma design modulario < in >
    virtual Message Pop() {
      Message m;
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
      return m;
    }

// Peek
#pragma design modulario < in >
    virtual Message Peek() {
      Message m;
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
      return m;
    }

    bool PeekNB(Message &/*data*/, const bool &/*unused*/ = true) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
      return false;      
    }

// PopNB
#pragma design modulario < in >
    virtual bool PopNB(Message &data, const bool &do_wait = true) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
      return false;
    }
  };


  template <typename Message>
  class InBlocking_Ports_abs : public InBlocking_abs<Message>
  {
  public:
    // Interface
    sc_in<bool> _VLDNAME_;
    sc_out<bool> _RDYNAME_;

    // Protected because generic
  protected:
    // Default constructor
    InBlocking_Ports_abs()
      : InBlocking_abs<Message>(),
        _VLDNAME_(sc_gen_unique_name(_VLDNAMEINSTR_)),
        _RDYNAME_(sc_gen_unique_name(_RDYNAMEINSTR_)) {
#ifndef __SYNTHESIS__
      this->read_reset_check.set_val_name(_VLDNAME_.name());
#endif
    }

    // Constructor
    explicit InBlocking_Ports_abs(const char *name)
      : InBlocking_abs<Message>(name),
        _VLDNAME_(CONNECTIONS_CONCAT(name, _VLDNAMESTR_)),
        _RDYNAME_(CONNECTIONS_CONCAT(name, _RDYNAMESTR_)) {
#ifndef __SYNTHESIS__
      this->read_reset_check.set_val_name(_VLDNAME_.name());
#endif
    }

  public:
    virtual ~InBlocking_Ports_abs() {}

    // Reset read
    virtual void Reset() {
#ifdef CONNECTIONS_SIM_ONLY
      this->read_reset_check.reset(this->non_leaf_port);
#else
      this->read_reset_check.reset(false);
#endif
      _RDYNAME_.write(false);
#ifdef CONNECTIONS_SIM_ONLY
      get_conManager().add_clock_event(this);
#endif
    }

    bool do_reset_check() {
      return this->read_reset_check.check();
    }

#ifndef __SYNTHESIS__
    std::string report_name() {
      return this->read_reset_check.report_name();
    }
#endif

// Pop
#pragma builtin_modulario
#pragma design modulario < in >
    Message Pop() {
      // this->read_reset_check.check();
#ifdef CONNECTIONS_SIM_ONLY
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      do {
        _RDYNAME_.write(true);
        wait();
      } while (_VLDNAME_.read() != true);
      _RDYNAME_.write(false);

      Message m;
      read_msg(m);
      return m;
    }

// Peek
#pragma design modulario < in >
    Message Peek() {
      // this->read_reset_check.check();
#ifdef CONNECTIONS_SIM_ONLY
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif

      QUERY_CALL();
      while (!_VLDNAME_.read()) {
        wait();
      }

      Message m;
      read_msg(m);
      return m;
    }

    bool PeekNB(Message &data, const bool &/*unused*/ = true) {
      read_msg(data);
#ifdef __SYNTHESIS__      
      _RDYNAME_.write(false);
#endif
      return _VLDNAME_.read();
    }

// PopNB
#pragma builtin_modulario
#pragma design modulario < in >
    bool PopNB(Message &data, const bool &do_wait = true) {
      // this->read_reset_check.check();
#ifdef CONNECTIONS_SIM_ONLY
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif

      _RDYNAME_.write(true);
      if (do_wait) {
        wait();
        _RDYNAME_.write(false);
      }
      read_msg(data);
      return _VLDNAME_.read();
    }

  protected:
    virtual void read_msg(Message &m) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
  };


  template <typename Message>
  class InBlocking_SimPorts_abs : public InBlocking_Ports_abs<Message>
  {
  public:

    virtual ~InBlocking_SimPorts_abs() {
#ifdef __CONN_RAND_STALL_FEATURE
      if (!!post_pacer) delete post_pacer;
      post_pacer = NULL;
#endif
    }
    // Protected because generic
  protected:
    // Default constructor
    InBlocking_SimPorts_abs()
      : InBlocking_Ports_abs<Message>() {
#ifdef CONNECTIONS_SIM_ONLY
      Init_SIM(sc_gen_unique_name("in"));
#endif
    }

    // Constructor
    explicit InBlocking_SimPorts_abs(const char *name)
      : InBlocking_Ports_abs<Message>(name) {
#ifdef CONNECTIONS_SIM_ONLY
      Init_SIM(name);
#endif
    }

  public:
    // Reset read
    void Reset() {
#ifdef CONNECTIONS_SIM_ONLY
      this->read_reset_check.reset(this->non_leaf_port);
      Reset_SIM();
      get_conManager().add_clock_event(this);
#else
      InBlocking_Ports_abs<Message>::Reset();
#endif
    }

    bool do_reset_check() {
      return this->read_reset_check.check();
    }

#ifndef __SYNTHESIS__
    std::string report_name() {
      return this->read_reset_check.report_name();
    }
#endif

// Pop
#pragma design modulario < in >
    Message Pop() {
#ifdef CONNECTIONS_SIM_ONLY
      // this->read_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      return Pop_SIM();
#else
      return InBlocking_Ports_abs<Message>::Pop();
#endif
    }

// Peek
#pragma design modulario < in >
    Message Peek() {
      return InBlocking_Ports_abs<Message>::Peek();
    }

    bool PeekNB(Message &data, const bool &/*unused*/ = true) {
      return InBlocking_Ports_abs<Message>::PeekNB(data);
    }

// PopNB
#pragma design modulario < in >
    bool PopNB(Message &data, const bool &do_wait = true) {
#ifdef CONNECTIONS_SIM_ONLY
      // this->read_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      if (Empty_SIM()) {
        Message m;
        set_default_value(m);
        data = m;
        return false;
      } else {
        data = ConsumeBuf_SIM();
        return true;
      }
#else
      return InBlocking_Ports_abs<Message>::PopNB(data, do_wait);
#endif
    }

#ifdef CONNECTIONS_SIM_ONLY
    void disable_spawn() {
      get_conManager().remove(this);
      this->disable_spawn_true = 1;
    }


#ifdef __CONN_RAND_STALL_FEATURE
    void set_rand_stall_prob(float &newProb) {
      if (newProb > 0) {
        float tmpFloat = (newProb/100.0);
        post_pacer->set_stall_prob(tmpFloat);
      }
    }

    void set_rand_hold_stall_prob(float &newProb) {
      if (newProb > 0) {
        float tmpFloat = (newProb/100.0);
        post_pacer->set_hold_stall_prob(tmpFloat);
      }
    }


    /**
     * \brief Enable random stalling support on an input port.
     * \ingroup Connections
     *
     * Enable random stalling locally for an input port, valid in CONNECTIONS_ACCURATE_SIM
     * and CONNECTIONS_FAST_SIM modes. Random stalling is non engaged by default,
     * unless CONN_RAND_STALL is set.
     *
     * Random stalling is used to randomly stall In<> ports, creating random
     * back pressure in a design to assist catching latency-sentitive bugs.
     *
     * Can also enable globally with enable_global_rand_stall(). This command takes
     * precendence over the global setting.
     *
     * \par A Simple Example
     * \code
     *      #include <connections/connections.h>
     *
     *      int sc_main(int argc, char *argv[])
     *      {
     *      ...
     *      my_testbench.dut.my_input_port.enable_local_rand_stall();
     *      ...
     *      }
     * \endcode
     * \par
     *
     */
    void enable_local_rand_stall() {
      local_rand_stall_override = true;
      local_rand_stall_enable = true;
    }

    /**
     * \brief Disable random stalling support on an input port.
     * \ingroup Connections
     *
     * Disable random stalling locally for an input port, valid in CONNECTIONS_ACCURATE_SIM
     * and CONNECTIONS_FAST_SIM modes. Random stalling is non engaged by default,
     * unless CONN_RAND_STALL is set.
     *
     * Random stalling is used to randomly stall In<> ports, creating random
     * back pressure in a design to assist catching latency-sentitive bugs.
     *
     * Can also disable globally with disable_global_rand_stall(). This command takes
     * precendence over the global setting.
     *
     * \par A Simple Example
     * \code
     *      #include <connections/connections.h>
     *
     *      int sc_main(int argc, char *argv[])
     *      {
     *      ...
     *      enable_global_rand_stall();
     *      my_testbench.dut.my_input_port.disable_local_rand_stall();
     *      ...
     *      }
     * \endcode
     * \par
     *
     */
    void disable_local_rand_stall() {
      local_rand_stall_override = true;
      local_rand_stall_enable = false;
    }

    /**
     * \brief Remove a random stall setting on an input port.
     * \ingroup Connections
     *
     * Removes a random stall setting for an input port, valid in CONNECTIONS_ACCURATE_SIM
     * and CONNECTIONS_FAST_SIM modes. Effectively cancels enable_local_rand_stall() and
     * disable_local_rand_stall(), reverting to enable_global_rand_stall() or disable_global_rand_stall()
     * settings.
     *
     * \par A Simple Example
     * \code
     *      #include <connections/connections.h>
     *
     *      int sc_main(int argc, char *argv[])
     *      {
     *      ...
     *      // globally enable rand stalling
     *      enable_global_rand_stall();
     *      // locally disable rand stalling
     *      my_testbench.dut.my_input_port.disable_local_rand_stall();
     *      ...
     *      // cancel local rand stall directive, re-enabling global setting
     *      // on this port
     *      my_testbench.dut.my_input_port.cancel_local_rand_stall();
     *      }
     * \endcode
     * \par
     *
     */
    void cancel_local_rand_stall() {
      local_rand_stall_override = false;
    }


    /**
     * \brief Enable random stalling debug message support on an input port.
     * \ingroup Connections
     *
     * Enable random stalling debug messages locally for an input port. Random stalling
     * must already be enabled using CONN_RAND_STALL, enable_global_rand_stall(), or
     * enable_local_rand_stall().
     *
     * Can also enable globally with enable_global_rand_stall_print_debug(). This command takes
     * precendence over the global setting.
     *
     * \par A Simple Example
     * \code
     *      #include <connections/connections.h>
     *
     *      int sc_main(int argc, char *argv[])
     *      {
     *      ...
     *      my_testbench.dut.my_input_port.enable_local_rand_stall_print_debug();
     *      ...
     *      }
     * \endcode
     * \par
     *
     */
    void enable_local_rand_stall_print_debug() {
      local_rand_stall_print_debug_override = true;
      local_rand_stall_print_debug_enable = true;
    }

    /**
     * \brief Disable random stalling debug message support on an input port.
     * \ingroup Connections
     *
     * Disable random stalling debug message.
     *
     * Can also disable globally with disable_global_rand_stall_print_debug(). This command takes
     * precendence over the global setting.
     *
     * \par A Simple Example
     * \code
     *      #include <connections/connections.h>
     *
     *      int sc_main(int argc, char *argv[])
     *      {
     *      ...
     *      enable_global_rand_stall();
     *      enable_global_rand_stall_print_debug();
     *      my_testbench.dut.my_input_port.disable_local_rand_stall_print_debug();
     *      ...
     *      }
     * \endcode
     * \par
     *
     */
    void disable_local_rand_stall_print_debug() {
      local_rand_stall_print_debug_override = true;
      local_rand_stall_print_debug_enable = false;
    }

    /**
     * \brief Remove a random stall debug message setting on an input port.
     * \ingroup Connections
     *
     * Removes a random stall debug message setting for an input port.
     *  Effectively cancels enable_local_rand_stall_print_debug() and disable_local_rand_stall_print_debug(),
     * reverting to enable_global_rand_stall_print_debug() or disable_global_rand_stall_print_debug() settings.
     *
     * \par A Simple Example
     * \code
     *      #include <connections/connections.h>
     *
     *      int sc_main(int argc, char *argv[])
     *      {
     *      ...
     *      // globally enable rand stalling
     *      enable_global_rand_stall();
     *      enable_global_rand_stall_print_debug();
     *      // locally disable rand stalling
     *      my_testbench.dut.my_input_port.disable_local_rand_stall_print_debug();
     *      ...
     *      // cancel local rand stall directive, re-enabling global setting
     *      // on this port
     *      my_testbench.dut.my_input_port.cancel_local_rand_stall_print_debug();
     *      }
     * \endcode
     * \par
     *
     */
    void cancel_local_rand_stall_print_debug() {
      local_rand_stall_print_debug_override = false;
    }
#endif // __CONN_RAND_STALL_FEATURE

  protected:
    Message data_buf;
    bool data_val;
    bool rdy_set_by_api;
#ifdef __CONN_RAND_STALL_FEATURE
    Pacer *post_pacer;
    bool pacer_stall;
    bool local_rand_stall_override;
    bool local_rand_stall_enable;
    bool local_rand_stall_print_debug_override;
    bool local_rand_stall_print_debug_enable;
    unsigned long rand_stall_counter;
    sc_process_b *actual_process_b;
#endif

    std::string full_name() { return "InBlockingSimPorts_abs"; }

    void Init_SIM(const char *name) {
      data_val = false;
      rdy_set_by_api = false;
      get_conManager().add(this);
#ifdef __CONN_RAND_STALL_FEATURE
      double x = rand()%100;
      double y = rand()%100;
      post_pacer = new Pacer(x/100, y/100);
      pacer_stall = false;
      local_rand_stall_override = false;
      local_rand_stall_enable = false;
      local_rand_stall_print_debug_override = false;
      local_rand_stall_print_debug_enable = false;
      actual_process_b = 0;
#endif
    }

    void Reset_SIM() {
#ifdef __CONN_RAND_STALL_FEATURE
      actual_process_b = sc_core::sc_get_current_process_b();
#endif

      data_val = false;
    }

// Although this code is being used only for simulation now, it could be
// synthesized if pre/post are used instead of spawned threads.
// Keep mudulario pragmas here to preserve that option

#pragma design modulario < out >
    void receive(const bool &stall) {
      if (stall) {
        this->_RDYNAME_.write(false);
        rdy_set_by_api = false;
      } else {
        this->_RDYNAME_.write(true);
        rdy_set_by_api = true;
      }
    }

#pragma design modulario < in >
    bool received(Message &data) {
      if (this->_VLDNAME_.read()) {
        read_msg(data);
        return true;
      }

      // not valid
      // randomize data
      //memset(&data, random(), sizeof(data));
      //if (!data_val)
      //    memset(&data, 0, sizeof(data));
      return false;
    }

    bool Pre() {
#ifdef __CONN_RAND_STALL_FEATURE
      if ((local_rand_stall_override ? local_rand_stall_enable : get_rand_stall_enable()) && pacer_stall) {
        ++rand_stall_counter;
        return true;
      }
#endif
      if (rdy_set_by_api != this->_RDYNAME_.read()) {
        // something has changed the value of the signal not through API
        // killing spawned threads;
        return false;
      }
      if (!data_val) {
        if (received(data_buf)) {
          data_val = true;
        }
      }
      return true;
    }

    bool PrePostReset() {
      data_val = false;
      return true;
    }

#ifdef __CONN_RAND_STALL_FEATURE
    bool Post() {
      if ((local_rand_stall_override ? local_rand_stall_enable : get_rand_stall_enable())) {
        if (post_pacer->tic()) {
          if ((local_rand_stall_print_debug_override ? local_rand_stall_print_debug_enable : get_rand_stall_print_debug_enable()) && (!pacer_stall)) {
            std::string name = this->_VLDNAME_.name();
            std::string nameSuff = "_";
            nameSuff += _VLDNAMESTR_;
            unsigned int suffLen = nameSuff.length();
            if (name.substr(name.length() - suffLen,suffLen) == nameSuff) { name.erase(name.length() - suffLen,suffLen); }
            if (actual_process_b) {
              CONNECTIONS_COUT("Entering random stall on port " << name << " in thread '" << actual_process_b->basename() << "'." << endl);
            } else {
              CONNECTIONS_COUT("Entering random stall on port " << name << " in UNKNOWN thread (port needs to be Reset to register thread)." << endl);
            }
            rand_stall_counter = 0;
          }
          pacer_stall=true;
        } else {
          if ((local_rand_stall_print_debug_override ? local_rand_stall_print_debug_enable : get_rand_stall_print_debug_enable()) && (pacer_stall)) {
            std::string name = this->_VLDNAME_.name();
            std::string nameSuff = "_";
            nameSuff += _VLDNAMESTR_;
            unsigned int suffLen = nameSuff.length();
            if (name.substr(name.length() - suffLen,suffLen) == nameSuff) { name.erase(name.length() - suffLen,suffLen); }
            if (actual_process_b) {
              CONNECTIONS_COUT("Exiting random stall on port " << name << " in thread '" << actual_process_b->basename() << "'. Was stalled for " << rand_stall_counter << " cycles." << endl);
            } else {
              CONNECTIONS_COUT("Exiting random stall on port " << name << " in thread UNKNOWN thread (port needs to be Reset to register thread). Was stalled for " << rand_stall_counter << " cycles." << endl);
            }
          }
          pacer_stall=false;
        }
      } else {
        pacer_stall = false;
      }
      receive(data_val || pacer_stall);
      return true;
    }
#else
    bool Post() {
      receive(data_val);
      return true;
    }
#endif

    bool Empty_SIM() { return !data_val; }

    Message &ConsumeBuf_SIM() {
      CONNECTIONS_ASSERT_MSG(data_val, "Unreachable state, asked to consume but data isn't valid!");
      data_val = false;
      return data_buf;
    }

    Message &Pop_SIM() {
      while (Empty_SIM()) {
        wait();
      }
      return ConsumeBuf_SIM();
    }
#endif // CONNECTIONS_SIM_ONLY

  protected:
    virtual void read_msg(Message &m) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
  };

//------------------------------------------------------------------------
// In
//------------------------------------------------------------------------
  /**
   * \brief Connections IN port
   * \ingroup Connections
   *
   * \tparam Message      DataType of message
   *
   * \par A Simple Example
   * \code
   *      #include <connections/connections.h>
   *
   *      ...
   *      Connections::In< T > in;
   *      ...
   *      in.Reset();
   *      wait();
   *      ...
   *      T msg = in.Pop(); // Blocking Pop from channel
   *      ...
   *      T reg;
   *      if (in.PopNB(reg)) { // Non-Blocking Pop from channel
   *      ...
   *      }
   * \endcode
   * \par
   *
   */
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Specializations of In port for marshall vs direct port
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename Message>
  class InBlocking <Message, SYN_PORT> : public InBlocking_Ports_abs<Message>
  {
  public:
    // Interface
    typedef Wrapped<Message> WMessage;
    static const unsigned int width = WMessage::width;
    typedef sc_lv<WMessage::width> MsgBits;
    sc_in<MsgBits> _DATNAME_;

    InBlocking() : InBlocking_Ports_abs<Message>(),
      _DATNAME_(sc_gen_unique_name(_DATNAMEINSTR_)) {}

    explicit InBlocking(const char *name) : InBlocking_Ports_abs<Message>(name),
      _DATNAME_(CONNECTIONS_CONCAT(name, _DATNAMESTR_)) {}

    virtual ~InBlocking() {}

// Pop
#pragma builtin_modulario
#pragma design modulario < in >
    Message Pop() {
      return InBlocking_Ports_abs<Message>::Pop();
    }

// Peek
#pragma design modulario < in >
    Message Peek() {
      return InBlocking_Ports_abs<Message>::Peek();
    }

#pragma builtin_modulario
#pragma design modulario < peek >
    bool PeekNB(Message &data, const bool &/*unused*/ = true) {
      return InBlocking_Ports_abs<Message>::PeekNB(data);
    }
    
// PopNB
#pragma builtin_modulario
#pragma design modulario < in >
    bool PopNB(Message &data, const bool &do_wait = true) {
      return InBlocking_Ports_abs<Message>::PopNB(data, do_wait);
    }

    // Bind to InBlocking
    void Bind(InBlocking<Message, SYN_PORT> &rhs) {
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    // Bind to Combinational
    void Bind(Combinational<Message, SYN_PORT> &rhs) {
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    // Bind to InBlocking
    void Bind(InBlocking<Message, MARSHALL_PORT> &rhs) {
#ifdef CONNECTIONS_SIM_ONLY
      rhs.disable_spawn();
      rhs.non_leaf_port = true;
#endif
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    // Bind to Combinational
    void Bind(Combinational<Message, MARSHALL_PORT> &rhs) {
#ifdef CONNECTIONS_SIM_ONLY
      this->_DATNAME_(rhs._DATNAMEOUT_);
      this->_VLDNAME_(rhs._VLDNAMEOUT_);
      this->_RDYNAME_(rhs._RDYNAMEOUT_);
      rhs.out_bound = true;
      rhs.out_ptr = this;
#else
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
#endif
    }

// Be safe: disallow DIRECT_PORT <-> SYN_PORT binding during HLS
#ifndef __SYNTHESIS__
    void Bind(InBlocking<Message, DIRECT_PORT> &rhs) {
      DirectToMarshalledInPort<Message> *dynamic_d2mport;

      dynamic_d2mport = new DirectToMarshalledInPort<Message>(sc_gen_unique_name("dynamic_d2mport"));
      this->sc_mod_alloc.push_back(dynamic_d2mport);
      dynamic_d2mport->_DATNAME_(rhs._DATNAME_);
      this->_DATNAME_(dynamic_d2mport->msgbits);

#ifdef CONNECTIONS_SIM_ONLY
      dynamic_d2mport->sibling_port = this; 
      rhs.disable_spawn();
      rhs.non_leaf_port = true;
#endif
      this->_DATNAME_(dynamic_d2mport->msgbits);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    void Bind(Combinational<Message, DIRECT_PORT> &rhs) {
      DirectToMarshalledInPort<Message> *dynamic_d2mport;

      dynamic_d2mport = new DirectToMarshalledInPort<Message>(sc_gen_unique_name("dynamic_d2mport"));
      this->sc_mod_alloc.push_back(dynamic_d2mport);
      this->_DATNAME_(dynamic_d2mport->msgbits);

#ifdef CONNECTIONS_SIM_ONLY
      dynamic_d2mport->sibling_port = this; 
      dynamic_d2mport->_DATNAME_(rhs._DATNAMEOUT_);
      this->_VLDNAME_(rhs._VLDNAMEOUT_);
      this->_RDYNAME_(rhs._RDYNAMEOUT_);
      rhs.out_bound = true;
      rhs.out_ptr = this;
#else
      dynamic_d2mport->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
#endif
    }

#ifdef CONNECTIONS_SIM_ONLY
    void Bind(Combinational<Message, TLM_PORT> &rhs) {
      TLMToDirectOutPort<Message> *dynamic_tlm2d_port;
      Combinational<Message, DIRECT_PORT> *dynamic_comb;

      dynamic_tlm2d_port = new TLMToDirectOutPort<Message>(sc_gen_unique_name("dynamic_tlm2d_port"), rhs.fifo);
      dynamic_tlm2d_port->sibling_port = this;
      dynamic_comb = new Combinational<Message, DIRECT_PORT>(sc_gen_unique_name("dynamic_comb"));
      this->con_obj_alloc.push_back(dynamic_tlm2d_port);
      this->con_obj_alloc.push_back(dynamic_comb);

      // Bind the marshaller to combinational
      this->Bind(*dynamic_comb);
      dynamic_tlm2d_port->_VLDNAME_(dynamic_comb->_VLDNAMEIN_);
      dynamic_tlm2d_port->_RDYNAME_(dynamic_comb->_RDYNAMEIN_);
      dynamic_tlm2d_port->_DATNAME_(dynamic_comb->_DATNAMEIN_);

      // Bound but no ptr indicates a cosim interface
      dynamic_comb->in_bound = true;
      dynamic_comb->in_ptr = 0;
    }
#endif // CONNECTIONS_SIM_ONLY

#endif // __SYNTHESIS__

#ifdef __CCS_P2P_H
    // Bind to p2p<>::in
    void Bind(p2p<SYN>::in<Message> &rhs) {
      this->_DATNAME_(rhs.i_dat);
      this->_VLDNAME_(rhs.i_vld);
      this->_RDYNAME_(rhs.o_rdy);
    }

    // Bind to p2p<>::chan
    void Bind(p2p<SYN>::chan<Message> &rhs) {
      this->_DATNAME_(rhs.dat);
      this->_VLDNAME_(rhs.vld);
      this->_RDYNAME_(rhs.rdy);
    }
#endif

    // Binding
    template <typename C>
    void operator()(C &rhs) {
      Bind(rhs);
    }

  protected:
    void read_msg(Message &m) {
      MsgBits mbits = _DATNAME_.read();
      Marshaller<WMessage::width> marshaller(mbits);
      WMessage result;
      result.Marshall(marshaller);
      m = result.val;
    }
  };


  template <typename Message>
  class InBlocking <Message, MARSHALL_PORT> : public InBlocking_SimPorts_abs<Message>
  {
  public:
    // Interface
    typedef Wrapped<Message> WMessage;
    static const unsigned int width = WMessage::width;
    typedef sc_lv<WMessage::width> MsgBits;
    sc_in<MsgBits> _DATNAME_;

#ifdef CONNECTIONS_SIM_ONLY
    in_port_marker marker;
#endif

    InBlocking() : InBlocking_SimPorts_abs<Message>(),
      _DATNAME_(sc_gen_unique_name(_DATNAMEINSTR_)) {}

    explicit InBlocking(const char *name) :
      InBlocking_SimPorts_abs<Message>(name)
      , _DATNAME_(CONNECTIONS_CONCAT(name, _DATNAMESTR_))
#ifdef CONNECTIONS_SIM_ONLY
      , marker(CONNECTIONS_CONCAT(name, "in_port_marker"), width, &(this->_VLDNAME_), &(this->_RDYNAME_), &_DATNAME_)
#endif
    {}

    virtual ~InBlocking() {}

    // Reset read
    void Reset() {
      InBlocking_SimPorts_abs<Message>::Reset();
    }

    // Pop
#pragma builtin_modulario
#pragma design modulario < in >
    Message Pop() {
      return InBlocking_SimPorts_abs<Message>::Pop();
    }

// Peek
#pragma design modulario < in >
    Message Peek() {
      return InBlocking_SimPorts_abs<Message>::Peek();
    }

#pragma builtin_modulario
#pragma design modulario < peek >    
    bool PeekNB(Message &data, const bool &/*unused*/ = true) {
      return InBlocking_SimPorts_abs<Message>::PeekNB(data);
    }
    
// PopNB
#pragma builtin_modulario
#pragma design modulario < in >
    bool PopNB(Message &data, const bool &do_wait = true) {
      return InBlocking_SimPorts_abs<Message>::PopNB(data, do_wait);
    }

    // Bind to InBlocking
    void Bind(InBlocking<Message, MARSHALL_PORT> &rhs) {
#ifdef CONNECTIONS_SIM_ONLY
      rhs.disable_spawn();
      rhs.non_leaf_port = true;
#endif
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    // Bind to Combinational
    void Bind(Combinational<Message, MARSHALL_PORT> &rhs) {
#ifdef CONNECTIONS_SIM_ONLY
      this->_DATNAME_(rhs._DATNAMEOUT_);
      this->_VLDNAME_(rhs._VLDNAMEOUT_);
      this->_RDYNAME_(rhs._RDYNAMEOUT_);
      rhs.out_bound = true;
      rhs.out_ptr = this;
      marker.top_port = true;
#else
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
#endif
    }

    // Bind to InBlocking
    void Bind(InBlocking<Message, SYN_PORT> &rhs) {
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    // Bind to Combinational
    void Bind(Combinational<Message, SYN_PORT> &rhs) {
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

// Be safe: disallow DIRECT_PORT <-> MARSHALL_PORT binding during HLS
#ifndef __SYNTHESIS__
    void Bind(InBlocking<Message, DIRECT_PORT> &rhs) {
      DirectToMarshalledInPort<Message> *dynamic_d2mport;

      dynamic_d2mport = new DirectToMarshalledInPort<Message>(sc_gen_unique_name("dynamic_d2mport"));
      this->sc_mod_alloc.push_back(dynamic_d2mport);
      dynamic_d2mport->_DATNAME_(rhs._DATNAME_);
      this->_DATNAME_(dynamic_d2mport->msgbits);

#ifdef CONNECTIONS_SIM_ONLY
      dynamic_d2mport->sibling_port = this; 
      rhs.disable_spawn();
      rhs.non_leaf_port = true;
#endif
      this->_DATNAME_(dynamic_d2mport->msgbits);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    void Bind(Combinational<Message, DIRECT_PORT> &rhs) {
      DirectToMarshalledInPort<Message> *dynamic_d2mport;

      dynamic_d2mport = new DirectToMarshalledInPort<Message>(sc_gen_unique_name("dynamic_d2mport"));
      this->sc_mod_alloc.push_back(dynamic_d2mport);
      this->_DATNAME_(dynamic_d2mport->msgbits);

#ifdef CONNECTIONS_SIM_ONLY
      dynamic_d2mport->sibling_port = this; 
      dynamic_d2mport->_DATNAME_(rhs._DATNAMEOUT_);
      this->_VLDNAME_(rhs._VLDNAMEOUT_);
      this->_RDYNAME_(rhs._RDYNAMEOUT_);
      rhs.out_bound = true;
      rhs.out_ptr = this;
#else
      dynamic_d2mport->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
#endif
    }

#ifdef CONNECTIONS_SIM_ONLY
    void Bind(Combinational<Message, TLM_PORT> &rhs) {
      TLMToDirectOutPort<Message> *dynamic_tlm2d_port;
      Combinational<Message, DIRECT_PORT> *dynamic_comb;

      dynamic_tlm2d_port = new TLMToDirectOutPort<Message>(sc_gen_unique_name("dynamic_tlm2d_port"), rhs.fifo);
      dynamic_tlm2d_port->sibling_port = this;
      dynamic_comb = new Combinational<Message, DIRECT_PORT>(sc_gen_unique_name("dynamic_comb"));
      this->con_obj_alloc.push_back(dynamic_tlm2d_port);
      this->con_obj_alloc.push_back(dynamic_comb);

      // Bind the marshaller to combinational
      this->Bind(*dynamic_comb);
      dynamic_tlm2d_port->_VLDNAME_(dynamic_comb->_VLDNAMEIN_);
      dynamic_tlm2d_port->_RDYNAME_(dynamic_comb->_RDYNAMEIN_);
      dynamic_tlm2d_port->_DATNAME_(dynamic_comb->_DATNAMEIN_);

      // Bound but no ptr indicates a cosim interface
      dynamic_comb->in_bound = true;
      dynamic_comb->in_ptr = 0;
    }
#endif // CONNECTIONS_SIM_ONLY

#endif // __SYNTHESIS__

#ifdef __CCS_P2P_H
    // Bind to p2p<>::in
    void Bind(p2p<SYN>::in<Message> &rhs) {
      this->_DATNAME_(rhs.i_dat);
      this->_VLDNAME_(rhs.i_vld);
      this->_RDYNAME_(rhs.o_rdy);
    }

    // Bind to p2p<>::chan
    void Bind(p2p<SYN>::chan<Message> &rhs) {
      this->_DATNAME_(rhs.dat);
      this->_VLDNAME_(rhs.vld);
      this->_RDYNAME_(rhs.rdy);
    }
#endif

    // Binding
    template <typename C>
    void operator()(C &rhs) {
      Bind(rhs);
    }

  protected:
    void read_msg(Message &m) {
      MsgBits mbits = _DATNAME_.read();
      Marshaller<WMessage::width> marshaller(mbits);
      WMessage result;
      result.Marshall(marshaller);
      m = result.val;
    }
  };

  template <typename Message>
  class InBlocking <Message, DIRECT_PORT> : public InBlocking_SimPorts_abs<Message>
  {
  public:
    // Interface
    sc_in<Message> _DATNAME_;

    InBlocking() : InBlocking_SimPorts_abs<Message>(),
      _DATNAME_(sc_gen_unique_name(_DATNAMEINSTR_)) {}

    explicit InBlocking(const char *name) : InBlocking_SimPorts_abs<Message>(name),
      _DATNAME_(CONNECTIONS_CONCAT(name, _DATNAMESTR_)) {}

    virtual ~InBlocking() {}

    // Reset read
    void Reset() {
      InBlocking_SimPorts_abs<Message>::Reset();
    }

    // Pop
#pragma builtin_modulario
#pragma design modulario < in >
    Message Pop() {
      return InBlocking_SimPorts_abs<Message>::Pop();
    }

    // Peek
#pragma design modulario < in >
    Message Peek() {
      return InBlocking_SimPorts_abs<Message>::Peek();
    }

#pragma builtin_modulario
#pragma design modulario < peek >
    bool PeekNB(Message &data, const bool &/*unused*/ = true) {
        return InBlocking_SimPorts_abs<Message>::PeekNB(data);
    }    

    // PopNB
#pragma builtin_modulario
#pragma design modulario < in >
    bool PopNB(Message &data, const bool &do_wait = true) {
      return InBlocking_SimPorts_abs<Message>::PopNB(data, do_wait);
    }

    // Bind to InBlocking
    void Bind(InBlocking<Message, DIRECT_PORT> &rhs) {
#ifdef CONNECTIONS_SIM_ONLY
      rhs.disable_spawn();
      rhs.non_leaf_port = true;
#endif
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    // Bind to Combinational
    void Bind(Combinational<Message, DIRECT_PORT> &rhs) {
#ifdef CONNECTIONS_SIM_ONLY
      this->_DATNAME_(rhs._DATNAMEOUT_);
      this->_VLDNAME_(rhs._VLDNAMEOUT_);
      this->_RDYNAME_(rhs._RDYNAMEOUT_);
      rhs.out_bound = true;
      rhs.out_ptr = this;
#else
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
#endif
    }

    // Binding
    template <typename C>
    void operator()(C &rhs) {
      Bind(rhs);
    }

  protected:
    void read_msg(Message &m) {
      m = _DATNAME_.read();
    }
  };


#ifdef CONNECTIONS_SIM_ONLY

  template <typename Message>
  class InBlocking <Message, TLM_PORT> : public InBlocking_abs<Message>
  {
  public:
    // Default constructor
    InBlocking() : InBlocking_abs<Message>(),
      i_fifo(sc_gen_unique_name("i_fifo")) {
#ifdef __CONN_RAND_STALL_FEATURE
      Init_SIM(sc_gen_unique_name("in"));
#endif
    }

    explicit InBlocking(const char *name) : InBlocking_abs<Message>(name),
      i_fifo(CONNECTIONS_CONCAT(name, "i_fifo")) {
#ifdef __CONN_RAND_STALL_FEATURE
      Init_SIM(name);
#endif
    }

    virtual ~InBlocking() {
#ifdef __CONN_RAND_STALL_FEATURE
      if (!!post_pacer) delete post_pacer;
      post_pacer = NULL;
#endif
    }

    // Reset read
    void Reset() {
      this->read_reset_check.reset(this->non_leaf_port);
      get_conManager().add_clock_event(this);
      Message temp;
      while (i_fifo->nb_get(temp));
    }

    bool do_reset_check() {
      return this->read_reset_check.check();
    }

#ifndef __SYNTHESIS__
    std::string report_name() {
      return this->read_reset_check.report_name();
    }
#endif

// Pop
#pragma design modulario < in >
    Message Pop() {
      // this->read_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
#ifdef __CONN_RAND_STALL_FEATURE
      while ((local_rand_stall_override ? local_rand_stall_enable : get_rand_stall_enable()) && post_pacer->tic()) { wait(); }
#endif
      return i_fifo->get();
    }

// Peek
#pragma design modulario < in >
    Message Peek() {
      // this->read_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      return i_fifo->peek();
    }

    bool PeekNB(Message &data, const bool &/*unused*/ = true) {
      assert(0);
      return false; // i_fifo-> tlm::tlm_get_peek_if<Message> ::nb_peek(data);
    }

// PopNB
#pragma design modulario < in >
    bool PopNB(Message &data, const bool &do_wait = true) {
      // this->read_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
#ifdef __CONN_RAND_STALL_FEATURE
      if ((local_rand_stall_override ? local_rand_stall_enable : get_rand_stall_enable()) && post_pacer->tic()) { return false; }
#endif
      return i_fifo->nb_get(data);
    }

    // Bind to InBlocking
    void Bind(InBlocking<Message, TLM_PORT> &rhs) {
      this->i_fifo(rhs.i_fifo);
    }

    // Bind to Combinational
    void Bind(Combinational<Message, TLM_PORT> &rhs) {
      this->i_fifo(rhs.fifo);
    }

    template <typename C>
    void operator()(C &rhs) {
      Bind(rhs);
    }

#ifdef __CONN_RAND_STALL_FEATURE
    void enable_local_rand_stall() {
      local_rand_stall_override = true;
      local_rand_stall_enable = true;
    }

    void disable_local_rand_stall() {
      local_rand_stall_override = true;
      local_rand_stall_enable = false;
    }

    void cancel_local_rand_stall() {
      local_rand_stall_override = false;
    }

    void enable_local_rand_stall_print_debug() {
      local_rand_stall_print_debug_override = true;
      local_rand_stall_print_debug_enable = true;
    }

    void disable_local_rand_stall_print_debug() {
      local_rand_stall_print_debug_override = true;
      local_rand_stall_print_debug_enable = false;
    }

    void cancel_local_rand_stall_print_debug() {
      local_rand_stall_print_debug_override = false;
    }
#endif // __CONN_RAND_STALL_FEATURE

  protected:
    sc_port<tlm::tlm_fifo_get_if<Message> > i_fifo;

#ifdef __CONN_RAND_STALL_FEATURE
    Pacer *post_pacer;
    bool local_rand_stall_override;
    bool local_rand_stall_enable;
    bool local_rand_stall_print_debug_override;
    bool local_rand_stall_print_debug_enable;

    void Init_SIM(const char *name) {
      double x = rand()%100;
      double y = rand()%100;
      post_pacer = new Pacer(x/100, y/100);
      local_rand_stall_override = false;
      local_rand_stall_enable = false;
      local_rand_stall_print_debug_override = false;
      local_rand_stall_print_debug_enable = false;
    }
#endif

  protected:
    void read_msg(Message &m) {
      m = i_fifo->peek();
    }
  };

#endif

//------------------------------------------------------------------------
// In
//------------------------------------------------------------------------

  template <typename Message>
  class In<Message, SYN_PORT> : public InBlocking<Message, SYN_PORT>
  {
  public:
    In() {}

    explicit In(const char *name) : InBlocking<Message, SYN_PORT>(name) {}

    virtual ~In() {}

    // Empty
    bool Empty() {
      QUERY_CALL();
      return !this->_VLDNAME_.read();
    }
  };

  template <typename Message>
  class In<Message, MARSHALL_PORT> : public InBlocking<Message, MARSHALL_PORT>
  {
  public:
    In() {}

    explicit In(const char *name) : InBlocking<Message, MARSHALL_PORT>(name) {}

    virtual ~In() {}

    // Empty
    bool Empty() {
#ifdef CONNECTIONS_SIM_ONLY
      return this->Empty_SIM();
#else
      QUERY_CALL();
      return !this->_VLDNAME_.read();
#endif
    }
  };

  template <typename Message>
  class In<Message, DIRECT_PORT> : public InBlocking<Message, DIRECT_PORT>
  {
  public:
    In() {}

    explicit In(const char *name) : InBlocking<Message, DIRECT_PORT>(name) {}

    virtual ~In() {}

    // Empty
    bool Empty() {
#ifdef CONNECTIONS_SIM_ONLY
      return this->Empty_SIM();
#else
      QUERY_CALL();
      return !this->_VLDNAME_.read();
#endif
    }
  };

#ifdef CONNECTIONS_SIM_ONLY
  template <typename Message>
  class In<Message, TLM_PORT> : public InBlocking<Message, TLM_PORT>
  {
  public:
    In() {}

    explicit In(const char *name) : InBlocking<Message, TLM_PORT>(name) {}

    virtual ~In() {}

    // Empty
    bool Empty() {
      return ! this->i_fifo->nb_can_get();
    }
  };
#endif


//------------------------------------------------------------------------
// OutBlocking_abs
//------------------------------------------------------------------------

  template <typename Message>
#ifdef CONNECTIONS_SIM_ONLY
  class OutBlocking_abs : public Blocking_abs, public CollectAllocs
#else
  class OutBlocking_abs : public CollectAllocs
#endif
  {
  public:

    // Protected because abstract class
  protected:

    ResetChecker write_reset_check;

    // Default constructor
    OutBlocking_abs()
      :
#ifdef CONNECTIONS_SIM_ONLY
      Blocking_abs(),
#endif
      write_reset_check("unnamed_out") {}

    // Constructor
    explicit OutBlocking_abs(const char *name)
      :
#ifdef CONNECTIONS_SIM_ONLY
      Blocking_abs(),
#endif
      write_reset_check(name) {}

  public:
    virtual ~OutBlocking_abs() {}

    // Reset write
    void Reset() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }

// Push
#pragma design modulario < out >
    void Push(const Message &m) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }

// PushNB
#pragma design modulario < out >
    bool PushNB(const Message &m, const bool &do_wait = true) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
      return false;
    }
  };



  template <typename Message>
  class OutBlocking_Ports_abs : public OutBlocking_abs<Message>
  {
  public:
    // Interface
    sc_out<bool> _VLDNAME_;
    sc_in<bool> _RDYNAME_;

    // Protected because abstract class
  protected:
    // Default constructor
    OutBlocking_Ports_abs()
      : OutBlocking_abs<Message>(),
        _VLDNAME_(sc_gen_unique_name(_VLDNAMEOUTSTR_)),
        _RDYNAME_(sc_gen_unique_name(_RDYNAMEOUTSTR_)) {
#ifndef __SYNTHESIS__
      this->write_reset_check.set_val_name(_VLDNAME_.name());
#endif
    }

    // Constructor
    explicit OutBlocking_Ports_abs(const char *name)
      : OutBlocking_abs<Message>(name),
        _VLDNAME_(CONNECTIONS_CONCAT(name, _VLDNAMESTR_)),
        _RDYNAME_(CONNECTIONS_CONCAT(name, _RDYNAMESTR_)) {
#ifndef __SYNTHESIS__
      this->write_reset_check.set_val_name(_VLDNAME_.name());
#endif
    }

  public:
    virtual ~OutBlocking_Ports_abs() {} 

    // Reset write
    void Reset() {
#ifdef CONNECTIONS_SIM_ONLY
      this->write_reset_check.reset(this->non_leaf_port);
#else
      this->write_reset_check.reset(false);
#endif
      _VLDNAME_.write(false);
      reset_msg();
#ifdef CONNECTIONS_SIM_ONLY
      get_conManager().add_clock_event(this);
#endif
    }

    bool do_reset_check() {
      return this->write_reset_check.check();
    }

#ifndef __SYNTHESIS__
    std::string report_name() {
      return this->write_reset_check.report_name();
    }
#endif

// Push
#pragma builtin_modulario
#pragma design modulario < out >
    void Push(const Message &m) {
      // this->write_reset_check.check();
#ifdef CONNECTIONS_SIM_ONLY
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      do {
        _VLDNAME_.write(true);
        write_msg(m);
        wait();
      } while (_RDYNAME_.read() != true);
      _VLDNAME_.write(false);
    }

// PushNB
#pragma builtin_modulario
#pragma design modulario < out >
    bool PushNB(const Message &m, const bool &do_wait = true) {
      // this->write_reset_check.check();
#ifdef CONNECTIONS_SIM_ONLY
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      _VLDNAME_.write(true);
      write_msg(m);
      wait();
      _VLDNAME_.write(false);
      invalidate_msg();
      return _RDYNAME_.read();
    }

  protected:
    virtual void reset_msg() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
    virtual void write_msg(const Message &m) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
    virtual void invalidate_msg() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
  };


  template <typename Message>
  class OutBlocking_SimPorts_abs : public OutBlocking_Ports_abs<Message>
  {
  public:

    // Protected because abstract class
  protected:
    // Default constructor
    OutBlocking_SimPorts_abs()
      : OutBlocking_Ports_abs<Message>() {
#ifdef CONNECTIONS_SIM_ONLY
      Init_SIM(sc_gen_unique_name("out"));
#endif
    }

    // Constructor
    explicit OutBlocking_SimPorts_abs(const char *name)
      : OutBlocking_Ports_abs<Message>(name) {
#ifdef CONNECTIONS_SIM_ONLY
      Init_SIM(name);
#endif
    }

  public:
    virtual ~OutBlocking_SimPorts_abs() {}

    // Reset write
    void Reset() {
#ifdef CONNECTIONS_SIM_ONLY
      this->write_reset_check.reset(this->non_leaf_port);
      Reset_SIM();
      get_conManager().add_clock_event(this);
#else
      OutBlocking_Ports_abs<Message>::Reset();
#endif
    }

    bool do_reset_check() {
      return this->write_reset_check.check();
    }

#ifndef __SYNTHESIS__
    std::string report_name() {
      return this->write_reset_check.report_name();
    }
#endif

    // Push
#pragma design modulario < out >
    void Push(const Message &m) {
#ifdef CONNECTIONS_SIM_ONLY
      // this->write_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      return Push_SIM(m);
#else
      OutBlocking_Ports_abs<Message>::Push(m);
#endif
    }

// PushNB
#pragma design modulario < out >
    bool PushNB(const Message &m, const bool &do_wait = true) {
#ifdef CONNECTIONS_SIM_ONLY
      // this->write_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      if (Full_SIM()) {
        return false;
      } else {
        FillBuf_SIM(m);
        return true;
      }
#else
      return OutBlocking_Ports_abs<Message>::PushNB(m, do_wait);
#endif
    }

#ifdef CONNECTIONS_SIM_ONLY
    void disable_spawn() {
      get_conManager().remove(this);
      this->disable_spawn_true = 1;
    }

  protected:
    bool data_val;
    Message data_buf;
    bool val_set_by_api;

    void Init_SIM(const char *name) {
      data_val = false;
      val_set_by_api = false;
      get_conManager().add(this);
    }

    std::string full_name() { return "Out_Blocking_SimPorts_abs"; }

    void Reset_SIM() {
      this->reset_msg();
      data_val = false;
    }

// Although this code is being used only for simulation now, it could be
// synthesized if pre/post are used instead of spawned threads.
// Keep mudulario pragmas here to preserve that option

#pragma design modulario < in >
    bool transmitted() { return this->_RDYNAME_.read(); }

#pragma design modulario < out >
    void transmit_data(const Message &m) {
      this->write_msg(m);
    }

#pragma design modulario < out >
    void transmit_val(const bool &vald) {
      if (vald) {
        this->_VLDNAME_.write(true);
        val_set_by_api = true;
      } else {
        this->_VLDNAME_.write(false);
        val_set_by_api = false;

        //corrupt and transmit data
        //memset(&data_buf, random(), sizeof(data_buf));
        //transmit_data(data_buf);
      }
    }

    bool Pre() {
      if (data_val) {
        if (transmitted()) {
          data_val = false;
        }
      }
      return true;
    }

    bool PrePostReset() {
      data_val = false;
      return true;
    }

    bool Post() {
      if (val_set_by_api != this->_VLDNAME_.read()) {
        // something has changed the value of the signal not through API
        // killing spawned threads;
        return false;
      }
      transmit_val(data_val);
      return true;
    }

    void FillBuf_SIM(const Message &m) {
      CONNECTIONS_ASSERT_MSG(!data_val, "Unreachable state, asked to fill buffer but buffer already full!");
      data_val = true;
      transmit_data(m);
      data_buf = m;
    }

    bool Empty_SIM() { return !data_val; }

    bool Full_SIM() { return data_val; }

    void Push_SIM(const Message &m) {
      while (Full_SIM()) {
        wait();
      }
      FillBuf_SIM(m);
    }
#endif //CONNECTIONS_SIM_ONLY
  };



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Specializations of Out port for marshall vs direct port
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  template <typename Message>
  class OutBlocking <Message, SYN_PORT> : public OutBlocking_Ports_abs<Message>
  {
  public:
    // Interface
    typedef Wrapped<Message> WMessage;
    static const unsigned int width = WMessage::width;
    typedef sc_lv<WMessage::width> MsgBits;
    sc_out<MsgBits> _DATNAME_;


    OutBlocking() : OutBlocking_Ports_abs<Message>(),
      _DATNAME_(sc_gen_unique_name(_DATNAMEOUTSTR_)) {}

    explicit OutBlocking(const char *name) :
      OutBlocking_Ports_abs<Message>(name)
      , _DATNAME_(CONNECTIONS_CONCAT(name, _DATNAMESTR_)) {}

    virtual ~OutBlocking() {}

    // Reset write
    virtual void Reset() {
      OutBlocking_Ports_abs<Message>::Reset();
    }

    // Push
#pragma builtin_modulario
#pragma design modulario < out >
    void Push(const Message &m) {
      OutBlocking_Ports_abs<Message>::Push(m);
    }

    // PushNB
#pragma builtin_modulario
#pragma design modulario < out >
    bool PushNB(const Message &m, const bool &do_wait = true) {
      return OutBlocking_Ports_abs<Message>::PushNB(m,do_wait);
    }

    // Bind to OutBlocking
    void Bind(OutBlocking<Message, SYN_PORT> &rhs) {
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    // Bind to Combinational Marshall Port
    void Bind(Combinational<Message, SYN_PORT> &rhs) {
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    // Bind to OutBlocking
    void Bind(OutBlocking<Message, MARSHALL_PORT> &rhs) {
#ifdef CONNECTIONS_SIM_ONLY
      rhs.disable_spawn();
      rhs.non_leaf_port = true;
#endif
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    // Bind to Combinational Marshall Port
    void Bind(Combinational<Message, MARSHALL_PORT> &rhs) {
#ifdef CONNECTIONS_SIM_ONLY
      this->_DATNAME_(rhs._DATNAMEIN_);
      this->_VLDNAME_(rhs._VLDNAMEIN_);
      this->_RDYNAME_(rhs._RDYNAMEIN_);
      rhs.in_bound = true;
      rhs.in_ptr = this;
#else
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
#endif
    }

    // For safety disallow DIRECT_PORT <-> SYN_PORT binding during HLS
#ifndef __SYNTHESIS__
    void Bind(OutBlocking<Message, DIRECT_PORT> &rhs) {
      MarshalledToDirectOutPort<Message> *dynamic_m2dport;
      dynamic_m2dport = new MarshalledToDirectOutPort<Message>(sc_gen_unique_name("dynamic_m2dport"));
      this->sc_mod_alloc.push_back(dynamic_m2dport);

#ifdef CONNECTIONS_SIM_ONLY
      dynamic_m2dport->sibling_port = this; 
      rhs.disable_spawn();
      rhs.non_leaf_port = true;
#endif

      this->_DATNAME_(dynamic_m2dport->msgbits);
      dynamic_m2dport->_DATNAME_(rhs._DATNAME_);
      dynamic_m2dport->_VLDNAME_(rhs._VLDNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    void Bind(Combinational<Message, DIRECT_PORT> &rhs) {
      MarshalledToDirectOutPort<Message> *dynamic_m2dport;

      dynamic_m2dport = new MarshalledToDirectOutPort<Message>(sc_gen_unique_name("dynamic_m2dport"));
      this->sc_mod_alloc.push_back(dynamic_m2dport);
      this->_DATNAME_(dynamic_m2dport->msgbits);

#ifdef CONNECTIONS_SIM_ONLY
      dynamic_m2dport->sibling_port = this; 
      dynamic_m2dport->_DATNAME_(rhs._DATNAMEIN_);
      dynamic_m2dport->_VLDNAME_(rhs._VLDNAMEIN_);
      this->_VLDNAME_(rhs._VLDNAMEIN_);
      this->_RDYNAME_(rhs._RDYNAMEIN_);
      rhs.in_bound = true;
      rhs.in_ptr = this;
#else
      dynamic_m2dport->_DATNAME_(rhs._DATNAME_);
      dynamic_m2dport->_VLDNAME_(rhs._VLDNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
#endif
    }

#ifdef CONNECTIONS_SIM_ONLY
    void Bind(Combinational<Message, TLM_PORT> &rhs) {
      DirectToTLMInPort<Message> *dynamic_d2tlm_port;
      Combinational<Message, DIRECT_PORT> *dynamic_comb;

      dynamic_d2tlm_port = new DirectToTLMInPort<Message>(sc_gen_unique_name("dynamic_d2tlm_port"), rhs.fifo);
      dynamic_d2tlm_port->sibling_port = this;
      dynamic_comb = new Combinational<Message, DIRECT_PORT>(sc_gen_unique_name("dynamic_comb"));
      this->con_obj_alloc.push_back(dynamic_d2tlm_port);
      this->con_obj_alloc.push_back(dynamic_comb);

      // Bind the marshaller to combinational
      this->Bind(*dynamic_comb);
      dynamic_d2tlm_port->_VLDNAME_(dynamic_comb->_VLDNAMEOUT_);
      dynamic_d2tlm_port->_RDYNAME_(dynamic_comb->_RDYNAMEOUT_);
      dynamic_d2tlm_port->_DATNAME_(dynamic_comb->_DATNAMEOUT_);

      // Bound but no ptr indicates a cosim interface
      dynamic_comb->out_bound = true;
      dynamic_comb->out_ptr = 0;
    }

#endif // CONNECTIONS_SIM_ONLY

#endif // __SYNTHESIS__

#ifdef __CCS_P2P_H
    // Bind to p2p<>::out
    void Bind(p2p<SYN>::out<Message> &rhs) {
      this->_DATNAME_(rhs.o_dat);
      this->_VLDNAME_(rhs.o_vld);
      this->_RDYNAME_(rhs.i_rdy);
    }

    // Bind to p2p<>::chan
    void Bind(p2p<SYN>::chan<Message> &rhs) {
      this->_DATNAME_(rhs.dat);
      this->_VLDNAME_(rhs.vld);
      this->_RDYNAME_(rhs.rdy);
    }
#endif

    // Binding
    template <typename C>
    void operator()(C &rhs) {
      Bind(rhs);
    }

  protected:
    void reset_msg() {
      _DATNAME_.write(0);
    }

    void write_msg(const Message &m) {
      Marshaller<WMessage::width> marshaller;
      WMessage wm(m);
      wm.Marshall(marshaller);
      MsgBits bits = marshaller.GetResult();
      _DATNAME_.write(bits);
    }

    void invalidate_msg() {
      // Only allow message invalidation of not going through SLEC design check,
      // since this is assigning to an uninitilized variable, but intentional in
      // HLS for x-prop.
#ifndef SLEC_CPC
      MsgBits dc_bits;
      _DATNAME_.write(dc_bits);
#endif
    }
  };



  template <typename Message>
  class OutBlocking <Message, MARSHALL_PORT> : public OutBlocking_SimPorts_abs<Message>
  {
  public:
    // Interface
    typedef Wrapped<Message> WMessage;
    static const unsigned int width = WMessage::width;
    typedef sc_lv<WMessage::width> MsgBits;
    sc_out<MsgBits> _DATNAME_;
#ifdef CONNECTIONS_SIM_ONLY
    out_port_marker marker;
    OutBlocking<Message, MARSHALL_PORT> *driver;
#endif

    OutBlocking() : OutBlocking_SimPorts_abs<Message>(),
      _DATNAME_(sc_gen_unique_name(_DATNAMEOUTSTR_))
#ifdef CONNECTIONS_SIM_ONLY
      , driver(0)
      , log_stream(0)
#endif
    {}

    explicit OutBlocking(const char *name)
      : OutBlocking_SimPorts_abs<Message>(name)
      , _DATNAME_(CONNECTIONS_CONCAT(name, _DATNAMESTR_))
#ifdef CONNECTIONS_SIM_ONLY
      , marker(CONNECTIONS_CONCAT(name, "out_port_marker"), width, &(this->_VLDNAME_), &(this->_RDYNAME_), &_DATNAME_)
      , driver(0)
      , log_stream(0)
#endif
    {}

    virtual ~OutBlocking() {} 

    // Reset write
    void Reset() {
      OutBlocking_SimPorts_abs<Message>::Reset();
    }

// Push
#pragma builtin_modulario
#pragma design modulario < out >
    void Push(const Message &m) {
      OutBlocking_SimPorts_abs<Message>::Push(m);
    }

// PushNB
#pragma builtin_modulario
#pragma design modulario < out >
    bool PushNB(const Message &m, const bool &do_wait = true) {
      return OutBlocking_SimPorts_abs<Message>::PushNB(m,do_wait);
    }

    // Bind to OutBlocking
    void Bind(OutBlocking<Message, MARSHALL_PORT> &rhs) {
#ifdef CONNECTIONS_SIM_ONLY
      rhs.disable_spawn();
      rhs.non_leaf_port = true;
      rhs.driver = this;
#endif
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    // Bind to Combinational Marshall Port
    void Bind(Combinational<Message, MARSHALL_PORT> &rhs) {
#ifdef CONNECTIONS_SIM_ONLY
      this->_DATNAME_(rhs._DATNAMEIN_);
      this->_VLDNAME_(rhs._VLDNAMEIN_);
      this->_RDYNAME_(rhs._RDYNAMEIN_);
      rhs.driver = this;
      rhs.in_bound = true;
      rhs.in_ptr = this;
      marker.top_port = true;
#else
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
#endif
    }

    // For safety disallow DIRECT_PORT <-> MARSHALL_PORT binding during HLS
#ifndef __SYNTHESIS__
    void Bind(OutBlocking<Message, DIRECT_PORT> &rhs) {
      MarshalledToDirectOutPort<Message> *dynamic_m2dport;
      dynamic_m2dport = new MarshalledToDirectOutPort<Message>(sc_gen_unique_name("dynamic_m2dport"));
      this->sc_mod_alloc.push_back(dynamic_m2dport);

#ifdef CONNECTIONS_SIM_ONLY
      dynamic_m2dport->sibling_port = this; 
      rhs.disable_spawn();
      rhs.non_leaf_port = true;
#endif

      this->_DATNAME_(dynamic_m2dport->msgbits);
      dynamic_m2dport->_DATNAME_(rhs._DATNAME_);
      dynamic_m2dport->_VLDNAME_(rhs._VLDNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    void Bind(Combinational<Message, DIRECT_PORT> &rhs) {
      MarshalledToDirectOutPort<Message> *dynamic_m2dport;

      dynamic_m2dport = new MarshalledToDirectOutPort<Message>(sc_gen_unique_name("dynamic_m2dport"));
      this->sc_mod_alloc.push_back(dynamic_m2dport);
      this->_DATNAME_(dynamic_m2dport->msgbits);

#ifdef CONNECTIONS_SIM_ONLY
      dynamic_m2dport->sibling_port = this; 
      dynamic_m2dport->_DATNAME_(rhs._DATNAMEIN_);
      dynamic_m2dport->_VLDNAME_(rhs._VLDNAMEIN_);
      this->_VLDNAME_(rhs._VLDNAMEIN_);
      this->_RDYNAME_(rhs._RDYNAMEIN_);
      rhs.in_bound = true;
      rhs.in_ptr = this;
#else
      dynamic_m2dport->_DATNAME_(rhs._DATNAME_);
      dynamic_m2dport->_VLDNAME_(rhs._VLDNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
#endif
    }

#ifdef CONNECTIONS_SIM_ONLY
    void Bind(Combinational<Message, TLM_PORT> &rhs) {
      DirectToTLMInPort<Message> *dynamic_d2tlm_port;
      Combinational<Message, DIRECT_PORT> *dynamic_comb;

      dynamic_d2tlm_port = new DirectToTLMInPort<Message>(sc_gen_unique_name("dynamic_d2tlm_port"), rhs.fifo);
      dynamic_d2tlm_port->sibling_port = this;
      dynamic_comb = new Combinational<Message, DIRECT_PORT>(sc_gen_unique_name("dynamic_comb") );
      this->con_obj_alloc.push_back(dynamic_d2tlm_port);
      this->con_obj_alloc.push_back(dynamic_comb);

      // Bind the marshaller to combinational
      this->Bind(*dynamic_comb);
      dynamic_d2tlm_port->_VLDNAME_(dynamic_comb->_VLDNAMEOUT_);
      dynamic_d2tlm_port->_RDYNAME_(dynamic_comb->_RDYNAMEOUT_);
      dynamic_d2tlm_port->_DATNAME_(dynamic_comb->_DATNAMEOUT_);

      // Bound but no ptr indicates a cosim interface
      dynamic_comb->out_bound = true;
      dynamic_comb->out_ptr = 0;
    }

#endif // CONNECTIONS_SIM_ONLY

#endif // __SYNTHESIS__

    // Binding
    template <typename C>
    void operator()(C &rhs) {
      Bind(rhs);
    }

  protected:
    void reset_msg() {
      _DATNAME_.write(0);
    }

#ifdef CONNECTIONS_SIM_ONLY
    Message traced_msg;
#endif

    void write_msg(const Message &m) {
#ifdef CONNECTIONS_SIM_ONLY
      traced_msg = m;
#endif
      Marshaller<WMessage::width> marshaller;
      WMessage wm(m);
      wm.Marshall(marshaller);
      MsgBits bits = marshaller.GetResult();
      _DATNAME_.write(bits);

#ifdef CONNECTIONS_SIM_ONLY
      if (log_stream)
      { *log_stream << std::dec << log_number << " | " << std::hex <<  m << " | " << sc_time_stamp() << "\n"; }
#endif
    }

    void invalidate_msg() {
      // Only allow message invalidation of not going through SLEC design check,
      // since this is assigning to an uninitilized variable, but intentional in
      // HLS for x-prop.
#ifndef SLEC_CPC
      MsgBits dc_bits;
      _DATNAME_.write(dc_bits);
#endif
    }

  public:
#ifdef CONNECTIONS_SIM_ONLY
    void set_trace(sc_trace_file *trace_file_ptr, std::string full_name) {
      sc_trace(trace_file_ptr, traced_msg, full_name);

      if (this->disable_spawn_true) {
        sc_spawn_options opt;
        opt.spawn_method();
        opt.set_sensitivity(&(_DATNAME_.value_changed()));
        opt.dont_initialize();
        sc_spawn(sc_bind(&OutBlocking<Message, MARSHALL_PORT>::trace_convert, this), 0, &opt);
      }
    }

    void trace_convert() {
      traced_msg = convert_from_lv<Message>(_DATNAME_.read());
    }

    std::ofstream *log_stream;
    int log_number;

    void set_log(int num, std::ofstream *fp) {
      log_stream = fp;
      log_number = num;
    }
#endif
  };

  template <typename Message>
  class OutBlocking <Message, DIRECT_PORT> : public OutBlocking_SimPorts_abs<Message>
  {
  public:
    // Interface
    sc_out<Message> _DATNAME_;
#ifdef CONNECTIONS_SIM_ONLY
    OutBlocking<Message, DIRECT_PORT>* driver{0};
#endif

    OutBlocking() : OutBlocking_SimPorts_abs<Message>(),
      _DATNAME_(sc_gen_unique_name(_DATNAMEOUTSTR_)) {}

    explicit OutBlocking(const char *name) : OutBlocking_SimPorts_abs<Message>(name),
      _DATNAME_(CONNECTIONS_CONCAT(name, _DATNAMESTR_)) {}

    virtual ~OutBlocking() {}

    // Reset write
    void Reset() {
      OutBlocking_SimPorts_abs<Message>::Reset();
    }

// Push
#pragma builtin_modulario
#pragma design modulario < out >
    void Push(const Message &m) {
      OutBlocking_SimPorts_abs<Message>::Push(m);
    }

// PushNB
#pragma builtin_modulario
#pragma design modulario < out >
    bool PushNB(const Message &m, const bool &do_wait = true) {
      return OutBlocking_SimPorts_abs<Message>::PushNB(m, do_wait);
    }

    // Bind to OutBlocking
    void Bind(OutBlocking<Message, DIRECT_PORT> &rhs) {
#ifdef CONNECTIONS_SIM_ONLY
      rhs.disable_spawn();
      rhs.non_leaf_port = true;
      rhs.driver = this;
#endif
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    // Bind to Combinational
    void Bind(Combinational<Message, DIRECT_PORT> &rhs) {
#ifdef CONNECTIONS_SIM_ONLY
      this->_DATNAME_(rhs._DATNAMEIN_);
      this->_VLDNAME_(rhs._VLDNAMEIN_);
      this->_RDYNAME_(rhs._RDYNAMEIN_);
      rhs.driver = this;
      rhs.in_bound = true;
      rhs.in_ptr = this;
#else
      this->_DATNAME_(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
#endif
    }

    // For safety disallow DIRECT_PORT <-> MARSHALL_PORT binding during HLS
#ifndef __SYNTHESIS__
    void Bind(OutBlocking<Message, MARSHALL_PORT> &rhs) {
      DirectToMarshalledOutPort<Message> *dynamic_d2mport;
      dynamic_d2mport = new DirectToMarshalledOutPort<Message>("dynamic_d2mport");
      this->sc_mod_alloc.push_back(dynamic_d2mport);

#ifdef CONNECTIONS_SIM_ONLY
      dynamic_d2mport->sibling_port = this; 
      rhs.disable_spawn();
      rhs.non_leaf_port = true;
#endif

      this->_DATNAME_(dynamic_d2mport->_DATNAME_);
      dynamic_d2mport->msgbits(rhs._DATNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
    }

    void Bind(Combinational<Message, MARSHALL_PORT> &rhs) {
      DirectToMarshalledOutPort<Message> *dynamic_d2mport;

      dynamic_d2mport = new DirectToMarshalledOutPort<Message>("dynamic_d2mport");
      this->sc_mod_alloc.push_back(dynamic_d2mport);
      this->_DATNAME_(dynamic_d2mport->_DATNAME_);

#ifdef CONNECTIONS_SIM_ONLY
      dynamic_d2mport->sibling_port = this; 
      dynamic_d2mport->_DATNAME_(rhs._DATNAMEIN_);
      dynamic_d2mport->_VLDNAME_(rhs._VLDNAMEIN_);
      this->_VLDNAME_(rhs._VLDNAMEIN_);
      this->_RDYNAME_(rhs._RDYNAMEIN_);
      rhs.in_bound = true;
      rhs.in_ptr = this;
#else
      dynamic_d2mport->_DATNAME_(rhs._DATNAME_);
      dynamic_d2mport->_VLDNAME_(rhs._VLDNAME_);
      this->_VLDNAME_(rhs._VLDNAME_);
      this->_RDYNAME_(rhs._RDYNAME_);
#endif
    }
#endif // __SYNTHESIS__

    // Binding
    template <typename C>
    void operator()(C &rhs) {
      Bind(rhs);
    }

  protected:
    void reset_msg() {
      Message dc;
      set_default_value(dc);
      _DATNAME_.write(dc);
    }

#ifdef CONNECTIONS_SIM_ONLY
    Message traced_msg;
#endif

    void write_msg(const Message &m) {
#ifdef CONNECTIONS_SIM_ONLY
      traced_msg = m;
#endif
      _DATNAME_.write(m);
#ifdef CONNECTIONS_SIM_ONLY
      if (log_stream)
      { *log_stream << std::dec << log_number << " | " << std::hex <<  m << " | " << sc_time_stamp() << "\n"; }
#endif

    }

    void invalidate_msg() {
      Message dc;
      set_default_value(dc);
      _DATNAME_.write(dc);
    }

  public:
#ifdef CONNECTIONS_SIM_ONLY
    void set_trace(sc_trace_file *trace_file_ptr, std::string full_name) {
      sc_trace(trace_file_ptr, traced_msg, full_name);

      if (this->disable_spawn_true) {
        sc_spawn_options opt;
        opt.spawn_method();
        opt.set_sensitivity(&(_DATNAME_.value_changed()));
        opt.dont_initialize();
        sc_spawn(sc_bind(&OutBlocking<Message, DIRECT_PORT>::trace_convert, this), 0, &opt);
      }
    }

    void trace_convert() {
      traced_msg = _DATNAME_.read();
    }

    std::ofstream *log_stream{0};
    int log_number{0};

    void set_log(int num, std::ofstream *fp) {
      log_stream = fp;
      log_number = num;
    }
#endif
  };

  template <typename Message>
  class write_log_if : public sc_interface {
  public:
    virtual void write_log(const Message& m) = 0;
  };


#ifdef CONNECTIONS_SIM_ONLY
  template <typename Message>
  class OutBlocking <Message, TLM_PORT> : public OutBlocking_abs<Message>
  {
  public:

    OutBlocking() : OutBlocking_abs<Message>(),
      o_fifo(sc_gen_unique_name("o_fifo")) {}

    explicit OutBlocking(const char *name) : OutBlocking_abs<Message>(name),
      o_fifo(CONNECTIONS_CONCAT(name,"o_fifo")) {}

    virtual ~OutBlocking() {}

    // Reset write
    void Reset() {
      this->write_reset_check.reset(this->non_leaf_port);
      get_conManager().add_clock_event(this);
    }

    bool do_reset_check() {
      return this->write_reset_check.check();
    }

#ifndef __SYNTHESIS__
    std::string report_name() {
      return this->write_reset_check.report_name();
    }
#endif

// Push
#pragma design modulario < out >
    void Push(const Message &m) {
      // this->write_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      o_fifo->put(m);
      write_log->write_log(m);
      wait(sc_core::SC_ZERO_TIME);
    }

// PushNB
#pragma design modulario < out >
    bool PushNB(const Message &m, const bool &do_wait = true) {
      // this->write_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      bool ret = o_fifo->nb_put(m);
      if (ret)
        write_log->write_log(m);
      return ret;
    }

    // Bind to OutBlocking
    void Bind(OutBlocking<Message, TLM_PORT> &rhs) {
      this->o_fifo(rhs.o_fifo);
      this->write_log(rhs.write_log);
    }

    // Bind to Combinational
    void Bind(Combinational<Message, TLM_PORT> &rhs) {
      this->o_fifo(rhs.fifo);
      this->write_log(rhs);
    }

    // Binding
    template <typename C>
    void operator()(C &rhs) {
      Bind(rhs);
    }

  protected:
    sc_port<tlm::tlm_fifo_put_if<Message> > o_fifo;
    sc_port<write_log_if<Message> > write_log;
  };
#endif //CONNECTIONS_SIM_ONLY


//------------------------------------------------------------------------
// Out
//------------------------------------------------------------------------

  template <typename Message>
  class Out<Message, SYN_PORT> : public OutBlocking<Message, SYN_PORT>
  {
  public:
    Out() {}

    explicit Out(const char *name) : OutBlocking<Message, SYN_PORT>(name) {}

    virtual ~Out() {}

    // Full
    bool Full() {
      QUERY_CALL();
      return !this->_RDYNAME_.read();
    }
  };


  template <typename Message>
  class Out<Message, MARSHALL_PORT> : public OutBlocking<Message, MARSHALL_PORT>
  {
  public:
    Out() {}

    explicit Out(const char *name) : OutBlocking<Message, MARSHALL_PORT>(name) {}

    virtual ~Out() {}

    // Full
    bool Full() {
#ifdef CONNECTIONS_SIM_ONLY
      return this->Full_SIM();
#else
      QUERY_CALL();
      return !this->_RDYNAME_.read();
#endif
    }
  };


  template <typename Message>
  class Out<Message, DIRECT_PORT> : public OutBlocking<Message, DIRECT_PORT>
  {
  public:
    Out() {}

    explicit Out(const char *name) : OutBlocking<Message, DIRECT_PORT>(name) {}

    virtual ~Out() {}

    // Full
    bool Full() {
#ifdef CONNECTIONS_SIM_ONLY
      return this->Full_SIM();
#else
      QUERY_CALL();
      return !this->_RDYNAME_.read();
#endif
    }
  };


#ifdef CONNECTIONS_SIM_ONLY
  template <typename Message>
  class Out<Message, TLM_PORT> : public OutBlocking<Message, TLM_PORT>
  {
  public:
    Out() {}

    explicit Out(const char *name) : OutBlocking<Message, TLM_PORT>(name) {}

    virtual ~Out() {}

    // Full
    bool Full() {
      return ! this->o_fifo->nb_can_put();
    }
  };
#endif


//------------------------------------------------------------------------
// Combinational MARSHALL_PORT
//------------------------------------------------------------------------

#ifdef CONNECTIONS_SIM_ONLY
  template <typename Message>
  class BA_Message
  {
  public:
    Message m;
    unsigned long ready_cycle;
  };

  class Connections_BA_abs : public sc_module
  {
  public:

    Connections_BA_abs() : sc_module(sc_module_name(sc_gen_unique_name("chan_ba"))) {}

    explicit Connections_BA_abs(const char *name) : sc_module(sc_module_name(name)) {}

    virtual ~Connections_BA_abs() {}

    virtual void annotate(unsigned long latency, unsigned int capacity) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }

    void disable_annotate() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }

    virtual const char *src_name() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
      return 0;
    }

    virtual const char *dest_name() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
      return 0;
    }
  };
#endif

  template <typename Message>
  class Combinational_abs
  {
    // Abstract class
  protected:
    ResetChecker read_reset_check, write_reset_check;

    // Default constructor
    Combinational_abs()
      : read_reset_check("unnamed_comb"),
        write_reset_check("unnamed_comb") {}

    // Constructor
    explicit Combinational_abs(const char *name)
      : read_reset_check(name),
        write_reset_check(name) {}

  public:
    virtual ~Combinational_abs() {}

    // Reset
    void ResetRead() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }

    void ResetWrite() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }

// Pop
#pragma design modulario < in >
    Message Pop() {
      Message m;
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
      return m;
    }

// Peek
#pragma design modulario < in >
    Message Peek() {
      Message m;
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
      return m;
    }

    bool PeekNB(Message &/*data*/, const bool &/*unused*/ = true) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
      return false;
    }
    
// PopNB
#pragma design modulario < in >
    bool PopNB(Message &data) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
      return false;
    }

// ekrimer: commenting these out, should really be EmptyWrite and EmptyRead, and in anycase will not work in verilog currently
    // Empty
//  bool Empty() { return !val.read(); }

// Push
#pragma design modulario < out >
    void Push(const Message &m) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }

// PushNB
#pragma design modulario < out >
    bool PushNB(const Message &m) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
      return false;
    }

// ekrimer: commenting out, similar to Empty
    // Full
//  bool Full() { return !rdy.read(); }
  };

  template <typename Message>
  class Combinational_Ports_abs : public Combinational_abs<Message>
#ifdef CONNECTIONS_SIM_ONLY
    , public Blocking_abs
#endif
  {
  public:
    // Interface
    sc_signal<bool> _VLDNAME_;
    sc_signal<bool> _RDYNAME_;

    // Abstract class
  protected:
    // Default constructor
    Combinational_Ports_abs()
      : Combinational_abs<Message>(),
        _VLDNAME_(sc_gen_unique_name(_COMBVLDNAMESTR_)),
        _RDYNAME_(sc_gen_unique_name(_COMBRDYNAMESTR_)) {
#ifndef __SYNTHESIS__
      this->read_reset_check.set_val_name(_VLDNAME_.name());
      this->write_reset_check.set_val_name(_VLDNAME_.name());
#endif
    }

    // Constructor
    explicit Combinational_Ports_abs(const char *name)
      : Combinational_abs<Message>(name),
        _VLDNAME_(CONNECTIONS_CONCAT(name, _VLDNAMESTR_)),
        _RDYNAME_(CONNECTIONS_CONCAT(name, _RDYNAMESTR_)) {
#ifndef __SYNTHESIS__
      this->read_reset_check.set_val_name(_VLDNAME_.name());
      this->write_reset_check.set_val_name(_VLDNAME_.name());
#endif
    }

  public:
    virtual ~Combinational_Ports_abs() {}

    // Reset
    void ResetRead() {
      this->read_reset_check.reset(false);
      _RDYNAME_.write(false);
    }

    void ResetWrite() {
      this->write_reset_check.reset(false);
      _VLDNAME_.write(false);
      reset_msg();
    }

    bool do_reset_check() {
      bool r{0};
      r |= this->read_reset_check.check();
      r |= this->write_reset_check.check();
      return r;
    }

#ifndef __SYNTHESIS__
    std::string report_name() {
      return this->write_reset_check.report_name();
    }
#endif

// Pop
#pragma builtin_modulario
#pragma design modulario < in >
    Message Pop() {
      // this->read_reset_check.check();
#ifdef CONNECTIONS_SIM_ONLY
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      do {
        _RDYNAME_.write(true);
        wait();
      } while (_VLDNAME_.read() != true);
      _RDYNAME_.write(false);
      Message m;
      read_msg(m);
      return m;
    }

// Peek
#pragma design modulario < in >
    Message Peek() {
      // this->read_reset_check.check();
#ifdef CONNECTIONS_SIM_ONLY
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      while (!_VLDNAME_.read()) {
        wait();
      }
      Message m;
      read_msg(m);
      return m;
    }

    bool PeekNB(Message &data, const bool &/*unused*/ = true) {
      read_msg(data);
#ifdef __SYNTHESIS__      
      _RDYNAME_.write(false);
#endif
      return _VLDNAME_.read();
    }
    
    
// PopNB
#pragma builtin_modulario
#pragma design modulario < in >
    bool PopNB(Message &data) {
      // this->read_reset_check.check();
#ifdef CONNECTIONS_SIM_ONLY
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      _RDYNAME_.write(true);
      wait();
      _RDYNAME_.write(false);
      read_msg(data);
      return _VLDNAME_.read();
    }

// ekrimer: commenting these out, should really be EmptyWrite and EmptyRead, and in anycase will not work in verilog currently
    // Empty
//  bool Empty() { return !val.read(); }

// Push
#pragma builtin_modulario
#pragma design modulario < out >
    void Push(const Message &m) {
      // this->write_reset_check.check();
#ifdef CONNECTIONS_SIM_ONLY
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      do {
        _VLDNAME_.write(true);
        write_msg(m);
        wait();
      } while (_RDYNAME_.read() != true);
      _VLDNAME_.write(false);
    }

// PushNB
#pragma builtin_modulario
#pragma design modulario < out >
    bool PushNB(const Message &m) {
      // this->write_reset_check.check();
#ifdef CONNECTIONS_SIM_ONLY
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      _VLDNAME_.write(true);
      write_msg(m);
      wait();
      _VLDNAME_.write(false);
      return _RDYNAME_.read();
    }

// ekrimer: commenting out, similar to Empty
    // Full
//  bool Full() { return !rdy.read(); }

  protected:
    virtual void reset_msg() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
    virtual void read_msg(Message &m) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
    virtual void write_msg(const Message &m) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
    virtual void invalidate_msg() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
  };


  template <typename Message, connections_port_t port_marshall_type>
  class Combinational_SimPorts_abs
#ifdef CONNECTIONS_SIM_ONLY
    : public Combinational_abs<Message>,
      public Connections_BA_abs,
      public Blocking_abs
#else
    : public Combinational_Ports_abs<Message>
#endif
  {
#ifdef CONNECTIONS_SIM_ONLY
    SC_HAS_PROCESS(Combinational_SimPorts_abs);
#endif

    // Abstract class
  protected:
    // Default constructor
    Combinational_SimPorts_abs()
#ifdef CONNECTIONS_SIM_ONLY
      : Combinational_abs<Message>()

      , Connections_BA_abs(sc_gen_unique_name("comb_ba"))

        //, in_msg(sc_gen_unique_name("comb_in_msg"))
      , _VLDNAMEIN_(sc_gen_unique_name(_COMBVLDNAMEINSTR_))
      , _RDYNAMEIN_(sc_gen_unique_name(_COMBRDYNAMEINSTR_))
        //, out_msg(sc_gen_unique_name("comb_out_msg"))
      , _VLDNAMEOUT_(sc_gen_unique_name(_COMBVLDNAMEOUTSTR_))
      , _RDYNAMEOUT_(sc_gen_unique_name(_COMBRDYNAMEOUTSTR_))

      , current_cycle(0)
      , latency(0)

      , out_bound(false), in_bound(false)
      , in_str(0), out_str(0)
      , sim_out(sc_gen_unique_name("sim_out")), sim_in(sc_gen_unique_name("sim_in"))
#else
      : Combinational_Ports_abs<Message>()
#endif
    {
#ifdef CONNECTIONS_SIM_ONLY
      Init_SIM("comb");
#endif
    }

    // Constructor
    explicit Combinational_SimPorts_abs(const char *name)
#ifdef CONNECTIONS_SIM_ONLY
      : Combinational_abs<Message>(name)

      , Connections_BA_abs(CONNECTIONS_CONCAT(name, "comb_BA"))

        //, in_msg(CONNECTIONS_CONCAT(name, "comb_in_msg"))
      , _VLDNAMEIN_(CONNECTIONS_CONCAT(name, _COMBVLDNAMEINSTR_))
      , _RDYNAMEIN_(CONNECTIONS_CONCAT(name, _COMBRDYNAMEINSTR_))
        //, out_msg(CONNECTIONS_CONCAT(name, "comb_out_msg"))
      , _VLDNAMEOUT_(CONNECTIONS_CONCAT(name, _COMBVLDNAMEOUTSTR_))
      , _RDYNAMEOUT_(CONNECTIONS_CONCAT(name, _COMBRDYNAMEOUTSTR_))

      , current_cycle(0)
      , latency(0)

      , out_bound(false), in_bound(false)
      , in_str(0), out_str(0)
      , sim_out(CONNECTIONS_CONCAT(name,"sim_out")), sim_in(CONNECTIONS_CONCAT(name, "sim_in"))
#else
      : Combinational_Ports_abs<Message>(name)
#endif
    {
#ifdef CONNECTIONS_SIM_ONLY
      Init_SIM("comb");
#endif
    }

    std::string full_name() { return "Combinational_SimPorts_abs"; }

  public:
    virtual ~Combinational_SimPorts_abs() {}

#ifdef CONNECTIONS_SIM_ONLY
    //sc_signal<MsgBits> in_msg;
    sc_signal<bool>     _VLDNAMEIN_;
    sc_signal<bool>     _RDYNAMEIN_;
    //sc_signal<MsgBits> out_msg;
    sc_signal<bool>    _VLDNAMEOUT_;
    sc_signal<bool>    _RDYNAMEOUT_;
    unsigned long current_cycle;
    unsigned long latency;
    tlm::circular_buffer< BA_Message<Message> > b;
#endif

    // Reset
    void ResetRead() {
#ifdef CONNECTIONS_SIM_ONLY
      /* assert(! out_bound); */

      this->read_reset_check.reset(false);
      get_conManager().add_clock_event(this);

      sim_in.Reset();
      Reset_SIM();
#else
      Combinational_Ports_abs<Message>::ResetRead();
#endif
    }

    void ResetWrite() {
#ifdef CONNECTIONS_SIM_ONLY
      /* assert(! in_bound); */

      this->write_reset_check.reset(false);
      get_conManager().add_clock_event(this);

      sim_out.Reset();

      Reset_SIM();
#else
      Combinational_Ports_abs<Message>::ResetWrite();
#endif
    }

    bool do_reset_check() {
      /*
          this->read_reset_check.check();
          this->write_reset_check.check();
      */
      /*
          Combinational_SimPorts_abs needs special handling for registration with ConManager.
          This is because it is possible to do Push/Pop thru regular instantiated In and Out
          Ports, and also possible to do direct Push/Pop on the channel itself (ie "portless
          channel access"). The only way the simulator knows if the latter is occurring is when
          the ResetWrite and ResetRead and Push Pop methods are called on the channel itself.
          The implications are that the reset_check.check() method needs to be done in the Push/Pop
          methods (cannot be done in do_reset_check()), and add_clock_event may or may not have
          been called, even though get_conManager().add() method is always called for this channel.
          So, in ConManager if we see that add_clock_event was not called for this class, it is OK.
      */
      return 0;
    }

// Pop
#pragma design modulario < in >
    Message Pop() {
#ifdef CONNECTIONS_SIM_ONLY
      /* assert(! out_bound); */

      if (this->read_reset_check.check()) {
         SC_REPORT_ERROR("CONNECTIONS-125",
           std::string("Unable to resolve clock on port - check and fix any prior warnings about missing Reset() on ports: ").c_str());
         sc_stop();
      }
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif

      return sim_in.Pop();
#else
      return Combinational_Ports_abs<Message>::Pop();
#endif
    }

// Peek
#pragma design modulario < in >
    Message Peek() {
#ifdef CONNECTIONS_SIM_ONLY
      /* assert(! out_bound); */

      if (this->read_reset_check.check()) {
         SC_REPORT_ERROR("CONNECTIONS-125",
           std::string("Unable to resolve clock on port - check and fix any prior warnings about missing Reset() on ports: ").c_str());
         sc_stop();
      }
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif

      return sim_in.Peek();
#else
      return Combinational_Ports_abs<Message>::Peek();
#endif
    }

    bool PeekNB(Message &data, const bool &/*unused*/ = true) {
#ifdef CONNECTIONS_SIM_ONLY
      this->read_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      return sim_in.PeekNB(data);
#else
      return Combinational_Ports_abs<Message>::PeekNB(data);
#endif

    }

// PopNB
#pragma design modulario < in >
    bool PopNB(Message &data) {
#ifdef CONNECTIONS_SIM_ONLY
      /* assert(! out_bound); */

      if (this->read_reset_check.check()) {
         SC_REPORT_ERROR("CONNECTIONS-125",
           std::string("Unable to resolve clock on port - check and fix any prior warnings about missing Reset() on ports: ").c_str());
         sc_stop();
      }
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif

      return sim_in.PopNB(data);
#else
      return Combinational_Ports_abs<Message>::PopNB(data);
#endif
    }

// ekrimer: commenting these out, should really be EmptyWrite and EmptyRead, and in anycase will not work in verilog currently
    // Empty
//  bool Empty() { return !val.read(); }

#ifdef CONNECTIONS_SIM_ONLY
    Message traced_msg;
#endif

// Push
#pragma design modulario < out >
    void Push(const Message &m) {
#ifdef CONNECTIONS_SIM_ONLY
      /* assert(! in_bound); */

      if (this->write_reset_check.check()) {
         SC_REPORT_ERROR("CONNECTIONS-125",
           std::string("Unable to resolve clock on port - check and fix any prior warnings about missing Reset() on ports: ").c_str());
         sc_stop();
      }
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      traced_msg = m;

      sim_out.Push(m);
#else
      Combinational_Ports_abs<Message>::Push(m);
#endif
    }

// PushNB
#pragma design modulario < out >
    bool PushNB(const Message &m) {
#ifdef CONNECTIONS_SIM_ONLY
      /* assert(! in_bound); */

      if (this->write_reset_check.check()) {
         SC_REPORT_ERROR("CONNECTIONS-125",
           std::string("Unable to resolve clock on port - check and fix any prior warnings about missing Reset() on ports: ").c_str());
         sc_stop();
      }
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif

      return sim_out.PushNB(m);
#else
      return Combinational_Ports_abs<Message>::PushNB(m);
#endif
    }

// ekrimer: commenting out, similar to Empty
    // Full
//  bool Full() { return !rdy.read(); }

#ifdef CONNECTIONS_SIM_ONLY
  public:
    OutBlocking_Ports_abs<Message> *in_ptr;
    InBlocking_Ports_abs<Message> *out_ptr;

    bool out_bound, in_bound;
    const char *in_str, *out_str;

    inline bool is_bypass() {
      return (latency == 0);
    }

    void annotate(unsigned long latency, unsigned int capacity) {
      this->latency = latency;
      assert(! (latency == 0 && capacity > 0)); // latency == 0 && capacity > 0 is not supported.
      assert(! (latency > 0 && capacity == 0)); // latency > 0 but capacity == 0 is not supported.
      if (capacity > 0) {
        this->b.resize(capacity);
      } else {
        this->b.resize(1);
      }

      this->sibling_port = in_ptr;
    }

    void disable_annotate() {
      Connections::get_conManager().remove_annotate(this);
    }

    const char *src_name() {
      if (in_str) {
        return in_str;
      } else if (in_bound) {
        if (in_ptr)
        { return in_ptr->_VLDNAME_.name(); }
        else
        { return "TLM_INTERFACE"; }
      } else {
        return "UNBOUND";
      }
    }

    const char *dest_name() {
      if (out_str) {
        return out_str;
      } else if (out_bound) {
        if (out_ptr)
        { return out_ptr->_VLDNAME_.name(); }
        else
        { return "TLM_INTERFACE"; }
      } else {
        return "UNBOUND";
      }
    }

    void enable_local_rand_stall() {
      sim_in.enable_local_rand_stall();
    }

    void disable_local_rand_stall() {
      sim_in.disable_local_rand_stall();
    }

    void cancel_local_rand_stall() {
      sim_in.cancel_local_rand_stall();
    }

    void enable_local_rand_stall_print_debug() {
      sim_in.enable_local_rand_stall_print_debug();
    }

    void disable_local_rand_stall_print_debug() {
      sim_in.disable_local_rand_stall_print_debug();
    }

    void cancel_local_rand_stall_print_debug() {
      sim_in.cancel_local_rand_stall_print_debug();
    }


  protected:
    OutBlocking<Message,port_marshall_type> sim_out;
    InBlocking<Message,port_marshall_type> sim_in;

    bool data_val;
    bool val_set_by_api;
    bool rdy_set_by_api;

    void Init_SIM(const char *name) {
      data_val = false;
      val_set_by_api = false;
      rdy_set_by_api = false;
      Reset_SIM();
      Connections::get_conManager().add(this);
      Connections::get_conManager().add_annotate(this);
    }

    void Reset_SIM() {
      current_cycle = 0;

      data_val = false;

      while (! b.is_empty()) { b.read(); }
    }

// Although this code is being used only for simulation now, it could be
// synthesized if pre/post are used instead of spawned threads.
// Keep mudulario pragmas here to preserve that option

#pragma design modulario < out >
    void receive(const bool &stall) {
      if (stall) {
        _RDYNAMEIN_.write(false);
        rdy_set_by_api = false;
      } else {
        _RDYNAMEIN_.write(true);
        rdy_set_by_api = true;
      }
    }

#pragma design modulario < in >
    bool received(Message &data) {
      if (_VLDNAMEIN_.read()) {
        /* MsgBits mbits = in_msg.read(); */
        /* Marshaller<WMessage::width> marshaller(mbits); */
        /* WMessage result; */
        /* result.Marshall(marshaller); */
        /* data = result.val; */
        read_msg(data);
        return true;
      }

      // not valid
      // randomize data
      //memset(&data, random(), sizeof(data));
      //if (!data_val)
      //    memset(&data, 0, sizeof(data));
      return false;
    }

#pragma design modulario < in >
    bool transmitted() { return _RDYNAMEOUT_.read(); }



#pragma design modulario < out >
    void transmit_data(const Message &m) {
      /* Marshaller<WMessage::width> marshaller; */
      /* WMessage wm(m); */
      /* wm.Marshall(marshaller); */
      /* MsgBits bits = marshaller.GetResult(); */
      /* out_msg.write(bits); */
      write_msg(m);
    }

#pragma design modulario < out >
    void transmit_val(const bool &vald) {
      if (vald) {
        _VLDNAMEOUT_.write(true);
        val_set_by_api = true;
      } else {
        _VLDNAMEOUT_.write(false);
        val_set_by_api = false;

        //corrupt and transmit data
        //memset(&data_buf, random(), sizeof(data_buf));
        //transmit_data(data_buf);
      }
    }

    bool Pre() {
      if (is_bypass()) { return true; } // TODO: return false to deregister.

      // Input
      if (rdy_set_by_api != _RDYNAMEIN_.read()) {
        // something has changed the value of the signal not through API
        // killing spawned threads;
        return false;
      }

      if (!b.is_full()) {
        Message m;
        if (received(m)) {
          BA_Message<Message> bam;
          bam.m = m;
          assert(latency > 0);
          bam.ready_cycle = current_cycle + latency;
          b.write(bam);
        }
      }

      // Output
      if (!b.is_empty()) {
        if (transmitted() && val_set_by_api) {
          b.read(); // pop
        }
      }
      return true;
    }

    bool PrePostReset() {
      data_val = false;

      current_cycle = 0;
      while (! b.is_empty()) { b.read(); }

      return true;
    }

    bool Post() {
      current_cycle++; // Increment to next cycle.

      if (is_bypass()) { return true; } // TODO: return false to deregister.

      // Input
      receive(b.is_full());

      // Output
      if (val_set_by_api != _VLDNAMEOUT_.read()) {
        // something has changed the value of the signal not through API
        // killing spawned threads;
        return false;
      }
      if (! b.is_empty() && (b.read_data().ready_cycle <= current_cycle)) {
        transmit_val(true);
        transmit_data(b.read_data().m); // peek
      } else {
        transmit_val(false);
      }
      return true;
    }

    void FillBuf_SIM(const Message &m) {
      BA_Message<Message> bam;
      bam.m = m;
      bam.ready_cycle = current_cycle + latency;
      assert(! b.is_full());
      b.write(bam);
    }

    bool Empty_SIM() {
      return b.is_empty();
    }

    bool Full_SIM() {
      return b.is_full();
    }

    void Push_SIM(const Message &m) {
      while (Full_SIM()) {
        wait();
      }
      FillBuf_SIM(m);
    }

    // If sim accurate mode is enabled, we use our own abstract declarations of these
    // since Combinational_Ports_abs includes sc_signal rdy/val signals, which
    // SimPorts may differentiate into in_rdy/in_val. There may be a better way to
    // factor this further, but SimPorts also takes over Push/Pop etc for sim accurate mode
    // already so we aren't strongly dependent on Combinational_Ports_abs.

  protected:
    virtual void reset_msg() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
    virtual void read_msg(Message &m) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
    virtual void write_msg(const Message &m) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
    virtual void invalidate_msg() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
#endif //CONNECTIONS_SIM_ONLY
  };


  template <typename Message>
  class Combinational <Message, SYN_PORT> : public Combinational_Ports_abs<Message>
  {
  public:
    // Interface
    typedef Wrapped<Message> WMessage;
    static const unsigned int width = WMessage::width;
    typedef sc_lv<WMessage::width> MsgBits;
    sc_signal<MsgBits> _DATNAME_;

    Combinational() : Combinational_Ports_abs<Message>(),
      _DATNAME_(sc_gen_unique_name(_COMBDATNAMESTR_)) {}

    explicit Combinational(const char *name) : Combinational_Ports_abs<Message>(name),
      _DATNAME_(CONNECTIONS_CONCAT(name, _COMBDATNAMESTR_)) {}

    virtual ~Combinational() {}

    // Reset
    void ResetRead() {
      return Combinational_Ports_abs<Message>::ResetRead();
    }

    void ResetWrite() {
      return Combinational_Ports_abs<Message>::ResetWrite();
    }

// Pop
#pragma builtin_modulario
#pragma design modulario < in >
    Message Pop() {
      return Combinational_Ports_abs<Message>::Pop();
    }

// Peek
#pragma design modulario < in >
    Message Peek() {
      return Combinational_Ports_abs<Message>::Peek();
    }

#pragma builtin_modulario
#pragma design modulario < peek >    
    bool PeekNB(Message &data, const bool &/*unused*/ = true) {
      return Combinational_Ports_abs<Message>::PeekNB(data, true);
    }
    

// PopNB
#pragma builtin_modulario
#pragma design modulario < in >
    bool PopNB(Message &data) {
      return Combinational_Ports_abs<Message>::PopNB(data);
    }

// Push
#pragma builtin_modulario
#pragma design modulario < out >
    void Push(const Message &m) {
      Combinational_Ports_abs<Message>::Push(m);
    }

// PushNB
#pragma builtin_modulario
#pragma design modulario < out >
    bool PushNB(const Message &m) {
      return Combinational_Ports_abs<Message>::PushNB(m);
    }


  protected:
    void reset_msg() {
      _DATNAME_.write(0);
    }

    void read_msg(Message &m) {
      MsgBits mbits = _DATNAME_.read();
      Marshaller<WMessage::width> marshaller(mbits);
      WMessage result;
      result.Marshall(marshaller);
      m = result.val;
    }

    void write_msg(const Message &m) {
      Marshaller<WMessage::width> marshaller;
      WMessage wm(m);
      wm.Marshall(marshaller);
      MsgBits bits = marshaller.GetResult();
      _DATNAME_.write(bits);
    }

    void invalidate_msg() {
      // Only allow message invalidation of not going through SLEC design check,
      // since this is assigning to an uninitilized variable, but intentional in
      // HLS for x-prop.
#ifndef SLEC_CPC
      MsgBits dc_bits;
      _DATNAME_.write(dc_bits);
#endif
    }
  };

  template <typename Message>
  class Combinational <Message, MARSHALL_PORT> : public Combinational_SimPorts_abs<Message, MARSHALL_PORT>
  {
#ifdef CONNECTIONS_SIM_ONLY
    SC_HAS_PROCESS(Combinational);
#endif

  public:
    // Interface
    typedef Wrapped<Message> WMessage;
    static const unsigned int width = WMessage::width;
    typedef sc_lv<WMessage::width> MsgBits;
#ifdef CONNECTIONS_SIM_ONLY
    sc_signal<MsgBits> _DATNAMEIN_;
    sc_signal<MsgBits> _DATNAMEOUT_;
    OutBlocking<Message, MARSHALL_PORT> *driver;
#else
    sc_signal<MsgBits> _DATNAME_;
#endif

    Combinational() : Combinational_SimPorts_abs<Message, MARSHALL_PORT>()
#ifdef CONNECTIONS_SIM_ONLY
      ,_DATNAMEIN_(sc_gen_unique_name(_COMBDATNAMEINSTR_))
      ,_DATNAMEOUT_(sc_gen_unique_name(_COMBDATNAMEOUTSTR_))
      ,dummyPortManager(sc_gen_unique_name("dummyPortManager"), this->sim_in, this->sim_out, *this)
#else
      ,_DATNAME_(sc_gen_unique_name(_COMBDATNAMESTR_))
#endif
    {
#ifdef CONNECTIONS_SIM_ONLY
      driver = 0;

      // SC_METHOD(do_bypass); // Cannot use due to duplicate name warnings during runtime..
      // this->sensitive << _DATNAMEIN_ << this->_VLDNAMEIN_ << this->_RDYNAMEOUT_;
      {
        sc_spawn_options opt;
        opt.spawn_method();
        opt.set_sensitivity(&(this->_DATNAMEIN_.default_event()));
        opt.set_sensitivity(&(this->_VLDNAMEIN_.default_event()));
        opt.set_sensitivity(&(this->_RDYNAMEOUT_.default_event()));
        opt.dont_initialize();
        sc_spawn(sc_bind(&Combinational<Message, MARSHALL_PORT>::do_bypass, this), 0, &opt);
      }

#endif
    }

    explicit Combinational(const char *name) : Combinational_SimPorts_abs<Message, MARSHALL_PORT>(name)

#ifdef CONNECTIONS_SIM_ONLY
      ,_DATNAMEIN_(CONNECTIONS_CONCAT(name, _COMBDATNAMEINSTR_))
      ,_DATNAMEOUT_(CONNECTIONS_CONCAT(name, _COMBDATNAMEOUTSTR_))
      ,dummyPortManager(CONNECTIONS_CONCAT(name, "dummyPortManager"), this->sim_in, this->sim_out, *this)
#else
      ,_DATNAME_(CONNECTIONS_CONCAT(name, _DATNAMESTR_))
#endif
    {
#ifdef CONNECTIONS_SIM_ONLY
      driver = 0;

      // SC_METHOD(do_bypass); // Cannot use due to duplicate name warnings during runtime..
      // this->sensitive << _DATNAMEIN_ << this->_VLDNAMEIN_ << this->_RDYNAMEOUT_;
      {
        sc_spawn_options opt;
        opt.spawn_method();
        opt.set_sensitivity(&(this->_DATNAMEIN_.default_event()));
        opt.set_sensitivity(&(this->_VLDNAMEIN_.default_event()));
        opt.set_sensitivity(&(this->_RDYNAMEOUT_.default_event()));
        opt.dont_initialize();
        sc_spawn(sc_bind(&Combinational<Message, MARSHALL_PORT>::do_bypass, this), 0, &opt);
      }

#endif
    }
    
    virtual ~Combinational() {}

    //void do_bypass() { Combinational_SimPorts_abs<Message,MARSHALL_PORT>::do_bypass(); }

#ifdef CONNECTIONS_SIM_ONLY
    void do_bypass() {
      if (! this->is_bypass()) { return; }
      _DATNAMEOUT_.write(_DATNAMEIN_.read());
      //write_msg(read_msg());
      this->_VLDNAMEOUT_.write(this->_VLDNAMEIN_.read());
      this->_RDYNAMEIN_.write(this->_RDYNAMEOUT_.read());
    }
#endif

    // Parent functions, to get around Catapult virtual function bug.
    void ResetRead() { return Combinational_SimPorts_abs<Message,MARSHALL_PORT>::ResetRead(); }
    void ResetWrite() { return Combinational_SimPorts_abs<Message,MARSHALL_PORT>::ResetWrite(); }
#pragma builtin_modulario
#pragma design modulario < in >
    Message Pop() { return Combinational_SimPorts_abs<Message,MARSHALL_PORT>::Pop(); }
#pragma design modulario < in >
    Message Peek() { return Combinational_SimPorts_abs<Message,MARSHALL_PORT>::Peek(); }
#pragma builtin_modulario
#pragma design modulario < peek >    
    bool PeekNB(Message &data, const bool &/*unused*/ = true) {
      return Combinational_SimPorts_abs<Message,MARSHALL_PORT>::PeekNB(data, true);
    }
    
#pragma builtin_modulario
#pragma design modulario < in >
    bool PopNB(Message &data) { return Combinational_SimPorts_abs<Message,MARSHALL_PORT>::PopNB(data); }
#pragma builtin_modulario
#pragma design modulario < out >
    void Push(const Message &m) { Combinational_SimPorts_abs<Message,MARSHALL_PORT>::Push(m); }
#pragma builtin_modulario
#pragma design modulario < out >
    bool PushNB(const Message &m) { return Combinational_SimPorts_abs<Message,MARSHALL_PORT>::PushNB(m);  }

  protected:
    void reset_msg() {
#ifdef CONNECTIONS_SIM_ONLY
      _DATNAMEOUT_.write(0);
#else
      _DATNAME_.write(0);
#endif
    }

    void read_msg(Message &m) {
#ifdef CONNECTIONS_SIM_ONLY
      MsgBits mbits = _DATNAMEIN_.read();
#else
      MsgBits mbits = _DATNAME_.read();
#endif
      Marshaller<WMessage::width> marshaller(mbits);
      WMessage result;
      result.Marshall(marshaller);
      m = result.val;
    }

    void write_msg(const Message &m) {
      Marshaller<WMessage::width> marshaller;
      WMessage wm(m);
      wm.Marshall(marshaller);
      MsgBits bits = marshaller.GetResult();
#ifdef CONNECTIONS_SIM_ONLY
      _DATNAMEOUT_.write(bits);
#else
      _DATNAME_.write(bits);
#endif
    }

    void invalidate_msg() {
      // Only allow message invalidation of not going through SLEC design check,
      // since this is assigning to an uninitilized variable, but intentional in
      // HLS for x-prop.
#ifndef SLEC_CPC
      MsgBits dc_bits;
#ifdef CONNECTIONS_SIM_ONLY
      _DATNAMEOUT_.write(dc_bits);
#else // ifdef CONNECTIONS_SIM_ONLY
      _DATNAME_.write(dc_bits);
#endif // ifdef CONNECTIONS_SIM_ONLY
#endif // ifndef SLEC_CPC
    }

#ifdef CONNECTIONS_SIM_ONLY
  protected:
    class DummyPortManager : public sc_module, public sc_trace_marker
    {
      SC_HAS_PROCESS(DummyPortManager);
    private:
      InBlocking<Message,MARSHALL_PORT> &in;
      OutBlocking<Message,MARSHALL_PORT> &out;
      Combinational<Message,MARSHALL_PORT> &parent;

    public:
      DummyPortManager(sc_module_name name, InBlocking<Message,MARSHALL_PORT> &in_, OutBlocking<Message,MARSHALL_PORT> &out_, Combinational<Message,MARSHALL_PORT> &parent_)
        : sc_module(name), in(in_), out(out_), parent(parent_) {}

      virtual void before_end_of_elaboration() {
        if (!parent.in_bound) {
          out(parent);
        } else {
          sc_signal<MsgBits> *dummy_out_msg = new sc_signal<MsgBits>;
          sc_signal<bool> *dummy_out_val = new sc_signal<bool>;
          sc_signal<bool> *dummy_out_rdy = new sc_signal<bool>;

          out._DATNAME_(*dummy_out_msg);
          out._VLDNAME_(*dummy_out_val);
          out._RDYNAME_(*dummy_out_rdy);

          out.disable_spawn();
        }
        if (!parent.out_bound) {
          in(parent);
        } else {
          sc_signal<MsgBits> *dummy_in_msg = new sc_signal<MsgBits>;
          sc_signal<bool> *dummy_in_val = new sc_signal<bool>;
          sc_signal<bool> *dummy_in_rdy = new sc_signal<bool>;

          in._DATNAME_(*dummy_in_msg);
          in._VLDNAME_(*dummy_in_val);
          in._RDYNAME_(*dummy_in_rdy);

          in.disable_spawn();
        }
      }

      virtual void set_trace(sc_trace_file *trace_file_ptr) {
        sc_trace(trace_file_ptr, parent._VLDNAMEOUT_, parent._VLDNAMEOUT_.name());
        sc_trace(trace_file_ptr, parent._RDYNAMEOUT_, parent._RDYNAMEOUT_.name());

        if (!parent.driver) {
          // this case occurs for "port-less channel access", ie comb_chan.Push(val)
          OutBlocking<Message, MARSHALL_PORT> *driver = &(parent.sim_out);
          driver->set_trace(trace_file_ptr, parent._DATNAMEOUT_.name());
        } else {
          OutBlocking<Message, MARSHALL_PORT> *driver = parent.driver;
          while (driver->driver)
          { driver = driver->driver; }

          driver->set_trace(trace_file_ptr, parent._DATNAMEOUT_.name());
        }
      }

      bool set_log(std::ofstream *os, int &log_num, std::string &path_name) {
        if (!parent.driver) {
          OutBlocking<Message, MARSHALL_PORT> *driver = &(parent.sim_out);

          path_name = parent.name();
          driver->set_log(++log_num, os);
          return true;
        } else {
          OutBlocking<Message, MARSHALL_PORT> *driver = parent.driver;
          while (driver->driver)
          { driver = driver->driver; }

          path_name = parent._DATNAMEOUT_.name();
          driver->set_log(++log_num, os);
          return true;
        }
      }

    } dummyPortManager;
#endif
  };


  template <typename Message>
  class Combinational <Message, DIRECT_PORT> : public Combinational_SimPorts_abs<Message, DIRECT_PORT>
  {
#ifdef CONNECTIONS_SIM_ONLY
    SC_HAS_PROCESS(Combinational);
#endif

  public:
    // Interface
#ifdef CONNECTIONS_SIM_ONLY
    dbg_signal<Message> _DATNAMEIN_;
    dbg_signal<Message> _DATNAMEOUT_;
    OutBlocking<Message, DIRECT_PORT> *driver{0};  // DGB
#else
#ifdef __SYNTHESIS__
    sc_signal<Message> _DATNAME_;
#else
    dbg_signal<Message> _DATNAME_;
#endif
#endif

    Combinational() : Combinational_SimPorts_abs<Message, DIRECT_PORT>()
#ifdef CONNECTIONS_SIM_ONLY
      ,_DATNAMEIN_(sc_gen_unique_name(_COMBDATNAMEINSTR_))
      ,_DATNAMEOUT_(sc_gen_unique_name(_COMBDATNAMEOUTSTR_ ))
      ,dummyPortManager(sc_gen_unique_name("dummyPortManager"), this->sim_in, this->sim_out, *this)
#else
      ,_DATNAME_(sc_gen_unique_name(_COMBDATNAMESTR_ ))
#endif
    {
#ifdef CONNECTIONS_SIM_ONLY
      // SC_METHOD(do_bypass); // Cannot use due to duplicate name warnings during runtime..
      // this->sensitive << _DATNAMEIN_ << this->_VLDNAMEIN_ << this->_RDYNAMEOUT_;
      {
        sc_spawn_options opt;
        opt.spawn_method();
        opt.set_sensitivity(&(this->_DATNAMEIN_.default_event()));
        opt.set_sensitivity(&(this->_VLDNAMEIN_.default_event()));
        opt.set_sensitivity(&(this->_RDYNAMEOUT_.default_event()));
        opt.dont_initialize();
        sc_spawn(sc_bind(&Combinational<Message, DIRECT_PORT>::do_bypass, this), 0, &opt);
      }
#endif
    }

    explicit Combinational(const char *name) : Combinational_SimPorts_abs<Message, DIRECT_PORT>(name)
#ifdef CONNECTIONS_SIM_ONLY
      ,_DATNAMEIN_(CONNECTIONS_CONCAT(name, _COMBDATNAMEINSTR_))
      ,_DATNAMEOUT_(CONNECTIONS_CONCAT(name, _COMBDATNAMEOUTSTR_))
      ,dummyPortManager(CONNECTIONS_CONCAT(name, "dummyPortManager"), this->sim_in, this->sim_out, *this)
#else
      ,_DATNAME_(CONNECTIONS_CONCAT(name, _DATNAMESTR_))
#endif
    {
#ifdef CONNECTIONS_SIM_ONLY
      // SC_METHOD(do_bypass); // Cannot use due to duplicate name warnings during runtime..
      // this->sensitive << _DATNAMEIN_ << this->_VLDNAMEIN_ << this->_RDYNAMEOUT_;
      {
        sc_spawn_options opt;
        opt.spawn_method();
        opt.set_sensitivity(&(this->_DATNAMEIN_.default_event()));
        opt.set_sensitivity(&(this->_VLDNAMEIN_.default_event()));
        opt.set_sensitivity(&(this->_RDYNAMEOUT_.default_event()));
        opt.dont_initialize();
        sc_spawn(sc_bind(&Combinational<Message, DIRECT_PORT>::do_bypass, this), 0, &opt);
      }
#endif
    }

    virtual ~Combinational() {}

#ifdef CONNECTIONS_SIM_ONLY
    void do_bypass() {
      if (! this->is_bypass()) { return; }
      _DATNAMEOUT_.write(_DATNAMEIN_.read());
      //write_msg(read_msg());
      this->_VLDNAMEOUT_.write(this->_VLDNAMEIN_.read());
      this->_RDYNAMEIN_.write(this->_RDYNAMEOUT_.read());
    }
#endif

    // Parent functions, to get around Catapult virtual function bug.
    void ResetRead() { return Combinational_SimPorts_abs<Message,DIRECT_PORT>::ResetRead(); }
    void ResetWrite() { return Combinational_SimPorts_abs<Message,DIRECT_PORT>::ResetWrite(); }
#pragma builtin_modulario
#pragma design modulario < in >
    Message Pop() { return Combinational_SimPorts_abs<Message,DIRECT_PORT>::Pop(); }
#pragma design modulario < in >
    Message Peek() { return Combinational_SimPorts_abs<Message,DIRECT_PORT>::Peek(); }

#pragma builtin_modulario
#pragma design modulario < peek >    
    bool PeekNB(Message &data, const bool &/*unused*/ = true) {
      return Combinational_SimPorts_abs<Message,DIRECT_PORT>::PeekNB(data, true);
    }
    
#pragma builtin_modulario
#pragma design modulario < in >
    bool PopNB(Message &data) { return Combinational_SimPorts_abs<Message,DIRECT_PORT>::PopNB(data); }
#pragma builtin_modulario
#pragma design modulario < out >
    void Push(const Message &m) { Combinational_SimPorts_abs<Message,DIRECT_PORT>::Push(m); }
#pragma builtin_modulario
#pragma design modulario < out >
    bool PushNB(const Message &m) { return Combinational_SimPorts_abs<Message,DIRECT_PORT>::PushNB(m);  }

  protected:
    void reset_msg() {
      Message dc;
      set_default_value(dc);
#ifdef CONNECTIONS_SIM_ONLY
      _DATNAMEOUT_.write(dc);
#else
      _DATNAME_.write(dc);
#endif
    }

    void read_msg(Message &m) {
#ifdef CONNECTIONS_SIM_ONLY
      m = _DATNAMEIN_.read();
#else
      m = _DATNAME_.read();
#endif
    }

    void write_msg(const Message &m) {
#ifdef CONNECTIONS_SIM_ONLY
      _DATNAMEOUT_.write(m);
#else
      _DATNAME_.write(m);
#endif
    }

    void invalidate_msg() {
      Message dc;
      set_default_value(dc);
#ifdef CONNECTIONS_SIM_ONLY
      _DATNAMEOUT_.write(dc);
#else
      _DATNAME_.write(dc);
#endif
    }

#ifdef CONNECTIONS_SIM_ONLY
  protected:

    class DummyPortManager : public sc_module, public sc_trace_marker
    {
      SC_HAS_PROCESS(DummyPortManager);
    private:
      InBlocking<Message,DIRECT_PORT> &in;
      OutBlocking<Message,DIRECT_PORT> &out;
      Combinational<Message,DIRECT_PORT> &parent;

    public:
      DummyPortManager(sc_module_name name, InBlocking<Message,DIRECT_PORT> &in_, OutBlocking<Message,DIRECT_PORT> &out_, Combinational<Message,DIRECT_PORT> &parent_)
        : sc_module(name), in(in_), out(out_), parent(parent_) {}

      virtual void before_end_of_elaboration() {
        if (!parent.in_bound) {
          out(parent);
        } else {
          sc_signal<Message> *dummy_out_msg = new sc_signal<Message>;
          sc_signal<bool> *dummy_out_val = new sc_signal<bool>;
          sc_signal<bool> *dummy_out_rdy = new sc_signal<bool>;

          out._DATNAME_(*dummy_out_msg);
          out._VLDNAME_(*dummy_out_val);
          out._RDYNAME_(*dummy_out_rdy);

          out.disable_spawn();
        }
        if (!parent.out_bound) {
          in(parent);
        } else {
          sc_signal<Message> *dummy_in_msg = new sc_signal<Message>;
          sc_signal<bool> *dummy_in_val = new sc_signal<bool>;
          sc_signal<bool> *dummy_in_rdy = new sc_signal<bool>;

          in._DATNAME_(*dummy_in_msg);
          in._VLDNAME_(*dummy_in_val);
          in._RDYNAME_(*dummy_in_rdy);

          in.disable_spawn();
        }
      }

      virtual void set_trace(sc_trace_file *trace_file_ptr) {
        sc_trace(trace_file_ptr, parent._VLDNAMEOUT_, parent._VLDNAMEOUT_.name());
        sc_trace(trace_file_ptr, parent._RDYNAMEOUT_, parent._RDYNAMEOUT_.name());

        if (!parent.driver) {
          // this case occurs for "port-less channel access", ie comb_chan.Push(val)
          OutBlocking<Message, DIRECT_PORT> *driver = &(parent.sim_out);
          driver->set_trace(trace_file_ptr, parent._DATNAMEOUT_.name());
        } else {
          OutBlocking<Message, DIRECT_PORT> *driver = parent.driver;
          while (driver->driver)
          { driver = driver->driver; }

          driver->set_trace(trace_file_ptr, parent._DATNAMEOUT_.name());
        }
      }

      bool set_log(std::ofstream *os, int &log_num, std::string &path_name) {
        if (!parent.driver) {
          OutBlocking<Message, DIRECT_PORT> *driver = &(parent.sim_out);

          path_name = parent.name();
          driver->set_log(++log_num, os);
          return true;
        } else {
          OutBlocking<Message, DIRECT_PORT> *driver = parent.driver;
          while (driver->driver)
          { driver = driver->driver; }

          path_name = parent._DATNAMEOUT_.name();
          driver->set_log(++log_num, os);
          return true;
        }
      }

    } dummyPortManager;
#endif
  };


#ifdef CONNECTIONS_SIM_ONLY
  template <typename Message>
  class Combinational <Message, TLM_PORT> :
    public Combinational_Ports_abs<Message>
  , public sc_trace_marker
  , public sc_object
  , public write_log_if<Message>
  {
  public:

    Combinational() : Combinational_Ports_abs<Message>()
      ,fifo(sc_gen_unique_name("fifo"), 2) {}


    explicit Combinational(const char *name) : Combinational_Ports_abs<Message>(name)
      ,fifo(CONNECTIONS_CONCAT(name, "fifo"), 1) {}

    virtual ~Combinational() {}

    // Reset
    void ResetRead() {
      this->read_reset_check.reset(false);
      get_conManager().add_clock_event(this);
      Message temp;
      while (fifo.nb_get(temp));
    }

    void ResetWrite() {
      this->write_reset_check.reset(false);
      get_conManager().add_clock_event(this);
    }

    bool do_reset_check() {
      /*
          this->read_reset_check.check();
          this->write_reset_check.check();
      */
      return 0;
    }

#ifndef __SYNTHESIS__
    std::string report_name() {
      return this->write_reset_check.report_name();
    }
#endif

// Pop
#pragma design modulario < in >
    Message Pop() {
      this->read_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      return fifo.get();
    }

// Peek
#pragma design modulario < in >
    Message Peek() {
      this->read_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      return fifo.peek();
    }

    bool PeekNB(Message &data, const bool &/*unused*/ = true) {
      return fifo.nb_peek(data);
    }
    
// PopNB
#pragma design modulario < in >
    bool PopNB(Message &data) {
      this->read_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      return fifo.nb_get(data);
    }

// Push
#pragma design modulario < out >
    void Push(const Message &m) {
      this->write_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      fifo.put(m);
      write_log(m);
    }

// PushNB
#pragma design modulario < out >
    bool PushNB(const Message &m) {
      this->write_reset_check.check();
#ifdef CONNECTIONS_ACCURATE_SIM
      get_sim_clk().check_on_clock_edge(this->clock_number);
#endif
      bool ret = fifo.nb_put(m);
      if (ret)
       write_log(m);

      return ret;
    }

    virtual void set_trace(sc_trace_file *trace_file_ptr) {}

    virtual bool set_log(std::ofstream *os, int &log_num, std::string &path_name) {
     log_stream = os;
     log_number = ++log_num;
     path_name = fifo.name();
     return 1;
    }

    virtual void write_log(const Message& m) {
      if (log_stream)
       *log_stream << std::dec << log_number << " | " << std::hex <<  m << " | " << sc_time_stamp() << "\n"; 
    }

    std::ofstream *log_stream{0};
    int log_number{0};

  protected:
    void reset_msg() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
    void read_msg(Message &m) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
    void write_msg(const Message &m) {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }
    void invalidate_msg() {
      CONNECTIONS_ASSERT_MSG(0, "Unreachable virtual function in abstract class!");
    }

  public:
    tlm::tlm_fifo<Message> fifo;
  };
#endif // CONNECTIONS_SIM_ONLY

}  // namespace Connections

#endif  // __CONNECTIONS__CONNECTIONS_H__
