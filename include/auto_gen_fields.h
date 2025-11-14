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
// Date: 22 Dec 2023

//*****************************************************************************************
// File: auto_gen_fields.h
//
// Description: C++ Macros to simplify making user-defined struct types work in Connections
//
// Revision History:
//       2.2.2 - Fix CAT-39602 - Changes from Stuart Swan
//       2.2.1 - Fix CAT-38256 - Clean up compiler warnings about extra semicolons
//       2.1.1 - Added back in typedef of 'this_type' for backward compatibility with
//               older designs
//             - Fix for CAT-35587 from Stuart Swan
//*****************************************************************************************

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

#include <connections/connections_utils.h>

/*
#ifndef __CONNECTIONS__CONNECTIONS_UTILS_H_
// Prevent redefine warnings from NVHLS
#undef CONNECTIONS_ASSERT_MSG
#undef CONNECTIONS_SIM_ONLY_ASSERT_MSG
#include <connections/connections_utils.h>
#endif

#ifndef NVHLS_CONNECTIONS_UTILS_H_
// Prevent redefine warnings from NVHLS
#undef CONNECTIONS_ASSERT_MSG
#undef CONNECTIONS_SIM_ONLY_ASSERT_MSG
#include <nvhls_connections_utils.h>
#endif
*/

#include <boost/preprocessor/list/for_each.hpp>
#include <boost/preprocessor/tuple/to_list.hpp>
#include <connections/marshaller.h>
#include <UIntOrEmpty.h>

#include <type_traits>

struct field_info {
  std::string name;    // field name
  unsigned width{0};   // bit width
  unsigned dim1{0};    // left most dimension -> 0 means does not exist
  unsigned dim0{0};    // right most dimension -> 0 means does not exist
  std::vector<field_info> fields;  // child field info if struct/class type

  inline friend std::ostream &operator<<(ostream &os, const field_info &rhs) { 
   os << rhs.name << " width: " << rhs.width << 
      " dim1: " << rhs.dim1 << " dim0: " << rhs.dim0 << "\n"  ;
   if (rhs.fields.size()) {
     os << "{\n";
     for (unsigned i=0; i < rhs.fields.size(); ++i)
       os << rhs.fields[i];
     os << "}\n";
   }
   return os;
  }

  void stream_indent(ostream &os, std::string indent) { 
   os << indent << name << " width: " << width << " dim1: " << dim1 << " dim0: " << dim0 << "\n"  ;
   if (fields.size()) {
     os << indent << "{\n";
     for (unsigned i=0; i < fields.size(); ++i)
       fields[i].stream_indent(os, indent + " ");
     os << indent << "}\n";
   }
  }

};

// Primary template
template <typename T, typename = void>
struct call_gen_field_info {
#ifndef CCS_SYSC
    static void gen_field_info(std::vector<field_info>& vec) { }
#endif
};

// Helper trait to detect .Marshall() member function
template <typename T, typename = void>
struct has_Marshall : std::false_type {};

#ifndef CCS_SYSC
/**** Long term solution -- will automatically cause compile error in correct situations ****/
template <typename T>
struct has_Marshall<T, 
  decltype(std::declval<T>().Marshall(std::declval<Marshaller<Wrapped<T>::width>&>()), void())> 
  : std::true_type {};
#endif

/**** alternative implementation has less rigorous error checking, may be useful for "bring up"
template <typename T>
struct has_Marshall<T, 
  decltype(std::declval<T>().gen_field_info(std::declval<std::vector<field_info>&>()), void())> 
  : std::true_type {};
****/

#ifdef CCS_SYSC
template <typename T>
struct call_gen_field_info<T> {
    static void gen_field_info(std::vector<field_info>& vec) { }
};
#else
// SFINAE-based specialization for types with a `.Marshall()` member
template <typename T>
struct call_gen_field_info<T, typename std::enable_if<has_Marshall<T>::value>::type> {
    static void gen_field_info(std::vector<field_info>& vec) { 
     T::gen_field_info(vec); 
    }
};
#endif


template <class T>
class type_traits
{
public:
  static const bool is_array{false};
  static const int  d1{0};
  typedef T elem_type;

  template <unsigned int Size, class S> static void Marshall(Marshaller<Size>& m, S& A) {
    m & A ;
  }

