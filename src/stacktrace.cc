#include <backward.hpp>

#include "stacktrace.h"

void PrintStack()
{
  backward::StackTrace st;
  st.load_here( 32 );
  backward::Printer p;
  p.print( st );
  return;
}
