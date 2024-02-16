/**************************************************************************
 *                                                                        *
 *  HLS Connections Library                                               *
 *                                                                        *
 *  Software Version: 2.1                                                 *
 *                                                                        *
 *  Release Date    : Mon Jan 15 20:15:38 PST 2024                        *
 *  Release Type    : Production Release                                  *
 *  Release Build   : 2.1.1                                               *
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
//*************************************************************************
// File: connections_fifo.h
//
// Description: Connections fifo module.
//   Configurable port names: (rdy/vld/dat) legacy: (rdy/val/msg)
//
// Revision History:
//   2.1.0 - 2023-10-16 - Fixed CAT-34870 - Include iomanip for setw
//   2.1.0 - 2023-10-16 - Add reset polarity check in line_trace()
//   2.1.0 - 2023-10-13 - Fixed CAT-34421
//   1.5.0 - 2023-05-12 - Fixed CAT-33347 - New "type mapping" feature requires fixes to connections_fifo.h
//   Add TLM_PORT specialization that uses sized tlm_fifo.
//
// Origin: Nivdia Matchlib Buffer class.
//*************************************************************************

#ifndef CONNECTIONS_FIFO_H
#define CONNECTIONS_FIFO_H
#ifndef __SYNTHESIS__
#include <iomanip>
#endif
#include "connections.h"

namespace Connections
{

  //------------------------------------------------------------------------
  // Helper class for Wrapped and Direct data
  //------------------------------------------------------------------------
  template <typename Message, connections_port_t port_marshall_type = AUTO_PORT>
  class FifoElem
  {
  public:
    // Interface
    typedef Wrapped<Message> WMessage;
    static const unsigned int width = WMessage::width;
    typedef sc_lv<WMessage::width> MsgBits;
    sc_signal<MsgBits> _DATNAME_;
    uint64 init_val{0};

    FifoElem() : _DATNAME_(sc_gen_unique_name(_DATNAMESTR_)) {}

    FifoElem(sc_module_name name) : _DATNAME_(CONNECTIONS_CONCAT(name, _DATNAMESTR_)) {}

    void reset_state() {
      _DATNAME_.write(init_val);
    }
  };

  template <typename Message>
  class FifoElem<Message, DIRECT_PORT>
  {
  public:
    // Interface
    sc_signal<Message> _DATNAME_;
    Message init_val;

    FifoElem() : _DATNAME_(sc_gen_unique_name(_DATNAMESTR_)) {}

    FifoElem(sc_module_name name) : _DATNAME_(CONNECTIONS_CONCAT(name, _DATNAMESTR_)) {}

    void reset_state() {
      _DATNAME_.write(init_val);
    }
  };

  //------------------------------------------------------------------------
  // Fifo
  //------------------------------------------------------------------------
  template <typename Message, unsigned int NumEntries, connections_port_t port_marshall_type = AUTO_PORT>
  class Fifo : public sc_module
  {
    SC_HAS_PROCESS(Fifo);

  public:
    // Interface
    sc_in_clk clk;
    sc_in<bool> rst;
    In<Message, port_marshall_type> enq;
    Out<Message, port_marshall_type> deq;

    Fifo()
      : sc_module(sc_module_name(sc_gen_unique_name("Fifo")))
      , clk("clk")
      , rst("rst")
      , enq(sc_gen_unique_name("enq"))
      , deq(sc_gen_unique_name("deq"))
    { 
      Init();
    }

    Fifo(sc_module_name name)
      : sc_module(name)
      , clk("clk")
      , rst("rst")
      , enq(CONNECTIONS_CONCAT(name, "enq"))
      , deq(CONNECTIONS_CONCAT(name, "deq"))
    {
      Init();
    }

  protected:
    typedef bool Bit;
    static const int AddrWidth = nbits<NumEntries>::val;
    typedef sc_uint<AddrWidth> BuffIdx;

    // Internal wires
    sc_signal<Bit> full_next;
    sc_signal<BuffIdx> head_next;
    sc_signal<BuffIdx> tail_next;

    // Internal state
    sc_signal<Bit> full;
    sc_signal<BuffIdx> head;
    sc_signal<BuffIdx> tail;
    FifoElem<Message, port_marshall_type> buffer[NumEntries];

    // Helper functions
    void Init() {
      #ifdef CONNECTIONS_SIM_ONLY
      enq.disable_spawn();
      deq.disable_spawn();
      #endif

      SC_METHOD(EnqRdy);
      sensitive << full;

      SC_METHOD(DeqVal);
      sensitive << full << head << tail;

      SC_METHOD(DeqMsg);
      #ifndef __SYNTHESIS__
      sensitive << deq._RDYNAME_ << full << head << tail;
      #else
      sensitive << tail;
      for(int i = 0; i < NumEntries; i++){
        sensitive << buffer[i]._DATNAME_;
      }
      #endif

      SC_METHOD(HeadNext);
      sensitive << enq._VLDNAME_ << full << head;

      SC_METHOD(TailNext);
      sensitive << deq._RDYNAME_ << full << head << tail;

      SC_METHOD(FullNext);
      sensitive << enq._VLDNAME_ << deq._RDYNAME_ << full << head << tail;

      SC_THREAD(Seq);
      sensitive << clk.pos();
      CONNECTIONS_RESET_SIGNAL_IS(rst);;

      // Needed so that DeqMsg always has a good tail value
      tail.write(0);
    }

    // Combinational logic

    // Enqueue ready
    void EnqRdy() { enq._RDYNAME_.write(!full.read()); }

    // Dequeue valid
    void DeqVal() {
      bool empty = (!full.read() && (head.read() == tail.read()));
      deq._VLDNAME_.write(!empty);
    }

    // Dequeue message
    void DeqMsg() {
      #ifndef __SYNTHESIS__
      bool empty = (!full.read() && (head.read() == tail.read()));
      bool do_deq = !empty;
      if (do_deq) {
      #endif
        deq._DATNAME_.write(buffer[tail.read()]._DATNAME_.read());
      #ifndef __SYNTHESIS__
      } else {
        deq._DATNAME_.write(buffer[0].init_val);
      }
      #endif
    }

    // Head next calculations
    void HeadNext() {
      bool do_enq = (enq._VLDNAME_.read() && !full.read());
      BuffIdx head_inc;
      if ((head.read() + 1) == NumEntries)
      { head_inc = 0; }
      else
      { head_inc = head.read() + 1; }

      if (do_enq)
      { head_next.write(head_inc); }
      else
      { head_next.write(head.read()); }
    }

    // Tail next calculations
    void TailNext() {
      bool empty = (!full.read() && (head.read() == tail.read()));
      bool do_deq = (deq._RDYNAME_.read() && !empty);
      BuffIdx tail_inc;
      if ((tail.read() + 1) == NumEntries)
      { tail_inc = 0; }
      else
      { tail_inc = tail.read() + 1; }

      if (do_deq)
      { tail_next.write(tail_inc); }
      else
      { tail_next.write(tail.read()); }
    }

    // Full next calculations
    void FullNext() {
      bool empty = (!full.read() && (head.read() == tail.read()));
      bool do_enq = (enq._VLDNAME_.read() && !full.read());
      bool do_deq = (deq._RDYNAME_.read() && !empty);

      BuffIdx head_inc;
      if ((head.read() + 1) == NumEntries)
      { head_inc = 0; }
      else
      { head_inc = head.read() + 1; }

      if (do_enq && !do_deq && (head_inc == tail.read()))
      { full_next.write(1); }
      else if (do_deq && full.read())
      { full_next.write(0); }
      else
      { full_next.write(full.read()); }
    }

    // Sequential logic
    void Seq() {
      // Reset state
      full.write(0);
      head.write(0);
      tail.write(0);
      #pragma hls_unroll yes
      for (unsigned int i = 0; i < NumEntries; ++i)
      { buffer[i].reset_state(); }

      wait();

      while (1) {
        // Head update
        head.write(head_next);

        // Tail update
        tail.write(tail_next);

        // Full update
        full.write(full_next);

        // Enqueue message
        if (enq._VLDNAME_.read() && !full.read()) {
          buffer[head.read()]._DATNAME_.write(enq._DATNAME_.read());
        }

        wait();
      }
    }

  public:
    #ifndef __SYNTHESIS__
    void line_trace() {
      #ifdef CONNECTIONS_POS_RESET
        bool rst_active = true;
      #else
        bool rst_active = false;
      #endif
      if (rst.read() != rst_active) {
        unsigned int width = (Message().length() / 4);
        // Enqueue port
        if (enq._VLDNAME_.read() && enq._RDYNAME_.read()) {
          std::cout << std::hex << std::setw(width) << enq._DATNAME_.read();
        } else {
          std::cout << std::setw(width + 1) << " ";
        }

        std::cout << " ( " << full.read() << " ) ";

        // Dequeue port
        if (deq._VLDNAME_.read() && deq._RDYNAME_.read()) {
          std::cout << std::hex << std::setw(width) << deq._DATNAME_.read();
        } else {
          std::cout << std::setw(width + 1) << " ";
        }
        std::cout << " | ";
      }
    }
    #endif //__SYNTHESIS__
  };

#ifdef CONNECTIONS_SIM_ONLY
  //------------------------------------------------------------------------
  // Fifo - TLM_PORT specialization uses sized tlm::tlm_fifo
  //------------------------------------------------------------------------
  template <typename Message, unsigned int NumEntries>
  class Fifo<Message, NumEntries, TLM_PORT> : public sc_module
  {
    SC_HAS_PROCESS(Fifo);

  public:
    // Interface (clk and rst not used here, but kept for consistant bindings)
    sc_in_clk clk;
    sc_in<bool> rst;
    In<Message, TLM_PORT> enq;
    Out<Message, TLM_PORT> deq;

    Fifo()
      :sc_module(sc_module_name(sc_gen_unique_name("Fifo")))
      ,clk("clk")
      ,rst("rst")
      ,enq(sc_gen_unique_name("enq"))
      ,deq(sc_gen_unique_name("deq"))
      ,fifo(sc_gen_unique_name("fifo"), NumEntries)
    {
      SC_THREAD(tput);
      SC_THREAD(tget);
    }

    Fifo(sc_module_name name)
      :sc_module(name)
      ,clk("clk") 
      ,rst("rst") 
      ,enq(CONNECTIONS_CONCAT(name, "enq"))
      ,deq(CONNECTIONS_CONCAT(name, "deq"))
      ,fifo(CONNECTIONS_CONCAT(name, "fifo"), NumEntries)
    {
      SC_THREAD(tput);
      SC_THREAD(tget);
    }

    void tput() {
      while (1) {
        fifo.put(enq.Pop());
      }
    }

    void tget() {
      while (1) {
        deq.Push(fifo.get());
      }
    }

  protected:
    tlm::tlm_fifo<Message> fifo;

  public:
    #ifndef __SYNTHESIS__
    void line_trace() {};
    #endif // __SYNTHESIS__

  };
#endif // CONNECTIONS_SIM_ONLY

}  // namespace Connections
#endif  // CONNECTIONS_FIFO_H
