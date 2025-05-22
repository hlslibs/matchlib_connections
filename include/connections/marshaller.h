


/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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
// marshaller.h
//
// Revision History:
//  2.2.2    - CAT-31753 - Add Wrapped and Marshaller specialization for arrays
//  2.2.1    - CAT-38139 - Fix BitUnion2 compile error
//  2.1.0    - CAT-34971 - clean up compiler warnings (added methods to set default values)
//  1.2.8    - fix number of long bits which can change based on 32-bit/64-bit target arch
//  1.2.6    - CAT-29221
//  1.2.4    - Add Connections marshaller support for ac_float
//           - Reduce #include list, add mc_typeconv.h explicitly for re-entrant features
//  1.2.0    - Fixed CAT-25473 - Sign bit needs to be handled properly in
//             marshaller SpecialWrapper2
//           - Added Sign to Type template parameters.
//           - Add Connections support for ac_std_float and ac::bfloat16. CAT-25338
//           - Remove ccs_p2p.h dependency from marshaller.h.
//           - Make marshaller.h "re-entrant" to allow better flexibility
//             for ordering include files.
//           - Add connections support for sc_fixed, ac_fixed (>3 parameters),
//             ieee_float, C int datatypes, and ac_complex usages of all
//             those types.  CAT-24885, CAT-24940, CAT-25256, CAT-25279
//
//*****************************************************************************************

#include <systemc.h>
#include <ccs_types.h>
#include <mc_typeconv.h>
#include "connections_utils.h"

// Max number of bits we can marshall. Beyond 10k tends to cause segfaults in connections code
// since objects get allocated on C stack, so very large objects cause stack overflows
// Keep in mind anything that is marshalled directly represents wires/registers in HW,
// so 10k limit is plenty for an individual port on a module, which is what this represents.
#ifndef MARSHALL_LIMIT
#define MARSHALL_LIMIT 10000
#endif

#if !defined(__CONNECTIONS__MARSHALLER_H_)
//------------------------------------------------------------------------
// Marshaller casting functions

// Helper to easily disable verbose X messaging when converting from logic types
// Use the macro CONNECTIONS_DISABLE_X_WARNINGS 
template<class Dummy>
struct connections_x_warnings {
  static void disable() {
    static bool once = 1;
    if ( once ) {
      sc_report_handler::set_actions(SC_ID_LOGIC_X_TO_BOOL_, SC_DO_NOTHING);
      sc_report_handler::set_actions(SC_ID_VECTOR_CONTAINS_LOGIC_VALUE_, SC_DO_NOTHING);
      once = 0;
    }
  }
};

template<typename A, int vec_width>
void connections_cast_type_to_vector(const A &data, int length, sc_lv<vec_width> &vec)
{
  type_to_vector(data, length, vec);
}

template<typename A, int vec_width>
void connections_cast_vector_to_type(const sc_lv<vec_width> &vec, bool is_signed, A *data)
{
#ifdef CONNECTIONS_DISABLE_X_WARNINGS
  connections_x_warnings<void>::disable();
#endif
  vector_to_type(vec, is_signed, data);
}

//------------------------------------------------------------------------
// Marshaller

