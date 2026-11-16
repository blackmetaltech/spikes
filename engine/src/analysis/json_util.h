// Tiny JSON number helper shared by the analysis modules (NaN / inf -> null).
#pragma once
#include <cmath>
#include <cstdio>
#include <sstream>

namespace spike::analysis::detail
{
inline void jnum( std::ostringstream& o, double v )
{
  if ( !std::isfinite( v ) )
    o << "null";
  else
  {
    char buf[ 40 ];
    std::snprintf( buf, sizeof buf, "%.10g", v );
    o << buf;
  }
}
} // namespace spike::analysis::detail
