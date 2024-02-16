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

// Author: Stuart Swan, Platform Architect, Siemens EDA
// Date: 22 Dec 2023

//*****************************************************************************************
// File: auto_gen_fields.h
//
// Description: C++ Macros to simplify making user-defined struct types work in Connections
//
// Revision History:
//       2.1.1 - Unspecified changes from Stuart Swan
//             - Fix for CAT-35587 from Stuart Swan
//*****************************************************************************************

#pragma once

#ifdef __clang__
#ifdef BOOST_PP_VARIADICS
#ifndef OK_BOOST_PASS
#error "For clang++ auto_gen_port_info.h must be included before any other includes of boost headers"
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

#include <auto_gen_fields.h>


struct port_info {
 port_info(std::string t, int w, std::string n) : type(t), width(w), name(n) {}
 std::string type;  // "sc_in", "In", etc. "{}" means it is a module (similar to SV modport)
 int width{0};
 std::string name;
 std::vector<port_info> child_vec;  // iff type is "{}" , then child_vec lists its ports
 std::vector<field_info> field_vec; // fields iff this is a struct/class type
};

template <class T>
class port_traits
{
public:
  static void gen_info(std::vector<port_info>& vec, std::string nm, T& obj) {
   port_info pi = port_info("{}", 0, nm); // this is like a SV modport
   obj.gen_port_info_vec(pi.child_vec);
   vec.push_back(pi);
   // call_gen_field_info<M>::gen_field_info(vec.back().field_vec);
  }
};

template <class T, int N>
class port_traits<T[N]>
{
public:
  static void gen_info(std::vector<port_info>& vec, std::string nm, T* obj) {
   for (unsigned u=0; u < N; ++u) {
     std::ostringstream os;
     os << nm << "_" << u;
     port_info pi = port_info("{}", 0, os.str()); // this is like a SV modport
     obj->gen_port_info_vec(pi.child_vec);
     vec.push_back(pi);
     // call_gen_field_info<M>::gen_field_info(vec.back().field_vec);
   }
  }
};

template <class T, int X, int Y>
class port_traits<T[X][Y]>
{
public:
  static void gen_info(std::vector<port_info>& vec, std::string nm, T obj[X][Y]) {
   for (unsigned x=0; x < X; ++x) {
    for (unsigned y=0; y < Y; ++y) {
     std::ostringstream os;
     os << nm << "_" << x << "_" << y;
     port_info pi = port_info("{}", 0, os.str()); // this is like a SV modport
     (obj[0][0]).gen_port_info_vec(pi.child_vec);
     vec.push_back(pi);
     // call_gen_field_info<M>::gen_field_info(vec.back().field_vec);
    }
   }
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
   call_gen_field_info<M>::gen_field_info(vec.back().field_vec);
  }
};
 
template <class M, int N>
class port_traits<sc_in<M>[N]>
{
public:
  static constexpr int width = Wrapped<M>::width;
  static constexpr const char* type = "sc_in";
  static void gen_info(std::vector<port_info>& vec, std::string nm, sc_in<M>* obj) {
   for (unsigned u=0; u < N; ++u) {
     std::ostringstream os;
     os << nm << "_" << u;
     vec.push_back(port_info(type, width, os.str())); 
     call_gen_field_info<M>::gen_field_info(vec.back().field_vec);
   }
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
   call_gen_field_info<M>::gen_field_info(vec.back().field_vec);
  }
};
 
template <class M, int N>
class port_traits<sc_out<M>[N]>
{
public:
  static constexpr int width = Wrapped<M>::width;
  static constexpr const char* type = "sc_out";
  static void gen_info(std::vector<port_info>& vec, std::string nm, sc_out<M>* obj) {
   for (unsigned u=0; u < N; ++u) {
     std::ostringstream os;
     os << nm << "_" << u;
     vec.push_back(port_info(type, width, os.str())); 
     call_gen_field_info<M>::gen_field_info(vec.back().field_vec);
   }
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
   call_gen_field_info<M>::gen_field_info(vec.back().field_vec);
  }
};
 