/**
 * \brief Marshaller is used to automatically convert types to logic vector and vice versa
 * \ingroup Marshaller
 *
 * \par Overview
 * Marshaller class: similar to boost's serialization approach, Marshaller class
 * provides an easy way to convert an arbitrary SystemC data structures to/from
 * a sequence of bits.
 * NOTE: I am assuming all the types that we are dealing with are
 * unsigned which may or may not be true. This matters in places where the
 * Marshaller will use vector_to_type calls.
 *
 * NOTE: The solution for the marshaller below relies on the
 * type_to_vector() and vector_to_type() calls as Catapult libraries deal
 * with the specialization for all C++ primitive types, SystemC types, and
 * the ac_* types. To make this vendor agnostic we would have to implement
 * our own connections_cast() with all the specialization. Since, the Marshaller
 * deals with unpacking and packing all complex types, using the Catapult
 * type conversion functions works well in this case.
 *
 * \par A Simple Example
 * \code
 *      #include <connections/marshaller.h>
 *
 *      ...
 *      class mem_req_t {
 *      public:
 *        bool do_store;
 *        bank_addr_t addr;
 *        DataType    wdata;
 *        static const int width = 1 + addr_width + DataType::width;
 *
 *        template <unsigned int Size>
 *        void Marshall(Marshaller<Size>& m) {
 *          m& do_store;
 *          m& addr;
 *          m& wdata;
 *        }
 *      };
 *
 *
 * \endcode
 * \par
 *
 */
template <unsigned int Size>
class Marshaller
{
  sc_lv<Size> glob;
  unsigned int cur_idx;
  bool is_marshalling;

public:
  /* Constructor.
   *   if is_marshalling == True:
   *     convert type to bits;
   *   else:
   *     convert bits to type. */
  Marshaller() : glob(0), cur_idx(0), is_marshalling(true) {}
  Marshaller(sc_lv<Size> v) : glob(v), cur_idx(0), is_marshalling(false) {}

#ifndef __SYNTHESIS__
  static_assert(Size < MARSHALL_LIMIT, "Size must be less than MARSHALL_LIMIT");
#endif

  /* Add a field to the glob, or extract it. */
  template <typename T, int FieldSize>
  void AddField(T &d) {
    CONNECTIONS_SIM_ONLY_ASSERT_MSG(cur_idx + FieldSize <= Size, "Field size exceeded Size. Is a message's width enum missing an element, and are all fields marshalled?");
    if (is_marshalling) {
      sc_lv<FieldSize> bits;
      connections_cast_type_to_vector(d, FieldSize, bits);
      glob.range(cur_idx + FieldSize - 1, cur_idx) = bits;
      cur_idx += FieldSize;
    } else {
      sc_lv<FieldSize> bits = glob.range(cur_idx + FieldSize - 1, cur_idx);
      connections_cast_vector_to_type(bits, false, &d);
      cur_idx += FieldSize;
    }
  }

  /* Return the bit vector. */
  sc_lv<Size> GetResult() {
    CONNECTIONS_SIM_ONLY_ASSERT_MSG(cur_idx==Size, "Size doesn't match current index. Is a message's width enum missing an element, and are all fields marshalled?");
    return glob.range(Size - 1, 0);
  }
};

/**
 * \brief Generic Wrapped class: wraps different datatypes to communicate with Marshaller
 * \ingroup Marshaller
 *
 * \par Overview
 *  This function is used to determine the width of a datatype. For ac_types, it relies on static member inside the class called width. For sc_types and bool, Wrapped class has specilizations that determine its width.
 * \par A Simple Example
 * \code
 *  #include <connections/marshaller.h>
 *
 *  ...
 *  DataType    wdata;
 *  const int width = Wrapped<DataType>::width;
 *
 * \endcode
 * \par
 *
 */
template <typename T>
class Wrapped
{
public:
  T val;
  Wrapped() {}
  Wrapped(const T &v) : val(v) {}
  static const unsigned int width = T::width;
  // Assigning is_signed to false by default. Specializations for basic signed types are defined below
  static const bool is_signed = false;
  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    val.template Marshall<Size>(m);
  }
};

/* User defined T needs to have a Marshall() function defined. */
template <unsigned int Size, typename T>
Marshaller<Size> &operator&(Marshaller<Size> &m, T &rhs) {
  rhs.Marshall(m);
  return m;
}

/* Wrapped class specialization for sc_lv.  */
template <int Width>
class Wrapped<sc_lv<Width> >
{
public:
  sc_lv<Width> val;
  Wrapped() {}
  Wrapped(const sc_lv<Width> &v) : val(v) {}
  static const unsigned int width = Width;
  static const bool is_signed = false;
  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    m &val;
  }
};

