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

//*****************************************************************************************
// File: auto_gen_fields.h
//
// Description: C++ Macros to simplify making user-defined struct types work in Connections
//
// Revision History:
//       2.2.2 - Fix CAT-39602 - Changes from Stuart Swan
//       2.2.1 - Fix CAT-38256 - Clean up compiler warnings about extra semicolons
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
 port_info(std::string t, int w, std::string n, bool i=0, bool o=0) 
  : type(t), width(w), name(n), is_sc_in_bool(i), is_sc_out_bool(o) {}
 std::string type;  // "sc_in", "In", etc. "{}" means it is a module (similar to SV modport)
 int width{0};
 std::string name;
 bool is_sc_in_bool{0}; // true iff is sc_in<bool>
 bool is_sc_out_bool{0}; // true iff is sc_out<bool>
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

template <>
class port_traits<sc_in<bool>>
{
public:
  static constexpr int width = Wrapped<bool>::width;
  static constexpr const char* type = "sc_in";
  static void gen_info(std::vector<port_info>& vec, std::string nm, sc_in<bool>& obj) {
   vec.push_back(port_info(type, width, nm, 1)); 
   call_gen_field_info<bool>::gen_field_info(vec.back().field_vec);
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

template <>
class port_traits<sc_out<bool>>
{
public:
  static constexpr int width = Wrapped<bool>::width;
  static constexpr const char* type = "sc_out";
  static void gen_info(std::vector<port_info>& vec, std::string nm, sc_out<bool>& obj) {
   vec.push_back(port_info(type, width, nm, 0, 1)); 
   call_gen_field_info<bool>::gen_field_info(vec.back().field_vec);
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


template <>
class port_traits<Connections::SyncIn>
{
public:
  static constexpr int width = 0;
  static constexpr const char* type = "In";
  static void gen_info(std::vector<port_info>& vec, std::string nm, Connections::SyncIn& obj) {
   vec.push_back(port_info(type, width, nm)); 
  }
};

template <>
class port_traits<Connections::SyncOut>
{
public:
  static constexpr int width = 0;
  static constexpr const char* type = "Out";
  static void gen_info(std::vector<port_info>& vec, std::string nm, Connections::SyncOut& obj) {
   vec.push_back(port_info(type, width, nm)); 
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
  } \
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
  struct port_name {
     std::string dotted_name;
     std::string flat_name;
     std::string io;
     std::string type_info_name;
     std::string sig_name;
     bool is_sc_in_bool{0};
     bool is_sc_out_bool{0};
     int width{0};
  };
  std::vector<port_name> port_name_vec;

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

  void gen_wrappers(double clkper=10, bool enable_trace=true, double clk_offset=0) {
    time_t now = time(0);
    char* dt = ctime(&now);
    remove_base();
    port_name_vec.clear();
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
    sc_h << "  #include \"" << "mc_toolkit_utils.h" << "\"" << "\n" ;
    sc_h << "public:\n";
    sc_h << "  " << module_name << " CCS_INIT_S1(" << inst << ");\n\n";
    sc_h << "  template <class T> struct type_info { };\n\n";
    sc_h << "  template <class T> struct type_info<sc_in<T>> {\n";
    sc_h << "    typedef T data_type;\n";
    sc_h << "    static const int width = Wrapped<data_type>::width;\n";
    sc_h << "    typedef sc_lv<width> sc_lv_type;\n";
    sc_h << "    static const bool is_sc_out = 0;\n";
    sc_h << "  };\n\n";
    sc_h << "  template <class T> struct type_info<sc_out<T>> {\n";
    sc_h << "    typedef T data_type;\n";
    sc_h << "    static const int width = Wrapped<data_type>::width;\n";
    sc_h << "    typedef sc_lv<width> sc_lv_type;\n";
    sc_h << "    static const bool is_sc_out = 1;\n";
    sc_h << "  };\n\n";

    for (unsigned i=0; i<port_info_vec.size(); ++i) {
     if (port_info_vec[i].type != std::string("{}")) {
      gen_name(port_name_vec, module_name  + "_inst." + port_info_vec[i].name, port_info_vec[i].name,
              port_info_vec[i].type, port_info_vec[i].width);
      if (port_info_vec[i].is_sc_in_bool)
        port_name_vec.back().is_sc_in_bool = 1;
      if (port_info_vec[i].is_sc_out_bool)
        port_name_vec.back().is_sc_out_bool = 1;
     } else {
      for (unsigned c=0; c < port_info_vec[i].child_vec.size(); ++c) {
        gen_name(port_name_vec, 
          child_type(i,c, "_inst.") , child_name(i,c) , port_info_vec[i].child_vec[c].type,
          port_info_vec[i].child_vec[c].width);
        if (port_info_vec[i].child_vec[c].is_sc_in_bool)
          port_name_vec.back().is_sc_in_bool = 1;
        if (port_info_vec[i].child_vec[c].is_sc_out_bool)
          port_name_vec.back().is_sc_out_bool = 1;
      }
     }
    }

    for (unsigned i=0; i<port_name_vec.size(); ++i) {
      port_name& n = port_name_vec[i];
      n.type_info_name = std::string("type_info_") + n.flat_name;
      n.sig_name = std::string("sig_") + n.flat_name;
      sc_h << "  typedef type_info<decltype(" << n.dotted_name << ")> " << n.type_info_name << ";\n";
    }

    sc_h << "\n";

    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     if (n.is_sc_in_bool)
      sc_h << "  sc_in<bool> CCS_INIT_S1(" << n.flat_name << ");\n";
     else if (n.is_sc_out_bool)
      sc_h << "  sc_out<bool> CCS_INIT_S1(" << n.flat_name << ");\n";
     else
      sc_h << "  " << n.io << "<sc_lv<" << n.type_info_name << "::width>> CCS_INIT_S1(" 
	<< n.flat_name << ");\n";

    }

    sc_h << "\n";

    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     if (!n.is_sc_in_bool && !n.is_sc_out_bool)
      sc_h << "  sc_signal<" << n.type_info_name << "::data_type> CCS_INIT_S1(" << n.sig_name << ");\n";
    }

    sc_h << "\n  sc_clock connections_clk;\n";
    sc_h << "  sc_event check_event;\n\n";
    sc_h << "  virtual void start_of_simulation() {\n";
    sc_h << "    Connections::get_sim_clk().add_clock_alias(\n";
    sc_h << "      connections_clk.posedge_event(), clk.posedge_event());\n";
    sc_h << "  }\n\n";
    sc_h << "  SC_CTOR(" << module_name << "_wrap) \n";
    sc_h << "  : connections_clk(\"connections_clk\", " << clkper << ", SC_NS, 0.5, " << clk_offset << ",SC_NS,true)\n";
    sc_h << "  {\n";
    sc_h << "    SC_METHOD(check_clock);\n";
    sc_h << "    sensitive << connections_clk << clk;\n\n";
    sc_h << "    SC_METHOD(check_event_method);\n";
    sc_h << "    sensitive << check_event;\n\n";
    if (enable_trace) {
      sc_h << "    trace_file_ptr = sc_create_vcd_trace_file(\"trace\");\n";
      sc_h << "    trace_hierarchy(this, trace_file_ptr);\n\n";
    }


    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     if (!n.is_sc_in_bool && !n.is_sc_out_bool) {
      if (n.io == "sc_out") {
        sc_h << "    SC_METHOD(method_" << n.flat_name << "); sensitive << " << n.sig_name << ";\n";
      } else {
        sc_h << "    SC_METHOD(method_" << n.flat_name << "); sensitive << " << n.flat_name << "; dont_initialize();\n";
      }
     }
    }

    sc_h << "\n";

    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     if (n.is_sc_in_bool || n.is_sc_out_bool)
      sc_h << "    " << n.dotted_name << "(" << n.flat_name << ");\n";
     else
      sc_h << "    " << n.dotted_name << "(" << n.sig_name << ");\n";
    }

    sc_h << "  }\n\n";
    sc_h << "  void check_clock() { check_event.notify(2, SC_PS);} // Let SC and Vlog delta cycles settle.\n\n";

    sc_h << "  void check_event_method() {\n";
    sc_h << "    if (connections_clk.read() == clk.read()) return;\n";
    sc_h << "    CCS_LOG(\"clocks misaligned!:\"  << connections_clk.read() << \" \" << clk.read());\n";
    sc_h << "  }\n";

    sc_h << "\n";

    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     if (!n.is_sc_in_bool && !n.is_sc_out_bool) {
      sc_h << "  void method_" << n.flat_name << "(){\n";
      sc_h << "    typename " << n.type_info_name << "::data_type obj;\n";
      sc_h << "    typename " << n.type_info_name << "::sc_lv_type lv;\n";
      if (n.io == "sc_in") {
        sc_h << "    lv = " << n.flat_name << ".read();\n";
        sc_h << "    obj = BitsToType<decltype(obj)>(lv);\n";
        sc_h << "    " << n.sig_name << " = obj;\n";
      } else {
        sc_h << "    obj = " << n.sig_name << ".read();\n";
        sc_h << "    lv = TypeToBits(obj);\n";
        sc_h << "    " << n.flat_name << " = lv;\n";
      }
      sc_h << "  }\n";
     }
    }

