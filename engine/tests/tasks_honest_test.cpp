// Valid circuits must not be rejected (full review, P1-5), and a quick pass must hold in the full episode for circuits that are NOT the reference (P1-3, the leech heartbeat at several adaptation times).
//   t_tuned        a circuit wired cell by cell (per-edge weights, one projection) is read as wired: it passes
//   m_adaptation   a mat2 cell with its threshold steps switched on, the sfa cell and the gif cell are adapting cells: the verdict never says the cell has no adaptation current
//   m_rhythm       the preBotzinger burster (butera_burster) is a bursting cell: the verdict never says it cannot burst
//   leech_heart    reference-like circuits with tau_w 2200 .. 3400 ms: a quick pass implies a full pass, in standard and researcher, on several seeds
#include <cstdio>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

namespace
{
json fixture( const std::string& name )
{
  return json::parse( chk::slurp( std::string( SPIKE_SOURCE_DIR ) + "/engine/tests/cheats/" + name + ".json" ) );
}
struct Res
{
  bool passed = false, loaded = false;
  double value = 0;
  std::string verdict;
};
Res run( const std::string& id, const json& c, uint64_t seed, bool quick, const std::string& mode = "standard" )
{
  Res r;
  Engine e;
  json cc = c;
  cc[ "seed" ] = 1000 * seed + c.value( "seed", 1 );
  r.loaded = e.load( cc.dump() );
  if ( !r.loaded ) { r.verdict = e.error(); return r; }
  auto t = make_task( id );
  t->set_mode( mode );
  t->set_quick( quick );
  t->reset( seed );
  for ( long s = 0; s < static_cast< long >( quick ? t->quick_duration_ms() : t->duration_ms() ); ++s ) t->step( e, 1.0 );
  const Score sc = t->score();
  r.passed = sc.passed;
  r.value = sc.value;
  r.verdict = sc.verdict;
  return r;
}
}  // namespace

int main()
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  {
    const Res r = run( "t_tuned", fixture( "t_tuned_per_edge" ), 1, true );
    std::printf( "t_tuned per-edge: %s | %s\n", r.passed ? "PASS" : "fail", r.verdict.c_str() );
    CHECK_MSG( r.loaded && r.passed, "t_tuned: a per-edge circuit that concentrates the weights on the preferred afferents must pass: " + r.verdict );
  }
  {
    json mat2 = fixture( "m_adaptation_mat2" );
    const Res r = run( "m_adaptation", mat2, 1, true );
    std::printf( "m_adaptation mat2: %s | %s\n", r.passed ? "PASS" : "fail", r.verdict.c_str() );
    CHECK_MSG( r.loaded && r.verdict.find( "no adaptation current" ) == std::string::npos, "m_adaptation: a mat2 cell with its steps on is an adapting cell: " + r.verdict );
    for ( const char* model : { "iaf_cond_exp_sfa_rr", "gif_cond_exp" } )
    {
      json c = fixture( "m_adaptation_mat2" );
      c[ "populations" ][ 0 ][ "model" ] = model;
      c[ "populations" ][ 0 ][ "params" ] = json::object();
      const Res q = run( "m_adaptation", c, 1, true );
      std::printf( "m_adaptation %s: %s | %s\n", model, q.passed ? "PASS" : "fail", q.verdict.c_str() );
      CHECK_MSG( q.loaded && q.verdict.find( "no adaptation current" ) == std::string::npos, std::string( "m_adaptation: " ) + model + " is an adapting cell: " + q.verdict );
    }
  }
  {
    const Res r = run( "m_rhythm", fixture( "m_rhythm_butera_10" ), 1, true );
    std::printf( "m_rhythm butera: %s | %s\n", r.passed ? "PASS" : "fail", r.verdict.c_str() );
    CHECK_MSG( r.loaded && r.verdict.find( "cannot burst" ) == std::string::npos, "m_rhythm: butera_burster is a bursting cell: " + r.verdict );
  }
  for ( int tw : { 2200, 2600, 3000, 3400 } )
    for ( const char* mode : { "standard", "researcher" } )
      for ( uint64_t seed : { 1, 2, 3, 101, 102 } )
      {
        const json c = fixture( "leech_heart_tauw" + std::to_string( tw ) );
        const Res q = run( "leech_heart", c, seed, true, mode );
        if ( !q.passed ) continue;
        const Res f = run( "leech_heart", c, seed, false, mode );
        CHECK_MSG( f.passed, "leech_heart tau_w " + std::to_string( tw ) + " " + mode + " seed " + std::to_string( seed ) + ": quick passes, full fails: " + f.verdict );
      }
  return chk::finish( "tasks_honest_test" );
}