/* Operator & for Marshaller and sc_lv. */
template <unsigned int Size, int K>
Marshaller<Size> &operator&(Marshaller<Size> &m, sc_lv<K> &rhs)
{
  m.template AddField<sc_lv<K>, K>(rhs);
  return m;
}

/* Wrapped class specialization for basic types.  */
#define MarshallBasicTypes(Type, Signed, Sizeof)       \
template <>                              \
class Wrapped<Type> {                    \
 public:                                 \
  Type val;                              \
  Wrapped() {}                           \
  Wrapped(const Type& v) : val(v) {}     \
  static const unsigned int width = Sizeof; \
  static const bool is_signed = Signed;  \
  template <unsigned int Size>           \
  void Marshall(Marshaller<Size>& m) {   \
    m& val;                              \
  }                                      \
};                                       \
template <unsigned int Size>             \
Marshaller<Size>& operator&(Marshaller<Size>& m, Type& rhs) { \
  m.template AddField<Type, Sizeof>(rhs);                     \
  return m;                                                   \
}
MarshallBasicTypes(bool,0,1);
MarshallBasicTypes(char,1,8);
MarshallBasicTypes(unsigned char,0,8);
MarshallBasicTypes(short,1,16);
MarshallBasicTypes(unsigned short,0,16);
MarshallBasicTypes(int,1,32);
MarshallBasicTypes(unsigned int,0, 32);
MarshallBasicTypes(long,1,sizeof(long)*8);
MarshallBasicTypes(unsigned long,0,sizeof(unsigned long)*8);
MarshallBasicTypes(long long,1,64);
MarshallBasicTypes(unsigned long long,0,64);

/* Use the preprocessor to define many similar wrapper specializations. */
#define MarshallEnum(Type, EnumSize)                              \
  template <>                                                     \
  class Wrapped<Type> {                                           \
   public:                                                        \
    Type val;                                                     \
    Wrapped() {}                                                  \
    Wrapped(const Type& v) : val(v) {}                            \
    static const unsigned int width = EnumSize;                   \
    static const bool is_signed = false;                          \
    template <unsigned int Size>                                  \
    void Marshall(Marshaller<Size>& m) {                          \
      m& val;                                                     \
    }                                                             \
  };                                                              \
                                                                  \
  template <unsigned int Size>                                    \
  Marshaller<Size>& operator&(Marshaller<Size> & m, Type & rhs) { \
    m.template AddField<Type, EnumSize>(rhs);                     \
    return m;                                                     \
  }

#define SpecialUnsignedWrapper(Type)                                 \
  template <int Width>                                               \
  class Wrapped<Type<Width> > {                                      \
   public:                                                           \
    Type<Width> val;                                                 \
    Wrapped() {}                                                     \
    Wrapped(const Type<Width>& v) : val(v) {}                        \
    static const unsigned int width = Width;                         \
    static const bool is_signed = false;                             \
    template <unsigned int Size>                                     \
    void Marshall(Marshaller<Size>& m) {                             \
      m& val;                                                        \
    }                                                                \
  };                                                                 \
                                                                     \
  template <unsigned int Size, int K>                                \
  Marshaller<Size>& operator&(Marshaller<Size> & m, Type<K> & rhs) { \
    m.template AddField<Type<K>, K>(rhs);                            \
    return m;                                                        \
  }

#define SpecialSignedWrapper(Type)                                   \
  template <int Width>                                               \
  class Wrapped<Type<Width> > {                                      \
   public:                                                           \
    Type<Width> val;                                                 \
    Wrapped() {}                                                     \
    Wrapped(const Type<Width>& v) : val(v) {}                        \
    static const unsigned int width = Width;                         \
    static const bool is_signed = true;                              \
    template <unsigned int Size>                                     \
    void Marshall(Marshaller<Size>& m) {                             \
      m& val;                                                        \
    }                                                                \
  };                                                                 \
                                                                     \
  template <unsigned int Size, int K>                                \
  Marshaller<Size>& operator&(Marshaller<Size> & m, Type<K> & rhs) { \
    m.template AddField<Type<K>, K>(rhs);                            \
    return m;                                                        \
  }