    sc_h << "\n";
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
      emit_vlog_name(vlog, prefix, port_info_vec[i].name, port_info_vec[i]);
     } else {
      for (unsigned c=0; c < port_info_vec[i].child_vec.size(); ++c) {
           emit_vlog_name(vlog, prefix, child_name(i,c), port_info_vec[i].child_vec[c]);
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

    prefix = "  ";

    vlog.open(module_name + "_wrap.sv");
    std::cout << "Generating " << module_name << "_wrap.sv" << "\n";

    vlog << "// Auto generated on: " << dt << "\n\n";
    vlog << "// This file shows the SystemVerilog input/output declarations for the exported wrapped SC model.\n";
    vlog << "// This file is only for documentation purposes.\n\n";

    for (unsigned i=0; i<port_info_vec.size(); ++i) {
     if (port_info_vec[i].type != std::string("{}")) {
      emit_systemvlog_type_decl(vlog, port_info_vec[i].name, port_info_vec[i]);
     } else {
      for (unsigned c=0; c < port_info_vec[i].child_vec.size(); ++c) {
        emit_systemvlog_type_decl(vlog, child_name(i,c), port_info_vec[i].child_vec[c]);
      }
     }
    }
    
    vlog << "\n";

    vlog << "module " << module_name << "(\n";
    for (unsigned i=0; i<port_info_vec.size(); ++i) {
     if (port_info_vec[i].type != std::string("{}")) {
      emit_vlog_name(vlog, prefix, port_info_vec[i].name, port_info_vec[i]);
     } else {
      for (unsigned c=0; c < port_info_vec[i].child_vec.size(); ++c) {
           emit_vlog_name(vlog, prefix, child_name(i,c), port_info_vec[i].child_vec[c]);
      }
     }
    }
    vlog << ");\n";
    for (unsigned i=0; i<port_info_vec.size(); ++i) {
     if (port_info_vec[i].type != std::string("{}")) {
      emit_systemvlog_decl(vlog, port_info_vec[i].name, port_info_vec[i]);
     } else {
      for (unsigned c=0; c < port_info_vec[i].child_vec.size(); ++c) {
        emit_systemvlog_decl(vlog, child_name(i,c), port_info_vec[i].child_vec[c]);
      }
     }
    }
    vlog << "endmodule;\n";
    vlog.close();

  }


  void emit_vlog_name(ofstream& vlog, std::string& prefix, std::string name, port_info& pi) {
    std::string io_type = pi.type;
    if (io_type == "In") {
      vlog << prefix << name << "_" << _RDYNAMESTR_ << "\n";
      prefix = ", ";
      vlog << prefix << name << "_" << _VLDNAMESTR_ << "\n";
      if (pi.width)
      vlog << prefix << name << "_" << _DATNAMESTR_ << "\n";
    } else if (io_type == "Out") {
      vlog << prefix << name << "_" << _RDYNAMESTR_ << "\n";
      prefix = ", ";
      vlog << prefix << name << "_" << _VLDNAMESTR_ << "\n";
      if (pi.width)
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
      if (pi.width)
      vlog << "  input [" << w << ":0] " << name << "_" << _DATNAMESTR_ << ";\n";
    } else if (pi.type == "Out") {
      vlog << "  input  " << name << "_" << _RDYNAMESTR_ << ";\n";
      vlog << "  output " << name << "_" << _VLDNAMESTR_ << ";\n";
      if (pi.width)
      vlog << "  output [" << w << ":0] " << name << "_" << _DATNAMESTR_ << ";\n";
    } else if (pi.type == "sc_in") {
      vlog << "  input [" << w << ":0] " << name << ";\n";
    } else if (pi.type == "sc_out") {
      vlog << "  output [" << w << ":0] " << name << ";\n";
    }
  }

  void emit_systemvlog_decl(ofstream& vlog, std::string name, port_info& pi) {
    int w = pi.width - 1;
    std::string type_str = std::string("[") + std::to_string(w) + ":0] ";

    if (pi.field_vec.size()) {
      type_str = name; 
      if ((pi.type == "In") || (pi.type == "Out")) 
        type_str += std::string("_") + _DATNAMESTR_;
      type_str += "_type ";
    }

    if (pi.type == "In") {
      vlog << "  output " << name << "_" << _RDYNAMESTR_ << ";\n";
      vlog << "  input  " << name << "_" << _VLDNAMESTR_ << ";\n";
      if (pi.width)
      vlog << "  input  " << type_str << name << "_" << _DATNAMESTR_ << ";\n";
    } else if (pi.type == "Out") {
      vlog << "  input  " << name << "_" << _RDYNAMESTR_ << ";\n";
      vlog << "  output " << name << "_" << _VLDNAMESTR_ << ";\n";
      if (pi.width)
      vlog << "  output " << type_str << name << "_" << _DATNAMESTR_ << ";\n";
    } else if (pi.type == "sc_in") {
      vlog << "  input " << type_str << name << ";\n";
    } else if (pi.type == "sc_out") {
      vlog << "  output " << type_str << name << ";\n";
    }
  }

  void emit_sv_type_decl_helper(field_info& fi, ostream &os, std::string indent, std::string nm) { 
    std::string dims = "";
    int dims_mult = 1;
    if (fi.dim1) {
      dims += std::string("[") + std::to_string(fi.dim1-1) + ":0] ";
      dims_mult *= fi.dim1;
    }
    if (fi.dim0) {
      dims += std::string("[") + std::to_string(fi.dim0-1) + ":0] ";
      dims_mult *= fi.dim0;
    }

    if (fi.fields.size()) {
     os << indent << "typedef struct packed {\n";
     for (int i=fi.fields.size() - 1; i >= 0; --i)
       emit_sv_type_decl_helper(fi.fields[i], os, indent + " ", nm + "_" + fi.fields[i].name);
     os << indent << "} " << nm << "_type; // width: " << fi.width << "\n";
     os << indent << "" << dims << nm << "_type" << " " << fi.name << "; // width: " << fi.width << "\n";
    } else {
     os << indent << "reg " << dims << " [" << fi.width - 1 << ":0] " 
        << fi.name << "; // width: " << dims_mult * fi.width << "\n";
    }
  }

  void emit_systemvlog_type_decl(ofstream& vlog, std::string name, port_info& pi) {
    if (pi.field_vec.size()) {
      std::string type_str = name; 
      if ((pi.type == "In") || (pi.type == "Out")) 
        type_str += std::string("_") + _DATNAMESTR_;

      vlog << "typedef struct packed {\n";
      for (int i=pi.field_vec.size() - 1; i >= 0; --i) {
        emit_sv_type_decl_helper(pi.field_vec[i], vlog, "  ", pi.field_vec[i].name);
      }
      vlog << "} " << type_str + "_type" << "; // width: " << pi.width << "\n\n";
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

  void gen_name(std::vector<port_name>& vec, std::string type, std::string name, std::string& io_type, int width) {
    if (io_type == "In") {
      port_name n; 
      n.dotted_name = type + "." + _RDYNAMESTR_;
      n.flat_name = name + "_" + _RDYNAMESTR_;
      n.io = "sc_out";
      n.is_sc_out_bool = 1;
      n.width = 1;
      vec.push_back(n);

      n = port_name();

      n.dotted_name = type + "." + _VLDNAMESTR_;
      n.flat_name = name + "_" + _VLDNAMESTR_;
      n.io = "sc_in";
      n.is_sc_in_bool = 1;
      n.width = 1;
      vec.push_back(n);

      n = port_name();

      n.dotted_name = type + "." + _DATNAMESTR_;
      n.flat_name = name + "_" + _DATNAMESTR_;
      n.io = "sc_in";
      n.width = width;
      if (width)
        vec.push_back(n);
    } else if (io_type == "Out") {
      port_name n; 
      n.dotted_name = type + "." + _RDYNAMESTR_;
      n.flat_name = name + "_" + _RDYNAMESTR_;
      n.io = "sc_in";
      n.width = 1;
      n.is_sc_in_bool = 1;
      vec.push_back(n);

      n = port_name();

      n.dotted_name = type + "." + _VLDNAMESTR_;
      n.flat_name = name + "_" + _VLDNAMESTR_;
      n.io = "sc_out";
      n.is_sc_out_bool = 1;
      n.width = 1;
      vec.push_back(n);

      n = port_name();

      n.dotted_name = type + "." + _DATNAMESTR_;
      n.flat_name = name + "_" + _DATNAMESTR_;
      n.width = width;
      n.io = "sc_out";
      if (width)
        vec.push_back(n);
    } else {
      port_name n; 
      n.dotted_name = type;
      n.flat_name = name;
      n.io = io_type;
      n.width = width;
      vec.push_back(n);
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

  void gen_wrap_rtl() {
    time_t now = time(0);
    char* dt = ctime(&now);
    remove_base();
    port_name_vec.clear();

    std::string inst(module_name + "_inst");
    std::string rtl_inst(module_name + "_rtl_inst");
    std::string rtl_proxy(module_name + "_rtl_proxy_type");

    for (unsigned i=0; i<port_info_vec.size(); ++i) {
     if (port_info_vec[i].type != std::string("{}")) {
      gen_name(port_name_vec, module_name  + "_inst." + port_info_vec[i].name, port_info_vec[i].name,
              port_info_vec[i].type, port_info_vec[i].width);
      if (port_info_vec[i].is_sc_in_bool)
        port_name_vec.back().is_sc_in_bool = 1;
      if (port_info_vec[i].is_sc_out_bool)
        port_name_vec.back().is_sc_out_bool = 1;
     } else {
      for (unsigned c=0; c < port_info_vec[i].child_vec.size(); ++c) {
        gen_name(port_name_vec, 
          child_type(i,c, "_inst.") , child_name(i,c) , port_info_vec[i].child_vec[c].type,
           port_info_vec[i].child_vec[c].width);
        if (port_info_vec[i].child_vec[c].is_sc_in_bool)
          port_name_vec.back().is_sc_in_bool = 1;
        if (port_info_vec[i].child_vec[c].is_sc_out_bool)
          port_name_vec.back().is_sc_out_bool = 1;
      }
     }
    }

    ofstream sc_h;
    sc_h.open(module_name + "_wrap_rtl.h");
    std::cout << "Generating " << module_name << "_wrap_rtl.h" << "\n";
    sc_h << "// Auto generated on: " << dt << "\n";
    sc_h << "// This file wraps the post-HLS RTL model to enable instantiation in an SC testbench\n";
    sc_h << "//  with the same SC interface as the original SC DUT\n\n";

    sc_h << "#include <TypeToBits.h>\n";
    sc_h << "#include \"" << module_name + ".h" << "\"" << "\n\n" ;
    sc_h << "class " << rtl_proxy << " : public sc_foreign_module {\n";
    sc_h << "public:\n";

    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     if (!n.is_sc_in_bool && !n.is_sc_out_bool) {
      if (n.io == "sc_out") {
        sc_h << "  sc_out<sc_lv<" << n.width << ">> " << "CCS_INIT_S1(" << n.flat_name << ");\n";
      } else {
        sc_h << "  sc_in<sc_lv<" << n.width << ">> " << "CCS_INIT_S1(" << n.flat_name << ");\n";
      }
     }
     else {
      if (n.io == "sc_out") {
        sc_h << "  sc_out<bool> " << "CCS_INIT_S1(" << n.flat_name << ");\n";
      } else {
        sc_h << "  sc_in<bool> " << "CCS_INIT_S1(" << n.flat_name << ");\n";
      }
     }
    }

    sc_h << "\n  " << rtl_proxy 
                   << "(sc_module_name nm , const char* hdl_name=\"" << module_name << "_wrap_rtl\") \n";
    sc_h << "    : sc_foreign_module(nm) {\n";
    sc_h << "     elaborate_foreign_module(hdl_name, 0, (const char**)0); \n";
    sc_h << "  }\n";

    sc_h << "};\n\n\n";

    sc_h << "class " << module_name + "_wrap_rtl" << " : public sc_module {\n";
    sc_h << "public:\n";
    sc_h << "  " << module_name << "& " << inst << ";\n\n";

    for (unsigned i=0; i<port_info_vec.size(); ++i) {
      sc_h << "  decltype(" << inst << "." << port_info_vec[i].name 
           << ") CCS_INIT_S1(" << port_info_vec[i].name << ");\n";
    }

    sc_h << "  \n\n";
    sc_h << "  template <class T> struct type_info { };\n\n";
    sc_h << "  template <class T> struct type_info<sc_in<T>> {\n";
    sc_h << "    typedef T data_type;\n";
    sc_h << "    static const int width = Wrapped<data_type>::width;\n";
    sc_h << "    typedef sc_lv<width> sc_lv_type;\n";
    sc_h << "    static const bool is_sc_out = 0;\n";
    sc_h << "  };\n\n";
    sc_h << "  template <class T> struct type_info<sc_out<T>> {\n";
    sc_h << "    typedef T data_type;\n";
    sc_h << "    static const int width = Wrapped<data_type>::width;\n";
    sc_h << "    typedef sc_lv<width> sc_lv_type;\n";
    sc_h << "    static const bool is_sc_out = 1;\n";
    sc_h << "  };\n\n";

    for (unsigned i=0; i<port_name_vec.size(); ++i) {
      port_name& n = port_name_vec[i];
      n.type_info_name = std::string("type_info_") + n.flat_name;
      n.sig_name = std::string("sig_") + n.flat_name;
      sc_h << "  typedef type_info<decltype(" << n.dotted_name << ")> " << n.type_info_name << ";\n";
    }

    sc_h << "\n";

    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     if (!n.is_sc_in_bool && !n.is_sc_out_bool)
      sc_h << "  sc_signal<" << n.type_info_name << "::sc_lv_type> CCS_INIT_S1(" << n.sig_name << ");\n";
    }

    sc_h << "\n";

    sc_h << "  " << rtl_proxy << " " << "CCS_INIT_S1(" << rtl_inst << ");\n";

    sc_h << "  SC_HAS_PROCESS(" << module_name << "_wrap_rtl);\n\n";
    sc_h << "  " << module_name << "_wrap_rtl(sc_module_name nm) : " << inst << "(*(" << module_name << "*)0){\n\n";

    for (unsigned i=0; i<port_info_vec.size(); ++i) {
     if (port_info_vec[i].type != std::string("{}")) {
      if ((port_info_vec[i].type == "In") || (port_info_vec[i].type == "Out"))
       if (port_info_vec[i].width)
        sc_h << "    " << port_info_vec[i].name << ".disable_spawn();\n";
     } else {
      for (unsigned c=0; c < port_info_vec[i].child_vec.size(); ++c) {
      if ((port_info_vec[i].child_vec[c].type == "In") || (port_info_vec[i].child_vec[c].type == "Out"))
       if (port_info_vec[i].width)
        sc_h << "    " << port_info_vec[i].name << "." 
             << port_info_vec[i].child_vec[c].name << ".disable_spawn();\n";
      }
     }
    }

    sc_h << "\n";

    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     if (!n.is_sc_in_bool && !n.is_sc_out_bool) {
      if (n.io == "sc_out") {
        sc_h << "    SC_METHOD(method_" << n.flat_name << "); sensitive << " << n.sig_name << "; dont_initialize();\n";
      } else {
        sc_h << "    SC_METHOD(method_" << n.flat_name << "); sensitive << " 
             << strip_dotted(n.dotted_name) << ";\n";
      }
     }
    }


    sc_h << "\n";

    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     if (n.is_sc_in_bool || n.is_sc_out_bool)
      sc_h << "    " << rtl_inst << "." << n.flat_name << "(" << strip_dotted(n.dotted_name) << ");\n";
     else
      sc_h << "    " << rtl_inst << "." << n.flat_name << "(" << n.sig_name << ");\n";
    }

    sc_h << "  }\n\n";

    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     if (!n.is_sc_in_bool && !n.is_sc_out_bool) {
      sc_h << "  void method_" << n.flat_name << "(){\n";
      sc_h << "    typename " << n.type_info_name << "::data_type obj;\n";
      sc_h << "    typename " << n.type_info_name << "::sc_lv_type lv;\n";
      if (n.io == "sc_in") {
        sc_h << "    obj = " << strip_dotted(n.dotted_name) << ";\n";
        sc_h << "    lv = TypeToBits(obj);\n";
        sc_h << "    " << n.sig_name << " = lv;\n";
      } else {
        sc_h << "    lv = " << n.sig_name << ".read();\n";
        sc_h << "    obj = BitsToType<decltype(obj)>(lv);\n";
        sc_h << "    " << strip_dotted(n.dotted_name) << " = obj;\n";
      }
      sc_h << "  }\n";
     }
    }

    sc_h << "\n";
    sc_h << "};\n";
    sc_h.close();

    ofstream sv_v;
    sv_v.open(module_name + "_wrap_rtl.sv");
    std::cout << "Generating " << module_name << "_wrap_rtl.sv" << "\n";
    sv_v << "// Auto generated on: " << dt << "\n";
    sv_v << "// This file wraps the post-HLS RTL model to enable instantiation in an SC testbench\n";
    sv_v << "// This sv wrapper transforms any packed structs into plain bit vectors\n";
    sv_v << "// for interfacing with the SC TB\n";
    sv_v << "\n";
    sv_v << "module " << module_name << "_wrap_rtl (\n";

    std::string comma(" ");
    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     sv_v << "  " << comma << " " << n.flat_name << "\n";
     comma = ",";
    }

    sv_v << ");\n\n";

    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     if (!n.is_sc_in_bool && !n.is_sc_out_bool) {
      if (n.io == "sc_out") {
        sv_v << "  output [" << n.width - 1 << ":0] " << n.flat_name << ";\n";
      } else {
        sv_v << "  input  [" << n.width - 1 << ":0] " << n.flat_name << ";\n";
      }
     }
     else {
      if (n.io == "sc_out") {
        sv_v << "  output " << n.flat_name << ";\n";
      } else {
        sv_v << "  input  " << n.flat_name << ";\n";
      }
     }
    }

    sv_v << "\n";
    comma = " ";
    sv_v << "  " << module_name << " " << module_name << "_inst (\n";
    for (unsigned i=0; i<port_name_vec.size(); ++i) {
     port_name& n = port_name_vec[i];
     sv_v << "  " << comma << " ." << n.flat_name << "(" << n.flat_name << ")\n";
     comma = ",";
    }
    sv_v << "  " << ");\n";

    sv_v << "\nendmodule\n";
    sv_v.close();

  }

  std::string strip_dotted(std::string& input) {
    size_t pos = input.find(".");
    if (pos != std::string::npos)
      return input.substr(pos + 1);
    return input;
  }
};