  inline static void trace(sc_trace_file *tf, const elem_type &v, const std::string &NAME ) {
    sc_trace(tf,v,NAME);
  }

  inline static void info(std::vector<field_info>& vec, const std::string &NAME ) {
   field_info i;
   i.name = NAME;
   i.width = Wrapped<T>::width;
   call_gen_field_info<T>::gen_field_info(i.fields);
   vec.push_back(i);
  }

  template <class S> inline static void stream(ostream& os, const S& rhs)
  {
    os << "{" << rhs << "} ";
  }

  template <class S> static bool equal(const S& lhs, const S& rhs) { 
    return lhs == rhs;
  }
};

template <class T, int D1>
class type_traits<T[D1]>
{
public:
  static const bool is_array{true};
  static const int  d1{D1};
  typedef T elem_type;

  template <unsigned int Size> static void Marshall(Marshaller<Size>& m, elem_type A[d1]) {
    for (int i=0; i<d1; i++) m & A [i];
  }

  template <class S> inline static void trace(sc_trace_file *tf, const S& v, const std::string &NAME )
  {
    for (int i=0; i<d1; i++)  {
      std::ostringstream os;
      os << NAME << "_" << i;
      sc_trace(tf,v[i], os.str());
    }
  }

  inline static void info(std::vector<field_info>& vec, const std::string &NAME ) {
   field_info i;
   i.name = NAME;
   i.width = Wrapped<T>::width;
   i.dim0 = d1;
   call_gen_field_info<T>::gen_field_info(i.fields);
   vec.push_back(i);
  }

  template <class S> inline static void stream(ostream& os, const S& rhs)
  {
    os << "{";
    for (int i=0; i<d1; i++)  {
      os << rhs[i];
      if (i < d1-1)
        os << ",";
    }
    os << "} ";
  }

  template <class S> static bool equal(const S& lhs, const S& rhs) { 
    for (int i=0; i<d1; i++)  {
      if (!(lhs[i] == rhs[i]))
        return false;
    }
    return true;
  }
};

template <class T, int D1, int D2>
class type_traits<T[D1][D2]>
{
public:
  static const bool is_array{true};
  static const int  d1{D1};
  static const int  d2{D2};
  typedef T elem_type;

  template <unsigned int Size> static void Marshall(Marshaller<Size>& m, elem_type A[d1][d2]) {
    for (int i1=0; i1<d1; i1++) 
      for (int i2=0; i2<d2; i2++) 
        m & A [i1][i2];
  }

  template <class S> inline static void trace(sc_trace_file *tf, const S& v, const std::string &NAME )
  {
    for (int i1=0; i1<d1; i1++)  
      for (int i2=0; i2<d2; i2++)  
      {
        std::ostringstream os;
        os << NAME << "_" << i1 << "_" << i2;
        sc_trace(tf,v[i1][i2], os.str());
      }
  }

  inline static void info(std::vector<field_info>& vec, const std::string &NAME ) {
   field_info i;
   i.name = NAME;
   i.width = Wrapped<T>::width ;
   i.dim1 = d1;
   i.dim0 = d2;
   call_gen_field_info<T>::gen_field_info(i.fields);
   vec.push_back(i);
  }

  template <class S> inline static void stream(ostream& os, const S& rhs)
  {
    os << "{";
    for (int i1=0; i1<d1; i1++)  
    {
     os << "{";
     for (int i2=0; i2<d2; i2++)  
     {
      os << rhs[i1][i2];
      if (i2 < d2-1)
        os << ",";
     }
     os << "} ";
    }
    os << "} ";
  }

  template <class S> static bool equal(const S& lhs, const S& rhs) { 
    for (int i1=0; i1<d1; i1++)  
     for (int i2=0; i2<d2; i2++)  
     {
      if (!(lhs[i1][i2] == rhs[i1][i2]))
        return false;
     }
    return true;
  }
};

template <>
class type_traits<nvhls::EmptyField>
{
public:
  static const bool is_array{false};
  typedef nvhls::EmptyField elem_type;

  template <unsigned int Size> static void Marshall(Marshaller<Size>& m, elem_type& A) {
  }

  template <class S> inline static void trace(sc_trace_file *tf, const S& v, const std::string &NAME )
  {
  }

  inline static void info(std::vector<field_info>& vec, const std::string &NAME ) {
  }