#define SpecialWrapper2(Type)                                              \
  template <int Width, bool Sign>                                          \
  class Wrapped<Type<Width, Sign> > {                                      \
   public:                                                                 \
    Type<Width, Sign> val;                                                 \
    Wrapped() {}                                                           \
    Wrapped(const Type<Width, Sign>& v) : val(v) {}                        \
    static const unsigned int width = Width;                               \
    static const bool is_signed = Sign;                                    \
    template <unsigned int Size>                                           \
    void Marshall(Marshaller<Size>& m) {                                   \
      m& val;                                                              \
    }                                                                      \
  };                                                                       \
                                                                           \
  template <unsigned int Size, int K, bool Sign>                           \
  Marshaller<Size>& operator&(Marshaller<Size> & m, Type<K, Sign> & rhs) { \
    m.template AddField<Type<K, Sign>, K>(rhs);                            \
    return m;                                                              \
  }

#define SpecialWrapper3(Type)                               \
  template <int Width, int IWidth, bool Sign, ac_q_mode Q, ac_o_mode O> \
    class Wrapped<Type<Width, IWidth, Sign, Q, O> > {       \
   public:                                                  \
   Type<Width,IWidth,Sign,Q,O> val;                         \
    Wrapped() {}                                            \
  Wrapped(const Type<Width,IWidth,Sign,Q,O>& v) : val(v) {} \
    static const unsigned int width = Width;                \
    static const bool is_signed = Sign;                     \
    template <unsigned int Size>                            \
    void Marshall(Marshaller<Size>& m) {                    \
      m& val;                                               \
    }                                                       \
  };                                                        \
                                                            \
  template <unsigned int Size, int K, int J, bool Sign, ac_q_mode Q, ac_o_mode O> \
    Marshaller<Size>& operator&(Marshaller<Size> & m, Type<K,J,Sign,Q,O> & rhs) { \
    m.template AddField<Type<K,J,Sign,Q,O>, K>(rhs);        \
    return m;                                               \
  }


#define SpecialWrapperIfc(Type)                                            \
  template <typename Message>                                              \
  class Wrapped<Type<Message> > {                                          \
   public:                                                                 \
    Type<Message> val;                                                     \
    Wrapped() : val() {}                                                   \
    Wrapped(const Type<Message>& v) : val(v) {}                            \
    static const unsigned int width = Wrapped<Message>::width;             \
    static const bool is_signed = Wrapped<Message>::is_signed;             \
    template <unsigned int Size>                                           \
    void Marshall(Marshaller<Size>& m) {                                   \
      m& val;                                                              \
    }                                                                      \
  };                                                                       \
                                                                           \
  template <unsigned int Size, typename Message>                           \
  Marshaller<Size>& operator&(Marshaller<Size> & m, Type<Message> & rhs) { \
    m.template AddField<Message, Wrapped<Message>::width>(rhs);            \
    return m;                                                              \
  }

SpecialWrapper2(ac_int);
SpecialWrapper3(ac_fixed);
SpecialUnsignedWrapper(sc_bv);
SpecialUnsignedWrapper(sc_uint);
SpecialSignedWrapper(sc_int);
SpecialUnsignedWrapper(sc_biguint);
SpecialSignedWrapper(sc_bigint);
SpecialWrapperIfc(sc_in);
SpecialWrapperIfc(sc_out);
SpecialWrapperIfc(sc_signal);
#endif  // first __CONNECTIONS__MARSHALLER_H_