template <class M, int N>
class port_traits<Connections::In<M>[N]>
{
public:
  static constexpr int width = Wrapped<M>::width;
  static constexpr const char* type = "In";
  static void gen_info(std::vector<port_info>& vec, std::string nm, Connections::In<M>* obj) {
   for (unsigned u=0; u < N; ++u) {
     std::ostringstream os;
     os << nm << "_" << u;
     vec.push_back(port_info(type, width, os.str())); 
     call_gen_field_info<M>::gen_field_info(vec.back().field_vec);
   }
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
   call_gen_field_info<M>::gen_field_info(vec.back().field_vec);
  }
};

template <class M, int N>
class port_traits<Connections::Out<M>[N]>
{
public:
  static constexpr int width = Wrapped<M>::width;
  static constexpr const char* type = "Out";
  static void gen_info(std::vector<port_info>& vec, std::string nm, Connections::Out<M>* obj) {
   for (unsigned u=0; u < N; ++u) {
     std::ostringstream os;
     os << nm << "_" << u;
     vec.push_back(port_info(type, width, os.str())); 
     call_gen_field_info<M>::gen_field_info(vec.back().field_vec);
   }
  }
};



#define GEN_PORT_INFO(R, _, F) \
   port_traits<decltype(F)>::gen_info(port_info_vec, #F, F); \
  //

#ifdef CCS_SYSC
#define GEN_PORT_INFO_VEC(FIELDS) // nothing
#else
#define GEN_PORT_INFO_VEC(FIELDS) \
  virtual void gen_port_info_vec(std::vector<port_info>& port_info_vec) { \
    BOOST_PP_LIST_FOR_EACH(GEN_PORT_INFO, _, FIELDS) \
  }; \
  //

#endif


#define PORT_LIST(X) BOOST_PP_TUPLE_TO_LIST(BOOST_PP_TUPLE_SIZE(X), X )


#define AUTO_GEN_PORT_INFO(THIS_TYPE, X) \
  GEN_PORT_INFO_VEC(PORT_LIST(X)) \
  //



class auto_gen_split_wrap {
public:
  auto_gen_split_wrap (std::string nm) : module_name(nm) {}

  std::vector<port_info> port_info_vec;
  std::string module_name;

  void emit_field_vec(ostream& os, std::vector<field_info>& v) {
    if (v.size()) {
     os << "{\n";
       for (unsigned z=0; z< v.size(); ++z)
         v[z].stream_indent(os, " ");
     os << "}\n";
    }
  }

  void emit(ostream& os) {
   for (unsigned i=0; i<port_info_vec.size(); ++i) {
    os << "port: " << port_info_vec[i].type 
              << " " << port_info_vec[i].width << " " << port_info_vec[i].name << "\n";
    emit_field_vec(std::cout, port_info_vec[i].field_vec);
   }
  }

  std::string vlog_size(int s) {
    std::ostringstream os;
    os << " [" << s-1 << ":0] ";

    if (s == 1)
      return("  ");

    return os.str();
  }

  void emit_split_ports_fields(ostream& os, 
          std::string prefix, std::vector<field_info>& v, std::string io) {
    for (unsigned i=0; i<v.size(); ++i)
      if (!v[i].fields.size()) {
        if ((v[i].dim1 == 0) && (v[i].dim0 == 0))
          os << io << vlog_size(v[i].width) << prefix + "_" << v[i].name << ";\n";
        else if (v[i].dim1 == 0) {
          for (unsigned dim0=0; dim0 < v[i].dim0; ++dim0)
            os << io << vlog_size(v[i].width) << prefix + "_" << v[i].name << "_" << dim0 << ";\n";
        } else {
          for (unsigned dim1=0; dim1 < v[i].dim1; ++dim1)
            for (unsigned dim0=0; dim0 < v[i].dim0; ++dim0)
              os << io << vlog_size(v[i].width) << prefix + "_" << v[i].name << "_" << dim1 << "_" << dim0 << ";\n";
        }
      }
      else {
        if ((v[i].dim1 == 0) && (v[i].dim0 == 0))
          emit_split_ports_fields(os, prefix + "_" + v[i].name, v[i].fields, io);
        else if (v[i].dim1 == 0) {
          for (unsigned dim0=0; dim0 < v[i].dim0; ++dim0)
            emit_split_ports_fields(os, prefix + "_" + v[i].name + "_" + std::to_string(dim0), v[i].fields, io);
 
        } else {
          for (unsigned dim1=0; dim1 < v[i].dim1; ++dim1)
            for (unsigned dim0=0; dim0 < v[i].dim0; ++dim0)
            emit_split_ports_fields(os, prefix + "_" + v[i].name + "_" + std::to_string(dim1) + "_" + std::to_string(dim0), v[i].fields, io);
        }
      }
  }

  void emit_split_ports(ostream& os, std::vector<port_info>& pi_vec, std::string prefix = "") {
   for (unsigned i=0; i<pi_vec.size(); ++i) {
    std::string io = "input ";
    if (pi_vec[i].type == "sc_out")
      io = "output ";
    if (pi_vec[i].type == "Out")
      io = "output ";

    if (pi_vec[i].type == "In") {
      os << "output  " << prefix << pi_vec[i].name << "_" << _RDYNAMESTR_ << ";\n";
      os << "input   " << prefix << pi_vec[i].name << "_" << _VLDNAMESTR_ << ";\n";
    }

    if (pi_vec[i].type == "Out") {
      os << "input   " << prefix << pi_vec[i].name << "_" << _RDYNAMESTR_ << ";\n";
      os << "output  " << prefix << pi_vec[i].name << "_" << _VLDNAMESTR_ << ";\n";
    }

    if (pi_vec[i].type == "{}") {
      emit_split_ports(os, pi_vec[i].child_vec, prefix + pi_vec[i].name + "_");
      continue;
    }

    if (!pi_vec[i].field_vec.size()) {
      os << io << vlog_size(pi_vec[i].width) << prefix << pi_vec[i].name ;
      if ((pi_vec[i].type == "In") || (pi_vec[i].type == "Out"))
        os << "_" << _DATNAMESTR_;
      os << ";\n";
    } else {
      emit_split_ports_fields(os, prefix + pi_vec[i].name, pi_vec[i].field_vec, io);
    }
   }
  }


  void emit_split_ports_fields_bare(ostream& os, 
          std::string prefix, std::vector<field_info>& v, std::string& comma) {
    for (unsigned i=0; i<v.size(); ++i)
      if (!v[i].fields.size()) {
        if ((v[i].dim1 == 0) && (v[i].dim0 == 0)) {
          os << comma << prefix + "_" << v[i].name << "\n";
          comma = ", ";
        } else if (v[i].dim1 == 0) {
          for (unsigned dim0=0; dim0 < v[i].dim0; ++dim0) {
            os << comma << prefix + "_" << v[i].name << "_" << dim0 << "\n";
            comma = ", ";
          }
        } else {
          for (unsigned dim1=0; dim1 < v[i].dim1; ++dim1)
            for (unsigned dim0=0; dim0 < v[i].dim0; ++dim0) {
              os << comma << prefix + "_" << v[i].name << "_" << dim1 << "_" << dim0 << "\n";
              comma = ", ";
            }
        }
      }
      else {
        if ((v[i].dim1 == 0) && (v[i].dim0 == 0)) {
         emit_split_ports_fields_bare(os, prefix + "_" + v[i].name, v[i].fields, comma);
        } else if (v[i].dim1 == 0) {
          for (unsigned dim0=0; dim0 < v[i].dim0; ++dim0) {
            emit_split_ports_fields_bare(os, prefix + "_" + v[i].name + "_" + std::to_string(dim0),
              v[i].fields, comma);
          }
        } else {
          for (unsigned dim1=0; dim1 < v[i].dim1; ++dim1)
            for (unsigned dim0=0; dim0 < v[i].dim0; ++dim0) {
             emit_split_ports_fields_bare(os, prefix + "_" + v[i].name + "_" + 
               std::to_string(dim1) + "_" + std::to_string(dim0), v[i].fields, comma);
            }
        }
      }
  }

  void emit_split_ports_bare(ostream& os, std::string& comma, 
                      std::vector<port_info>& pi_vec, std::string prefix="") {
   for (unsigned i=0; i<pi_vec.size(); ++i) {
    if (pi_vec[i].type == "In") {
      os << comma << prefix << pi_vec[i].name << "_" << _RDYNAMESTR_ << "\n";
      comma = ", ";
      os << comma << prefix << pi_vec[i].name << "_" << _VLDNAMESTR_ << "\n";
    }

    if (pi_vec[i].type == "Out") {
      os << comma << prefix << pi_vec[i].name << "_" << _RDYNAMESTR_ << "\n";
      comma = ", ";
      os << comma << prefix << pi_vec[i].name << "_" << _VLDNAMESTR_ << "\n";
    }

    if (pi_vec[i].type == "{}") {
      emit_split_ports_bare(os, comma, pi_vec[i].child_vec, prefix + pi_vec[i].name + "_");
      continue;
    }

    if (!pi_vec[i].field_vec.size()) {
      os << comma << prefix << pi_vec[i].name;
      if ((pi_vec[i].type == "In") || (pi_vec[i].type == "Out"))
        os << "_" << _DATNAMESTR_;
      os << "\n";
      comma = ", ";
    }
    else {
      emit_split_ports_fields_bare(os, prefix + pi_vec[i].name, pi_vec[i].field_vec, comma);
    }
   }
  }

  void emit_bindings_fields(ostream& os, 
          std::string prefix, std::vector<field_info>& v, std::string& comma) {
    for (int i=v.size() - 1; i >= 0; --i) {   // descending order to get bit order correct
      if (!v[i].fields.size()) {
        if ((v[i].dim1 == 0) && (v[i].dim0 == 0)) {
          os << comma << prefix + "_" << v[i].name << "\n";
          comma = ", ";
        } else if (v[i].dim1 == 0) {
          for (int dim0=v[i].dim0 - 1; dim0 >= 0; --dim0) {  // descending order ..
            os << comma << prefix + "_" << v[i].name << "_" << dim0 << "\n";
            comma = ", ";
          }
        } else {
          for (int dim1=v[i].dim1 - 1; dim1 >= 0; --dim1)  // descending order ..
            for (int dim0=v[i].dim0 - 1; dim0 >= 0; --dim0) {
              os << comma << prefix + "_" << v[i].name << "_" << dim1 << "_" << dim0 << "\n";
              comma = ", ";
            }
        }
      }
      else {
        if ((v[i].dim1 == 0) && (v[i].dim0 == 0)) {
          emit_bindings_fields(os, prefix + "_" + v[i].name, v[i].fields, comma);
        } else if (v[i].dim1 == 0) {
          for (int dim0=v[i].dim0 - 1; dim0 >= 0; --dim0) {  // descending order ..
            emit_bindings_fields(os, prefix + "_" + v[i].name + "_" + std::to_string(dim0),
               v[i].fields, comma);
          }
        } else {
          for (int dim1=v[i].dim1 - 1; dim1 >= 0; --dim1)  // descending order ..
            for (int dim0=v[i].dim0 - 1; dim0 >= 0; --dim0) {
            emit_bindings_fields(os, prefix + "_" + v[i].name + "_" + std::to_string(dim1) +
               "_" + std::to_string(dim0), v[i].fields, comma);
            }
        }
      }
    }
  }

  void emit_bindings(ostream& os, std::string& comma, std::vector<port_info>& pi_vec, std::string prefix="") {
   for (unsigned i=0; i<pi_vec.size(); ++i) {
    if (pi_vec[i].type == "In") {
      std::stringstream ss;
      ss << prefix << pi_vec[i].name << "_" << _RDYNAMESTR_;
      os << comma << "." << ss.str() << "(" << ss.str() << ")\n";
      comma = ", ";
      ss.str("");
      ss << prefix << pi_vec[i].name << "_" << _VLDNAMESTR_;
      os << comma << "." << ss.str() << "(" << ss.str() << ")\n";
    }

    if (pi_vec[i].type == "Out") {
      std::stringstream ss;
      ss << prefix << pi_vec[i].name << "_" << _RDYNAMESTR_;
      os << comma << "." << ss.str() << "(" << ss.str() << ")\n";
      comma = ", ";
      ss.str("");
      ss << prefix << pi_vec[i].name << "_" << _VLDNAMESTR_;
      os << comma << "." << ss.str() << "(" << ss.str() << ")\n";
    }

    if (pi_vec[i].type == "{}") {
      emit_bindings(os, comma, pi_vec[i].child_vec, prefix + pi_vec[i].name + "_");
      continue;
    }

    if (!pi_vec[i].field_vec.size()) {
      std::stringstream ss;
      ss << prefix << pi_vec[i].name ;
      if ((pi_vec[i].type == "In") || (pi_vec[i].type == "Out"))
        ss << "_" << _DATNAMESTR_;
      os << comma << "." << ss.str() << "(" << ss.str() << ")\n";
      comma = ", ";
    }
    else {
      std::string postfix = "";
      if ((pi_vec[i].type == "In") || (pi_vec[i].type == "Out"))
        postfix = std::string("_") + _DATNAMESTR_;
      os << comma << "." << prefix << pi_vec[i].name << postfix << "({\n";
      comma = ", ";
      std::string concat_comma = "  ";
      emit_bindings_fields(os, prefix + pi_vec[i].name, pi_vec[i].field_vec, concat_comma);
      os << "})\n";
    }
   }
  }

  void gen_wrapper() {
    time_t now = time(0);
    char* dt = ctime(&now);
    remove_base();
    ofstream vlog;
    vlog.open(module_name + "_split_wrap.v");
    std::cout << "Generating " << module_name << "_split_wrap.v" << "\n";

    vlog << "// Auto generated on: " << dt << "\n";
    vlog << "// This wraps the Verilog RTL produced by HLS and splits all the ports and fields\n";
    vlog << "// into individual input and output ports in Verilog\n\n";
    vlog << "module " << module_name << "_wrap(\n";
    std::string comma = "  ";
    emit_split_ports_bare(vlog, comma, port_info_vec);
    vlog << ");\n\n" ;
    emit_split_ports(vlog, port_info_vec);
    vlog << "\n" ;
    vlog << module_name << " " << module_name << "_inst (\n";
    comma = "  ";
    emit_bindings(vlog, comma, port_info_vec);
    vlog << ");\n\n";
    vlog << "endmodule\n" ;
    vlog.close();
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

class auto_gen_wrapper {
public:
  auto_gen_wrapper(std::string nm) : module_name(nm) {}

  std::vector<port_info> port_info_vec;
  std::string module_name;

  void emit() {
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
    std::cout << "Generating " << module_name << "_wrap.cpp" << "\n";

    sc_cpp << "// Auto generated on: " << dt << "\n";
    sc_cpp << "// This file uses SC_MODULE_EXPORT to export a SC wrapper to HDL simulators\n\n";
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
    std::cout << "Generating " << module_name << "_wrap.h" << "\n";
    sc_h << "// Auto generated on: " << dt << "\n";
    sc_h << "// This file is an SC wrapper of the pre-HLS model to an HDL simulator\n\n";
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
    std::cout << "Generating " << module_name << "_wrap.v" << "\n";

    std::string prefix("  ");

    vlog << "// Auto generated on: " << dt << "\n\n";
    vlog << "// This file shows the Verilog input/output declarations for the exported wrapped SC model.\n";
    vlog << "// This file is only for documentation purposes.\n\n";
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
