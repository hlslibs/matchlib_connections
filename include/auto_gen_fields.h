// INSERT_EULA_COPYRIGHT: 2023

// Prototype code - not fully production ready

// Author: Stuart Swan, Platform Architect, Siemens EDA
// Date: 10 Aug 2023

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
  inline friend void sc_trace(sc_trace_file *tf, const this_type &v, const std::string &NAME ) { \
     BOOST_PP_LIST_FOR_EACH(GEN_TRACE_FIELD, _, FIELDS); \
  }
  //

#define GEN_STREAM_FIELD(R, _, F) \
   os << #F ; \
   type_traits<decltype(rhs.F)>::stream(os, rhs.F); 
   //

#define GEN_STREAM_METHOD(FIELDS) \
  inline friend std::ostream &operator<<(ostream &os, const this_type &rhs) { \
    BOOST_PP_LIST_FOR_EACH(GEN_STREAM_FIELD, _, FIELDS); \
    return os; \
  }
  //

#define GEN_ADD_FIELD_WIDTH(R, _, F) \
   + calc_bit_width<decltype(F)>::width
  //

#define GEN_WIDTH(FIELDS) \
  static const unsigned int width = 0 \
    BOOST_PP_LIST_FOR_EACH(GEN_ADD_FIELD_WIDTH, _, FIELDS); \
    ; 
  //

#define GEN_EQUAL_FIELD(R, _, F) \
   && type_traits<decltype(F)>::equal(F, rhs.F)
  //



#define GEN_EQUAL(FIELDS) \
  bool operator==(const this_type & rhs) const {  \
    return true \
      BOOST_PP_LIST_FOR_EACH(GEN_EQUAL_FIELD, _, FIELDS) \
    ; \
  }
  //


#define FIELD_LIST(X) BOOST_PP_TUPLE_TO_LIST(BOOST_PP_TUPLE_SIZE(X), X )

#define AUTO_GEN_FIELD_METHODS(THIS_TYPE, X) \
  typedef THIS_TYPE this_type; \
  GEN_MARSHALL_METHOD(FIELD_LIST(X)) \
  GEN_TRACE_METHOD(FIELD_LIST(X)) \
  GEN_STREAM_METHOD(FIELD_LIST(X)) \
  GEN_WIDTH(FIELD_LIST(X)) \
  GEN_EQUAL(FIELD_LIST(X))
  //