#if defined(SC_FIXED_H) && !defined(__MARSHALLER_SC_FIXED_H)
#define __MARSHALLER_SC_FIXED_H

#define SpecialSyscFixedWrapper(Type, Sign)                  \
template <int W, int I, sc_q_mode Q, sc_o_mode O, int N>     \
  class Wrapped<Type<W,I,Q,O,N> > {                          \
 public:                                                     \
  Type<W,I,Q,O,N> val;                                       \
  Wrapped() : val(0) {}                                      \
  Wrapped(const Type<W,I,Q,O,N>  &v) : val(v) {}             \
 static const unsigned int width = W;                        \
 static const bool is_signed = Sign;                         \
  template <unsigned int Size>                               \
  void Marshall(Marshaller<Size>& m) {                       \
    m& val;                                                  \
  }                                                          \
};                                                           \
                                                             \
template <unsigned int Size, int W, int I, sc_q_mode Q, sc_o_mode O, int N >  \
  Marshaller<Size>& operator&(Marshaller<Size>& m, Type<W,I,Q,O, N> &rhs) {   \
  m.template AddField<Type<W,I,Q,O,N>, W>(rhs);                               \
  return m;                                                                   \
}
SpecialSyscFixedWrapper(sc_fixed,1);
SpecialSyscFixedWrapper(sc_fixed_fast,1);
SpecialSyscFixedWrapper(sc_ufixed,0);
SpecialSyscFixedWrapper(sc_ufixed_fast,0);
#endif  //  SC_FIXED_H

#if defined(__AC_STD_FLOAT_H) && !defined(__MARSHALLER_AC_STD_FLOAT_H)
#define __MARSHALLER_AC_STD_FLOAT_H

#define SpecialFloatWrapper(Type, W)     \
template <>                              \
class Wrapped<Type > {                   \
 public:                                 \
  Type val;                              \
 Wrapped() : val(0.0) {}                 \
 Wrapped(const Type &v) : val(v) {}      \
  static const unsigned int width = W;   \
  static const bool is_signed = 1;       \
  template <unsigned int Size>           \
    void Marshall(Marshaller<Size>& m) { \
    m& val;                              \
  }                                      \
};                                       \
 template <unsigned int Size >                                  \
  Marshaller<Size>& operator&(Marshaller<Size>& m, Type &rhs) { \
  m.template AddField<Type, W > (rhs);                          \
  return m;                                                     \
}
SpecialFloatWrapper(ac_ieee_float<binary16>, 16);
SpecialFloatWrapper(ac_ieee_float<binary32>, 32);
SpecialFloatWrapper(ac_ieee_float<binary64>, 64);
SpecialFloatWrapper(ac_ieee_float<binary128>, 128);
SpecialFloatWrapper(ac_ieee_float<binary256>, 256);

template <>
class Wrapped<ac::bfloat16 >
{
public:
  ac::bfloat16 val;
  Wrapped() : val(0.0) {}
  Wrapped(const ac::bfloat16 &v) : val(v) {}
  static const unsigned int width = 16;
  static const bool is_signed = 1;
  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    m &val;
  }
};
template <unsigned int Size >
Marshaller<Size> &operator&(Marshaller<Size> &m,  ac::bfloat16 &rhs)
{
  m.template AddField< ac::bfloat16, 16 > (rhs);
  return m;
}

template <int W, int E>
class Wrapped<ac_std_float<W,E> >
{
public:
  ac_std_float<W,E> val;
  Wrapped() : val(0.0) {}
  Wrapped(const ac_std_float<W,E> &v) : val(v) {}
  static const unsigned int width = W;
  static const bool is_signed = 1;
  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    m &val;
  }
};

template <unsigned int Size, int W, int E>
Marshaller<Size> &operator&(Marshaller<Size> &m, ac_std_float<W,E> &rhs)
{
  m.template AddField<ac_std_float<W,E>, W > (rhs);
  return m;
}