  template <class S> inline static void stream(ostream& os, const S& rhs)
  {
    os << "{} ";
  }

  template <class S> static bool equal(const S& lhs, const S& rhs) { 
    return true;
  }
};

template <class T>
class calc_bit_width
{
public:
  static const unsigned int width = Wrapped<T>::width;
};

template <class T, int D1>
class calc_bit_width<T[D1]>
{
public:
  static const unsigned int width = Wrapped<T>::width * D1;
};

template <class T, int D1, int D2>
class calc_bit_width<T[D1][D2]>
{
public:
  static const unsigned int width = Wrapped<T>::width * D1 * D2;
};

template <>
class calc_bit_width<nvhls::EmptyField>
{
public:
  static const unsigned int width = 0;
};


#define GEN_MARSHALL_FIELD(R, _, F) \
   type_traits<decltype(F)>::Marshall(m, F); 
   //

#define GEN_MARSHALL_METHOD(FIELDS) \
  template <unsigned int Size> void Marshall(Marshaller<Size>& m) { \
     BOOST_PP_LIST_FOR_EACH(GEN_MARSHALL_FIELD, _, FIELDS); \
  }
  //

#define GEN_TRACE_FIELD(R, _, F) \
   type_traits<decltype(v.F)>::trace(tf, v.F, NAME + "_" + #F); 
   //

#define GEN_TRACE_METHOD(FIELDS) \
  inline friend void sc_trace(sc_trace_file *tf, const auto_gen_type &v, const std::string &NAME ) { \
     BOOST_PP_LIST_FOR_EACH(GEN_TRACE_FIELD, _, FIELDS); \
  }
  //

#define GEN_INFO_FIELD(R, _, F) \
   type_traits<decltype(F)>::info(vec, #F); 
   //

#define GEN_INFO_METHOD(FIELDS) \
  inline static void gen_field_info(std::vector<field_info>& vec) { \
     BOOST_PP_LIST_FOR_EACH(GEN_INFO_FIELD, _, FIELDS); \
  }
  //

#define GEN_STREAM_FIELD(R, _, F) \
   os << #F ; \
   type_traits<decltype(rhs.F)>::stream(os, rhs.F); 
   //

#define GEN_STREAM_METHOD(FIELDS) \
  inline friend std::ostream &operator<<(ostream &os, const auto_gen_type &rhs) { \
    BOOST_PP_LIST_FOR_EACH(GEN_STREAM_FIELD, _, FIELDS); \
    return os; \
  }
  //

#define GEN_ADD_FIELD_WIDTH(R, _, F) \
   + calc_bit_width<decltype(F)>::width
  //

#define GEN_WIDTH(FIELDS) \
  static const unsigned int width = 0 \
    BOOST_PP_LIST_FOR_EACH(GEN_ADD_FIELD_WIDTH, _, FIELDS) \
    ; 
  //

#define GEN_EQUAL_FIELD(R, _, F) \
   && type_traits<decltype(F)>::equal(F, rhs.F)
  //

#define GEN_EQUAL(FIELDS) \
  bool operator==(const auto_gen_type & rhs) const {  \
    return true \
      BOOST_PP_LIST_FOR_EACH(GEN_EQUAL_FIELD, _, FIELDS) \
    ; \
  }
  //


#define FIELD_LIST(X) BOOST_PP_TUPLE_TO_LIST(BOOST_PP_TUPLE_SIZE(X), X )

#define AUTO_GEN_FIELD_METHODS(THIS_TYPE, X) \
  typedef THIS_TYPE this_type; \
  typedef THIS_TYPE auto_gen_type; \
  GEN_MARSHALL_METHOD(FIELD_LIST(X)) \
  GEN_TRACE_METHOD(FIELD_LIST(X)) \
  GEN_INFO_METHOD(FIELD_LIST(X)) \
  GEN_STREAM_METHOD(FIELD_LIST(X)) \
  GEN_WIDTH(FIELD_LIST(X)) \
  GEN_EQUAL(FIELD_LIST(X))
  //

#define AUTO_GEN_FIELD_METHODS_V2(THIS_TYPE, X) \
  typedef THIS_TYPE auto_gen_type; \
  GEN_TRACE_METHOD(FIELD_LIST(X)) \
  GEN_STREAM_METHOD(FIELD_LIST(X)) \
  GEN_EQUAL(FIELD_LIST(X))
  //


