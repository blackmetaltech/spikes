// Minimal test helpers (no framework): CHECK/CHECK_EQ record failures, finish() returns the exit code.
#pragma once
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace chk
{
inline int& failures() { static int f = 0; return f; }
inline std::string slurp( const std::string& path )
{
  std::ifstream f( path );
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
inline int finish( const char* name )
{
  std::printf( "%s: %s (%d failure%s)\n", name, failures() ? "FAILED" : "ok", failures(), failures() == 1 ? "" : "s" );
  return failures() ? 1 : 0;
}
} // namespace chk

#define CHECK( cond )                                                                \
  do                                                                                 \
  {                                                                                  \
    if ( not( cond ) )                                                               \
    {                                                                                \
      std::printf( "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond );               \
      ++chk::failures();                                                             \
    }                                                                                \
  } while ( 0 )
#define CHECK_MSG( cond, msg )                                                       \
  do                                                                                 \
  {                                                                                  \
    if ( not( cond ) )                                                               \
    {                                                                                \
      std::printf( "  FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__, #cond, std::string( msg ).c_str() ); \
      ++chk::failures();                                                             \
    }                                                                                \
  } while ( 0 )