#endif  // __AC_STD_FLOAT_H

#if defined(__AC_FLOAT_H) && !defined(__MARSHALLER_AC_FLOAT_H)
#define __MARSHALLER_AC_FLOAT_H
template <int W, int I, int E, ac_q_mode Q>
class Wrapped<ac_float<W,I,E,Q> >
{
public:
  typedef ac_float<W,I,E,Q> Type;
  Type val;
  Wrapped() {}
  Wrapped(const Type &v) : val(v) {}
  static const unsigned int width = Type::width + Type::e_width;
  static const bool is_signed = Type::sign;
  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    m &val.m;
    m &val.e;
  }
};
template <unsigned int Size, int W, int I, int E, ac_q_mode Q>
Marshaller<Size>& operator&(Marshaller<Size> &m, ac_float<W,I,E,Q> &rhs)
{
  typedef ac_float<W,I,E,Q> Type;
  m.template AddField<Type::mant_t, Type::width>(rhs.m);
  m.template AddField<Type::exp_t, Type::e_width>(rhs.e);
  return m;
}
#endif //__MARSHALLER_AC_FLOAT_H

#if defined(__AC_COMPLEX_H) && !defined(__MARSHALLER_AC_COMPLEX_H)
#define __MARSHALLER_AC_COMPLEX_H

template <class T>
class Wrapped<ac_complex<T> >
{
public:
  ac_complex<T> val;
  Wrapped() {}
  Wrapped(const ac_complex<T> &v) : val(v) {}
  static const unsigned int width = 2 * Wrapped<T>::width;
  static const bool is_signed = Wrapped<T>::is_signed;
  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    m &val._r;
    m &val._i;
  }
};

template <unsigned int Size, class T>
Marshaller<Size> &operator&(Marshaller<Size> &m, ac_complex<T> &rhs)
{
  m.template AddField<T, Wrapped<T>::width>(rhs._r);
  m.template AddField<T, Wrapped<T>::width>(rhs._i);
  return m;
}
#endif  // __AC_COMPLEX_H

#if defined(_INCLUDED_AC_ARRAY_H_) && !defined(__MARSHALLER_INCLUDED_AC_ARRAY_H_)
#define __MARSHALLER_INCLUDED_AC_ARRAY_H_
//No dims specialization
template<typename T>
class Wrapped<ac_array<T,0,0,0> >
{
public:
  typedef ac_array<T,0,0,0> Type;
  typedef Wrapped<T> WType;
  Type val; 
  Wrapped() {}
  Wrapped(const Type &v) {
    val[0] = v[0];
  }
  static const unsigned int width = WType::width;
  static const bool is_signed = WType::is_signed;
  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    m &val[0];
  }
};
template<unsigned int Size, typename T>
Marshaller<Size>& operator&(Marshaller<Size> &m, ac_array<T,0,0,0> &rhs)
{
  m &rhs[0];
  return m;
}

template<typename T, unsigned D1, unsigned D2, unsigned D3>
class Wrapped<ac_array<T, D1, D2, D3> >
{
public:
  typedef ac_array<T, D1, D2, D3> Type;
  typedef ac_array<T, D2, D3> FType;
  typedef Wrapped<FType> WType;
  Type val; 
  Wrapped() {}
  Wrapped(const Type &v) {
    for (unsigned i=0; i<D1; i++) {
      val[i] = v[i];
    }
  }
  static const unsigned int width = WType::width * D1;
  static const bool is_signed = WType::is_signed;
  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    for (unsigned i=0; i<D1; i++) {
      m &val[i];
    }
  }
};
template<unsigned int Size, typename T, unsigned D1, unsigned D2, unsigned D3>
Marshaller<Size>& operator&(Marshaller<Size> &m, ac_array<T, D1, D2, D3> &rhs)
{
  for (unsigned i=0; i<D1; i++) {
    m &rhs[i];
  }
  return m;
}
#endif //_INCLUDED_AC_ARRAY_H_

