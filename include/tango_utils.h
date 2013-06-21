#ifndef _TANGO_UTILS_H____dffeFDK3099e_
#define _TANGO_UTILS_H____dffeFDK3099e_

#include <iostream>
#include "tango.h"
void print_exception(std::ostream & stream, const CORBA::Exception &e);
std::ostream & operator<<(std::ostream & os, const CORBA::Exception &e);

#endif // _TANGO_UTILS_H____dffeFDK3099e_
