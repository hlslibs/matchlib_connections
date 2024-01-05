/**************************************************************************
 *                                                                        *
 *  HLS Connections Library                                               *
 *                                                                        *
 *  Software Version: 2.1                                                 *
 *                                                                        *
 *  Release Date    : Fri Jan  5 08:41:49 PST 2024                        *
 *  Release Type    : Production Release                                  *
 *  Release Build   : 2.1.1                                               *
 *                                                                        *
 *  Copyright 2023 Siemens                                                *
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

// Prototype code - not fully production ready

// Author: Stuart Swan, Platform Architect, Siemens EDA
// Date: 6 Oct 2023

#pragma once

#ifdef __clang__
#ifdef BOOST_PP_VARIADICS
#ifndef OK_BOOST_PASS
#error "For clang++ auto_gen_fields.h must be included before any other includes of boost headers"
#endif
#endif
#define BOOST_PP_VARIADICS 1
#define OK_BOOST_PASS 1
#endif

#include <boost/preprocessor/list/for_each.hpp>
#include <boost/preprocessor/tuple/to_list.hpp>
#include <connections/marshaller.h>
#include <mc_connections.h>
#include <ctime>


struct port_info {
 port_info(std::string t, int w, std::string n) : type(t), width(w), name(n) {}
 std::string type;
 int width{0};
 std::string name;
 std::vector<port_info> child_vec;
};

class gen_port_info_vec_if {
public:
  virtual void gen_port_info_vec(std::vector<port_info>& port_info_vec) = 0;
};

template <class T>
class port_traits
{
public:
  static void gen_info(std::vector<port_info>& vec, std::string nm, T& obj) {
   port_info pi = port_info("{}", 0, nm);
   obj.gen_port_info_vec(pi.child_vec);
   vec.push_back(pi);
  }
};

template <class M>
class port_traits<sc_in<M>>
{
public:
  static constexpr int width = Wrapped<M>::width;
  static constexpr const char* type = "sc_in";
  static void gen_info(std::vector<port_info>& vec, std::string nm, sc_in<M>& obj) {
   vec.push_back(port_info(type, width, nm)); 
  }
};

template <class M>
class port_traits<sc_out<M>>
{
public:
  static constexpr int width = Wrapped<M>::width;
  static constexpr const char* type = "sc_out";
  static void gen_info(std::vector<port_info>& vec, std::string nm, sc_out<M>& obj) {
   vec.push_back(port_info(type, width, nm)); 
  }
};

template <class M>
class port_traits<Connections::In<M>>
{
public:
  static constexpr int width = Wrapped<M>::width;
  static constexpr const char* type = "In";
  static void gen_info(std::vector<port_info>& vec, std::string nm, Connections::In<M>& obj) {
   vec.push_back(port_info(type, width, nm)); 
  }
};

template <class M>
class port_traits<Connections::Out<M>>
{
public:
  static constexpr int width = Wrapped<M>::width;
  static constexpr const char* type = "Out";
  static void gen_info(std::vector<port_info>& vec, std::string nm, Connections::Out<M>& obj) {
   vec.push_back(port_info(type, width, nm)); 
  }
};



#define GEN_PORT_INFO(R, _, F) \
   port_traits<decltype(F)>::gen_info(port_info_vec, #F, this->F); \
  //

#ifndef __SYNTHESIS__
#define GEN_PORT_INFO_VEC(FIELDS) \
  virtual void gen_port_info_vec(std::vector<port_info>& port_info_vec) { \
    BOOST_PP_LIST_FOR_EACH(GEN_PORT_INFO, _, FIELDS) \
  }; \
  //
#else
#define GEN_PORT_INFO_VEC(FIELDS) \
  virtual void gen_port_info_vec(std::vector<port_info>& port_info_vec) { \
  }; \
  //
#endif



#define PORT_LIST(X) BOOST_PP_TUPLE_TO_LIST(BOOST_PP_TUPLE_SIZE(X), X )


#define AUTO_GEN_PORT_INFO(THIS_TYPE, X) \
  GEN_PORT_INFO_VEC(PORT_LIST(X)) \
  //



class auto_gen_wrapper {
public:
  auto_gen_wrapper(std::string nm) : module_name(nm) {}

  std::vector<port_info> port_info_vec;
  std::string module_name;

  void display() {
   for (unsigned i=0; i<port_info_vec.size(); ++i) {
    std::cout << "port: " << port_info_vec[i].type 
              << " " << port_info_vec[i].width << " " << port_info_vec[i].name << "\n";
    for (unsigned z=0; z<port_info_vec[i].child_vec.size(); ++z)
      std::cout << " child port: " <<
        port_info_vec[i].child_vec[z].type << " " << 
        port_info_vec[i].child_vec[z].width << " " << port_info_vec[i].child_vec[z].name << "\n";
   }
  }

  void gen_wrappers(int clkper=10, bool enable_trace=true) {
    time_t now = time(0);
    char* dt = ctime(&now);
    remove_base();
    ofstream sc_cpp;
    sc_cpp.open(module_name + "_wrap.cpp");

    sc_cpp << "// Auto generated on: " << dt << "\n";
    sc_cpp << "#include \"" << module_name + "_wrap.h" << "\"" << "\n" ;
    if (enable_trace)
      sc_cpp << "sc_trace_file* trace_file_ptr;\n";
    sc_cpp << "\n";
    sc_cpp << "#ifdef SC_MODULE_EXPORT\n";
    sc_cpp << "SC_MODULE_EXPORT(" << module_name + "_wrap" << ");\n";
    sc_cpp << "#endif\n";
    sc_cpp.close();

    std::string inst(module_name + "_inst");

    ofstream sc_h;
    sc_h.open(module_name + "_wrap.h");
    sc_h << "// Auto generated on: " << dt << "\n";
    sc_h << "#include \"" << module_name + ".h" << "\"" << "\n\n" ;
    if (enable_trace)
      sc_h << "extern sc_trace_file* trace_file_ptr;\n\n";
    sc_h << "class " << module_name + "_wrap" << " : public sc_module {\n";
    sc_h << "public:\n";
    sc_h << "  " << module_name << " CCS_INIT_S1(" << inst << ");\n\n";
    for (unsigned i=0; i<port_info_vec.size(); ++i) {
     if (port_info_vec[i].type != std::string("{}")) {
      emit_io(sc_h, module_name  + "_inst." + port_info_vec[i].name, port_info_vec[i].name,
              port_info_vec[i].type);
     } else {
      for (unsigned c=0; c < port_info_vec[i].child_vec.size(); ++c) {
        emit_io(sc_h, child_type(i,c, "_inst.") , child_name(i,c) , port_info_vec[i].child_vec[c].type);
      }
     }
    }
    sc_h << "\n  sc_clock connections_clk;\n";
    sc_h << "  sc_event check_event;\n\n";
    sc_h << "  virtual void start_of_simulation() {\n";
    sc_h << "    Connections::get_sim_clk().add_clock_alias(\n";
    sc_h << "      connections_clk.posedge_event(), clk.posedge_event());\n";
    sc_h << "  }\n\n";
    sc_h << "  SC_CTOR(" << module_name << "_wrap) \n";
    sc_h << "  : connections_clk(\"connections_clk\", " << clkper << ", SC_NS, 0.5,0,SC_NS,true)\n";
    sc_h << "  {\n";
    sc_h << "    SC_METHOD(check_clock);\n";
    sc_h << "    sensitive << connections_clk << clk;\n\n";
    sc_h << "    SC_METHOD(check_event_method);\n";
    sc_h << "    sensitive << check_event;\n\n";
    if (enable_trace) {
      sc_h << "    trace_file_ptr = sc_create_vcd_trace_file(\"trace\");\n";
      sc_h << "    trace_hierarchy(this, trace_file_ptr);\n\n";
    }
    for (unsigned i=0; i<port_info_vec.size(); ++i) {
     if (port_info_vec[i].type != std::string("{}")) {
      emit_bind(sc_h, module_name  + "_inst." + port_info_vec[i].name, port_info_vec[i].name,
              port_info_vec[i].type);
     } else {
      for (unsigned c=0; c < port_info_vec[i].child_vec.size(); ++c) {
        emit_bind(sc_h, child_type(i,c, "_inst.") , child_name(i,c) , port_info_vec[i].child_vec[c].type);
      }
     }
    }
    sc_h << "  }\n\n";
    sc_h << "  void check_clock() { check_event.notify(2, SC_PS);} // Let SC and Vlog delta cycles settle.\n\n";

    sc_h << "  void check_event_method() {\n";
    sc_h << "    if (connections_clk.read() == clk.read()) return;\n";
    sc_h << "    CCS_LOG(\"clocks misaligned!:\"  << connections_clk.read() << \" \" << clk.read());\n";
    sc_h << "  }\n";
    sc_h << "};\n";
    sc_h.close();

    ofstream vlog;
    vlog.open(module_name + "_wrap.v");

    std::string prefix("  ");

    vlog << "// Auto generated on: " << dt << "\n\n";
    vlog << "module " << module_name << "(\n";
    for (unsigned i=0; i<port_info_vec.size(); ++i) {
     if (port_info_vec[i].type != std::string("{}")) {
      emit_vlog_name(vlog, prefix, port_info_vec[i].name, port_info_vec[i].type);
     } else {
      for (unsigned c=0; c < port_info_vec[i].child_vec.size(); ++c) {
           emit_vlog_name(vlog, prefix, child_name(i,c), port_info_vec[i].child_vec[c].type);
      }
     }
    }
    vlog << ");\n";
    for (unsigned i=0; i<port_info_vec.size(); ++i) {
     if (port_info_vec[i].type != std::string("{}")) {
      emit_vlog_decl(vlog, port_info_vec[i].name, port_info_vec[i]);
     } else {
      for (unsigned c=0; c < port_info_vec[i].child_vec.size(); ++c) {
        emit_vlog_decl(vlog, child_name(i,c), port_info_vec[i].child_vec[c]);
      }
     }
    }
    vlog << "endmodule;\n";
    vlog.close();
  }


  void emit_vlog_name(ofstream& vlog, std::string& prefix, std::string name, std::string& io_type) {
    if (io_type == "In") {
      vlog << prefix << name << "_" << _RDYNAMESTR_ << "\n";
      prefix = ", ";
      vlog << prefix << name << "_" << _VLDNAMESTR_ << "\n";
      vlog << prefix << name << "_" << _DATNAMESTR_ << "\n";
    } else if (io_type == "Out") {
      vlog << prefix << name << "_" << _RDYNAMESTR_ << "\n";
      prefix = ", ";
      vlog << prefix << name << "_" << _VLDNAMESTR_ << "\n";
      vlog << prefix << name << "_" << _DATNAMESTR_ << "\n";
    } else {
      vlog << prefix << name << "\n";
      prefix = ", ";
    }
  }

  void emit_vlog_decl(ofstream& vlog, std::string name, port_info& pi) {
    int w = pi.width - 1;
    if (pi.type == "In") {
      vlog << "  output " << name << "_" << _RDYNAMESTR_ << ";\n";
      vlog << "  input  " << name << "_" << _VLDNAMESTR_ << ";\n";
      vlog << "  input [" << w << ":0] " << name << "_" << _DATNAMESTR_ << ";\n";
    } else if (pi.type == "Out") {
      vlog << "  input  " << name << "_" << _RDYNAMESTR_ << ";\n";
      vlog << "  output " << name << "_" << _VLDNAMESTR_ << ";\n";
      vlog << "  output [" << w << ":0] " << name << "_" << _DATNAMESTR_ << ";\n";
    } else if (pi.type == "sc_in") {
      vlog << "  input [" << w << ":0] " << name << ";\n";
    } else if (pi.type == "sc_out") {
      vlog << "  output [" << w << ":0] " << name << ";\n";
    }
  }

  void emit_bind(ofstream& sc_h, std::string type, std::string name, std::string& io_type) {
    if (io_type == "In") {
      sc_h << "    " << type << "." << _RDYNAMESTR_ 
           << "(" << name << "_" << _RDYNAMESTR_ << ");\n";
      sc_h << "    " << type << "." << _VLDNAMESTR_ 
           << "(" << name << "_" << _VLDNAMESTR_ << ");\n";
      sc_h << "    " << type << "." << _DATNAMESTR_ 
           << "(" << name << "_" << _DATNAMESTR_ << ");\n";
    } else if (io_type == "Out") {
      sc_h << "    " << type << "." << _RDYNAMESTR_ 
           << "(" << name << "_" << _RDYNAMESTR_ << ");\n";
      sc_h << "    " << type << "." << _VLDNAMESTR_ 
           << "(" << name << "_" << _VLDNAMESTR_ << ");\n";
      sc_h << "    " << type << "." << _DATNAMESTR_ 
           << "(" << name << "_" << _DATNAMESTR_ << ");\n";
    } else {
      sc_h << "    " << type << "(" << name << ");\n";
    }
  }

  void emit_io(ofstream& sc_h, std::string type, std::string name, std::string& io_type) {
    if (io_type == "In") {
      sc_h << "  decltype(" << type << "." << _RDYNAMESTR_ 
           << ")   CCS_INIT_S1(" << name << "_" << _RDYNAMESTR_ << ");\n";
      sc_h << "  decltype(" << type << "." << _VLDNAMESTR_ 
           << ")   CCS_INIT_S1(" << name << "_" << _VLDNAMESTR_ << ");\n";
      sc_h << "  decltype(" << type << "." << _DATNAMESTR_ 
           << ")   CCS_INIT_S1(" << name << "_" << _DATNAMESTR_ << ");\n";
    } else if (io_type == "Out") {
      sc_h << "  decltype(" << type << "." << _RDYNAMESTR_ 
           << ")   CCS_INIT_S1(" << name << "_" << _RDYNAMESTR_ << ");\n";
      sc_h << "  decltype(" << type << "." << _VLDNAMESTR_ 
           << ")   CCS_INIT_S1(" << name << "_" << _VLDNAMESTR_ << ");\n";
      sc_h << "  decltype(" << type << "." << _DATNAMESTR_ 
           << ")   CCS_INIT_S1(" << name << "_" << _DATNAMESTR_ << ");\n";
    } else {
      sc_h << "  decltype(" << type << ")   CCS_INIT_S1(" << name << ");\n";
    }
  }

  std::string child_type(int p, int c, std::string postfix) {
   return module_name + postfix +  port_info_vec[p].name + "." + port_info_vec[p].child_vec[c].name;
  }

  std::string child_name(int p, int c) {
   return port_info_vec[p].name + "_" + port_info_vec[p].child_vec[c].name;
  }

  void remove_base() {
    for (unsigned i=0; i<port_info_vec.size(); ++i) {
     if (port_info_vec[i].type == std::string("{}")) {
      for (unsigned c=0; c < port_info_vec[i].child_vec.size(); ++c) 
       port_info_vec[i].child_vec[c].name = strip_base(port_info_vec[i].child_vec[c].name);
     }
    }
  }

  std::string strip_base(std::string& input) {
    size_t pos = input.find("::");
    if (pos != std::string::npos)
      return input.substr(pos + 2);
    return input;
  }
};