#if defined( __CCS_P2P_H) && !defined(__MARSHALLER_CCS_P2P_H)
#define __MARSHALLER_CCS_P2P_H
//------------------------------------------------------------------------
// Wrapped< p2p<>::in<T> >
//------------------------------------------------------------------------
// Specialization for p2p<>::in

template <typename Message>
class Wrapped<p2p<>::in<Message> >
{
public:
  p2p<>::in<Message> val;
  Wrapped() : val("in") {}
  Wrapped(const p2p<>::in<Message> &v) : val(v) {}
  static const unsigned int width = Wrapped<Message>::width;
  static const bool is_signed = Wrapped<Message>::is_signed;
  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    m &val;
  }
};

template <unsigned int Size, typename Message>
Marshaller<Size> &operator&(Marshaller<Size> &m, p2p<>::in<Message> &rhs)
{
  m.template AddField<Message, Wrapped<Message>::width>(rhs);
  return m;
}

//------------------------------------------------------------------------
// Wrapped< p2p<>::out<T> >
//------------------------------------------------------------------------
// Specialization for p2p<>::out

template <typename Message>
class Wrapped<p2p<>::out<Message> >
{
public:
  p2p<>::out<Message> val;
  Wrapped() : val("out") {}
  Wrapped(const p2p<>::out<Message> &v) : val(v) {}
  static const unsigned int width = Wrapped<Message>::width;
  static const bool is_signed = Wrapped<Message>::is_signed;
  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    m &val;
  }
};

template <unsigned int Size, typename Message>
Marshaller<Size> &operator&(Marshaller<Size> &m, p2p<>::out<Message> &rhs)
{
  m.template AddField<Message, Wrapped<Message>::width>(rhs);
  return m;
}

//------------------------------------------------------------------------
// Wrapped< p2p<>::chan<T> >
//------------------------------------------------------------------------
// Specialization for p2p<>::chan

template <typename Message>
class Wrapped<p2p<>::chan<Message> >
{
public:
  p2p<>::chan<Message> val;
  Wrapped() : val("chan") {}
  Wrapped(const p2p<>::chan<Message> &v) : val(v) {}
  static const unsigned int width = Wrapped<Message>::width;
  static const bool is_signed = Wrapped<Message>::is_signed;
  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    m &val;
  }
};

template <unsigned int Size, typename Message>
Marshaller<Size> &operator&(Marshaller<Size> &m, p2p<>::chan<Message> &rhs)
{
  m.template AddField<Message, Wrapped<Message>::width>(rhs);
  return m;
}
#endif   // __CCS_P2P_H

#ifndef __CONNECTIONS__MARSHALLER_H_
#define __CONNECTIONS__MARSHALLER_H_

/* Wrapped class specialization helper for arrays. */
template <class T, unsigned int N>
class Wrapped<T[N]>
{
public:
  typedef T Type[N];
  Type val;
  Wrapped() {}
  Wrapped(const Type &v) {
    for (unsigned int i=0; i<N; i++) {
      val[i] = v[i];
    }
  }
  static const unsigned int width = N * Wrapped<T>::width;
  static const bool is_signed = Wrapped<T>::is_signed;
  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    for (unsigned int i=0; i<N; i++) {
      m &val[i];
    }
  }
};

template<unsigned int Size, typename T, unsigned int N>
Marshaller<Size>& operator&(Marshaller<Size> &m, T (&rhs)[N])
{
  for (unsigned int i=0; i<N; i++) {
    m &rhs[i];
  }
  return m;
}

/**
 * \brief StaticMax Class: returns the larger value between two unsigned integers
 * \ingroup StaticMax
 *
 * \par A Simple Example
 * \code
 *  #include <connections/marshaller.h>
 *
 *  ...
 *  static const unsigned int larger_width =
 *     StaticMax<width1, width1>::value
 *
 * \endcode
 * \par
 *
 */
