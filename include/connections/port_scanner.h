

/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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
// port_scanner is used by the python-based wrapper generator utility
//
// Revision History:
//  1.2.0 - Initial version
//
//*****************************************************************************************

#ifndef __CONNECTIONS__PORT_SCANNER_H__
#define __CONNECTIONS__PORT_SCANNER_H__

#include <systemc>
namespace Connections
{

// Port scanner for wrapping modules with wrapper_gen.py
#ifdef CONNECTIONS_SIM_ONLY
#ifndef _UTIL_DATNAME_
// Port names defined in connections.h - but port_scanner also needs this
#if defined(CONNECTIONS_NAMING_ORIGINAL)
#define _UTIL_DATNAME_ msg
#else
#define _UTIL_DATNAME_ dat
#endif
#endif

  class port_scanner
  {
  public:
    bool is_descendent_of(sc_object *child, sc_object *ancestor) {
      sc_object *parent = child->get_parent_object();

      if (!parent) { return false; }

      if (parent == ancestor) { return true; }

      return is_descendent_of(parent, ancestor);
    }

    void scan_port(sc_object *obj, sc_object *wrap_object, std::ostream &os) {
      if (Connections::in_port_marker *p = dynamic_cast<Connections::in_port_marker *>(obj)) {
        p->end_of_elaboration();

        if (p->bound_to && p->top_port) {
          if (!is_descendent_of(p->bound_to, wrap_object)) {
            os << "In " << std::dec << p->w << " " << p->_UTIL_DATNAME_->name() << "\n";
          }
        }
      }

      if (Connections::out_port_marker *p = dynamic_cast<Connections::out_port_marker *>(obj)) {
        p->end_of_elaboration();

        if (p->bound_to && p->top_port) {
          if (!is_descendent_of(p->bound_to, wrap_object)) {
            os << "Out " << std::dec << p->w << " " << p->_UTIL_DATNAME_->name() << "\n";
          }
        }
      }
    }

    void scan_hierarchy(sc_object *obj, sc_object *wrap_object, std::ostream &os) {
      scan_port(obj, wrap_object, os);

      std::vector<sc_object *> children = obj->get_child_objects();
      for ( unsigned i = 0; i < children.size(); i++ ) {
        if ( children[i] ) { scan_hierarchy( children[i], wrap_object, os); }
      }
    }

    int scan(const char *name, const char *fname) {
      sc_start(1, SC_PS); // forces elaboration to complete
      sc_object *o = sc_find_object(name);

      if (!o) { return 1; }

      std::ofstream ofs(fname, std::ofstream::out);
      scan_hierarchy(o, o, ofs);
      ofs.close();

      return 0;
    }
  };

#ifdef __GNUC__
#define _ATTRIB_NOINLINE_ __attribute__ ((noinline))
#else
#ifdef _MSC_VER
#define _ATTRIB_NOINLINE_ __declspec(noinline)
#else
#define _ATTRIB_NOINLINE_
#endif
#endif
  _ATTRIB_NOINLINE_ static int port_scan(const char *name, const char *fname)
  {
    if (!name) { return 0; }
    (void)port_scan(0, 0); // to get rid of unused function warnings

    Connections::port_scanner p;
    return p.scan(name, fname);
  }

#undef _ATTRIB_NOINLINE_
#undef _UTIL_DATNAME_
#endif // CONNECTIONS_SIM_ONLY

}  // namespace Connections

#endif  // __CONNECTIONS__PORT_SCANNER_H__