template <unsigned int A, unsigned int B>
class StaticMax
{
public:
  static const unsigned int value = A > B ? A : B;
};

/**
 * \brief  BitUnion2 class: A union class to hold two Marshallers.
 * \ingroup BitUnion2
 *
 * \par A Simple Example
 * \code
 *  #include <connections/marshaller.h>
 *
 *  ...
 *  typedef BitUnion2<TypeA, TypeB> UnionType;
 *  UnionType type_union;
 *  TypeA type_a;
 *  TypeB type_b;
 *  ....
 *  type_union.Set(type_a); // Assign variable of TypeA to TypeUnion
 *  if (type_union.IsA()) {
 *    TypeA tmp = type_union.GetA();
 *  }
 *
 *  type_union.Set(type_b); // Assign variable of TypeB to TypeUnion
 *  if (type_union.IsB()) {
 *    TypeB tmp = type_union.GetB();
 *  }
 *
 * \endcode
 * \par
 *
 */
template <typename A, typename B>
class BitUnion2
{
public:
  static const unsigned int larger_width =
    StaticMax<Wrapped<A>::width, Wrapped<B>::width>::value;
  static const unsigned int width = larger_width + 1;
  static const bool is_signed = false;

  /* Constructors. */
  BitUnion2() : payload(0), tag(0) {}
  BitUnion2(const A &initdata) : payload(0), tag(0) { Set(initdata); }
  BitUnion2(const B &initdata) : payload(0), tag(1) { Set(initdata); }

  bool IsA() const { return tag == 0; }
  bool IsB() const { return tag == 1; }

  A GetA() const {
    CONNECTIONS_SIM_ONLY_ASSERT_MSG(tag == 0, "Tag doesn't match request! Use GetB() instead.");
    Marshaller<width> m(payload);
    Wrapped<A> result;
    result.template Marshall<width>(m);
    return result.val;
  }
  B GetB() const {
    CONNECTIONS_SIM_ONLY_ASSERT_MSG(tag == 1, "Tag doesn't match request! Use GetA() instead.");
    Marshaller<width> m(payload);
    Wrapped<B> result;
    result.template Marshall<width>(m);
    return result.val;
  }

  void Set(const A &data) {
    Wrapped<A> wdata(data);
    Marshaller<Wrapped<A>::width> m;
    wdata.template Marshall<Wrapped<A>::width>(m);
    payload = 0; //note, we could directly assign m.GetResult here. Assuming payload is wider, systemC would pad with zeros.
    //but if payload is narrower, systemc would silently truncate. We'd want to avoid that. Range below would
    //fail if payload is narrower
    payload.range(Wrapped<A>::width - 1, 0) = m.GetResult();
    tag = 0;
  }
  void Set(const B &data) {
    Wrapped<B> wdata(data);
    Marshaller<Wrapped<B>::width> m;
    wdata.template Marshall<Wrapped<B>::width>(m);
    payload = 0; //same as in Set(A) above
    payload.range(Wrapped<B>::width - 1, 0) = m.GetResult();
    tag = 1;
  }

  template <unsigned int Size>
  void Marshall(Marshaller<Size> &m) {
    m &payload;
    m &tag;
  }

protected:
  /* Stores the value of the current Marshaller.*/
  sc_lv<larger_width> payload;
  /* Signal whether A or B is set (0 for A and 1 for B). */
  sc_lv<1> tag;
};

template <class T>
void set_default_value(T& v) {
  v = T();
}

template <int W, bool S=true>
void set_default_value(ac_int<W,S>& v) {
  v = ac_int<W, S>(0);
}

template <int W, int I, bool S=true>
void set_default_value(ac_fixed<W, I, S>& v) {
  v = ac_fixed<W, I, S>(0);
}

#endif  // __CONNECTIONS__MARSHALLER_H_
