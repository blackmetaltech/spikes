// Negative tests for the honesty contract (docs/REQ-0.4.md rule 3): cheap or degenerate circuits must FAIL every level.
//
//   tasks_cheap_test                         the CTest: every row of cheap_rows_{a,b,c}.inc fails on seeds 1 and 2 (the game's CHECK uses the quick
//                                            episode, Task::quick_duration_ms(), so the rows run it too); every task that has rows also has its
//                                            reference circuit pass the same harness once (a control: a broken harness would "fail" everything)
//   tasks_cheap_test run <task> <circuit.json> [seed=1]    tuning aid: score one circuit, print parts and verdict
//
// HOW TO ADD ROWS (one .inc file per owner; the three files are #included inside rows() below, so a row is a statement):
//
//     CHEAP_ROW( "m_threshold", "empty bench",            cheap::empty( "m_threshold" ) );
//     CHEAP_ROW( "m_threshold", "one cell, tonic 450 pA", cheap::tonic( "m_threshold", 450 ) );
//     CHEAP_ROW( "reflex",      "reference minus convergence",
//                cheap::from_ref( "reflex", cheap::cut( "SN", "MN" ) ) );
//     CHEAP_ROW_V( "m_inhibition", "stop acts without an inhibitory cell", "inhibitory", <circuit> );   // + a word the verdict must contain
//   (both macros are variadic in the circuit, so a lambda body with commas in braces is fine: CHEAP_ROW( t, "x", [] { Bench b; ...; return b.c; }() ); )
//
// A row passes the test when the task does NOT pass (Score::passed false) on seeds 1 and 2, and says something (non-empty verdict).
// CHEAP_ROW_V takes the word BEFORE the circuit and also requires the verdict to contain it (case-insensitive), to pin that the verdict names the missing mechanism.
//
// Builders (namespace cheap, all return a circuit json that the engine can load):
//   skeleton( task )                       every sensor and motor channel declared, no targets, no cells (add cells before loading)
//   empty( task )                          skeleton + one unconnected cell "N0" (the game's empty bench)
//   single_cell( task, model, params )     one cell "N1" on every sensor (the lab's first-connect weight) and every motor channel
//   tonic( task, I_e, model )              like single_cell with a bias current I_e (pA), every sensor weight 0
//   from_ref( task, Mut )                  the reference circuit, edited (see the Mut builders below; Mut is std::function<void(json&)>)
//   zero_weights( task ) / max_weights( task )   the reference with every synapse and sensor weight set to 0 / to +-an extreme value (see max_all)
// Mut builders: cut(pre,post,factor=0), silence(pop), scale_input(name,factor) (0 = cut the sensor), scale_all_inputs(f), set_params(pop,json),
//   set_model(pop,model), set_delay(pre,post,d), scale_output_gain(name,f), both(a,b), all(a,b,c...), zero_all, max_all.
// Bench is the fluent circuit builder used by the micro tests (pop/proj/in/out) for rows that need a hand-built circuit.
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

namespace cheap
{

using Mut = std::function< void( json& ) >;

// ---------------------------------------------------------------- fluent builder
struct Bench
{
  json c = { { "schema_version", 1 }, { "name", "cheap bench" }, { "resolution_ms", 0.1 }, { "seed", 1 }, { "populations", json::array() },
    { "projections", json::array() }, { "inputs", json::array() }, { "outputs", json::array() }, { "record", { { "spikes", "all" } } } };
  Bench& pop( const std::string& id, const std::string& model = "iaf_psc_alpha", int n = 1, json params = json::object() )
  {
    c[ "populations" ].push_back( { { "id", id }, { "model", model }, { "n", n }, { "params", params } } );
    return *this;
  }
  Bench& proj( const std::string& pre, const std::string& post, double w, double delay = 2.0 )
  {
    c[ "projections" ].push_back( { { "pre", pre }, { "post", post }, { "rule", "all_to_all" },
      { "synapse", { { "synapse_model", "static_synapse" }, { "weight", w }, { "delay", delay } } } } );
    return *this;
  }
  Bench& in( const std::string& name, const std::string& kind, double weight, std::vector< std::string > pops )
  {
    json t = json::array();
    for ( auto& p : pops ) t.push_back( { { "pop", p } } );
    c[ "inputs" ].push_back( { { "name", name }, { "kind", kind }, { "targets", t }, { "weight", weight }, { "value", 0.0 } } );
    return *this;
  }
  Bench& out( const std::string& name, std::vector< std::string > pops, const json& modulator = json() )
  {
    json n = json::array();
    for ( auto& p : pops ) n.push_back( { { "pop", p } } );
    json o = { { "name", name }, { "kind", "rate" }, { "neurons", n } };
    if ( !modulator.is_null() )
    {
      o[ "kind" ] = "modulator";
      o[ "modulator" ] = modulator[ "modulator" ];
      o[ "gain" ] = modulator[ "gain" ];
      o[ "tau_ms" ] = modulator[ "tau_ms" ];
    }
    c[ "outputs" ].push_back( o );
    return *this;
  }
};

// ---------------------------------------------------------------- task-derived skeletons
inline double sensor_weight( const std::string& task, const std::string& sensor )
{
  auto t = make_task( task );
  auto k = t->channel_kinds();
  auto it = k.find( sensor );
  if ( it != k.end() && it->second.weight > 0 ) return it->second.weight;
  return it != k.end() && it->second.kind == "dc" ? 300.0 : 280.0;
}

inline json modulator_of( const Task::ChannelKind& k ) { return { { "modulator", k.modulator }, { "gain", k.gain }, { "tau_ms", k.tau_ms } }; }

/// Channels declared and wired to `in_pops` / `out_pops` (sensor weight `w`, or the lab's first-connect weight when w < 0).
inline Bench channels( const std::string& task, const std::vector< std::string >& in_pops, const std::vector< std::string >& out_pops, double w = -1 )
{
  Bench b;
  auto t = make_task( task );
  auto k = t->channel_kinds();
  for ( auto& s : t->sensor_channels() )
    b.in( s, k.count( s ) ? k[ s ].kind : "poisson", w >= 0 ? w : sensor_weight( task, s ), in_pops );
  for ( auto& m : t->motor_channels() )
  {
    if ( k.count( m ) && k[ m ].kind == "modulator" ) b.out( m, out_pops, modulator_of( k[ m ] ) );
    else b.out( m, out_pops );
  }
  return b;
}

inline json skeleton( const std::string& task ) { return channels( task, {}, {} ).c; }

inline json empty( const std::string& task )
{
  Bench b = channels( task, {}, {} );
  b.pop( "N0" );  // the engine needs at least one cell to load
  return b.c;
}

inline json single_cell( const std::string& task, const std::string& model = "", json params = json::object() )
{
  const std::string m = model.empty() ? ( [ & ] {
    auto d = make_task( task )->default_model();
    return d.empty() ? std::string( "iaf_psc_alpha" ) : ( d == "izhikevich_CH" ? std::string( "izhikevich" ) : d );
  } )() : model;
  Bench b = channels( task, { "N1" }, { "N1" } );
  b.pop( "N1", m, 1, params );
  return b.c;
}

inline json tonic( const std::string& task, double I_e, const std::string& model = "iaf_psc_alpha" )
{
  Bench b = channels( task, { "N1" }, { "N1" }, 0.0 );
  b.pop( "N1", model, 1, { { "I_e", I_e } } );
  return b.c;
}

// ---------------------------------------------------------------- the reference and edits of it
inline json reference( const std::string& task )
{
  const std::string path = std::string( SPIKE_SOURCE_DIR ) + "/circuits/" + reference_circuit( task );
  const std::string txt = chk::slurp( path );
  CHECK_MSG( !txt.empty(), "cannot read " + path );
  return txt.empty() ? json::object() : json::parse( txt );
}

inline json* find_pop( json& c, const std::string& id )
{
  for ( auto& p : c[ "populations" ] )
    if ( p[ "id" ] == id ) return &p;
  return nullptr;
}

inline Mut cut( const std::string& pre, const std::string& post, double factor = 0.0 )
{
  return [ = ]( json& c ) {
    bool hit = false;
    for ( auto& p : c[ "projections" ] )
      if ( p[ "pre" ] == pre && p[ "post" ] == post )
      {
        hit = true;
        p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * factor;
      }
    CHECK_MSG( hit, "edit matched no projection " + pre + ">" + post );
  };
}
inline Mut set_delay( const std::string& pre, const std::string& post, double d )
{
  return [ = ]( json& c ) {
    for ( auto& p : c[ "projections" ] )
      if ( p[ "pre" ] == pre && p[ "post" ] == post ) p[ "synapse" ][ "delay" ] = d;
  };
}
inline Mut scale_input( const std::string& name, double factor )
{
  return [ = ]( json& c ) {
    for ( auto& i : c[ "inputs" ] )
      if ( i[ "name" ] == name )
      {
        if ( factor == 0.0 ) i[ "targets" ] = json::array();
        else i[ "weight" ] = i[ "weight" ].get< double >() * factor;
        return;
      }
    CHECK_MSG( false, "no input " + name );
  };
}
inline Mut scale_all_inputs( double factor )
{
  return [ = ]( json& c ) { for ( auto& i : c[ "inputs" ] ) i[ "weight" ] = i[ "weight" ].get< double >() * factor; };
}
inline Mut silence( const std::string& id )
{
  return [ = ]( json& c ) {
    for ( auto& p : c[ "projections" ] )
      if ( p[ "pre" ] == id || p[ "post" ] == id ) p[ "synapse" ][ "weight" ] = 0.0;
    for ( auto& i : c[ "inputs" ] )
    {
      json t = json::array();
      for ( auto& x : i[ "targets" ] ) if ( x[ "pop" ] != id ) t.push_back( x );
      i[ "targets" ] = t;
    }
    for ( auto& o : c[ "outputs" ] )
    {
      json t = json::array();
      for ( auto& x : o[ "neurons" ] ) if ( x[ "pop" ] != id ) t.push_back( x );
      o[ "neurons" ] = t;
    }
  };
}
inline Mut set_params( const std::string& id, json params )
{
  return [ = ]( json& c ) {
    json* p = find_pop( c, id );
    CHECK_MSG( p != nullptr, "no population " + id );
    if ( p ) for ( auto it = params.begin(); it != params.end(); ++it ) ( *p )[ "params" ][ it.key() ] = it.value();
  };
}
inline Mut set_model( const std::string& id, const std::string& model )
{
  return [ = ]( json& c ) {
    json* p = find_pop( c, id );
    CHECK_MSG( p != nullptr, "no population " + id );
    if ( p ) { ( *p )[ "model" ] = model; ( *p )[ "params" ] = json::object(); }
  };
}
inline Mut scale_output_gain( const std::string& name, double factor )
{
  return [ = ]( json& c ) {
    for ( auto& o : c[ "outputs" ] )
      if ( o[ "name" ] == name ) o[ "gain" ] = o[ "gain" ].get< double >() * factor;
  };
}
inline Mut both( Mut a, Mut b ) { return [ = ]( json& c ) { a( c ); b( c ); }; }
template < class... M > Mut all( Mut a, M... rest )
{
  if constexpr ( sizeof...( rest ) == 0 ) return a;
  else return both( a, all( rest... ) );
}
/// Every synapse and sensor weight to 0.
inline void zero_all( json& c )
{
  for ( auto& p : c[ "projections" ] )
  {
    if ( p.contains( "synapse" ) ) p[ "synapse" ][ "weight" ] = 0.0;
    if ( p.contains( "edges" ) ) for ( auto& e : p[ "edges" ] ) e[ 2 ] = 0.0;
  }
  for ( auto& i : c[ "inputs" ] ) i[ "weight" ] = 0.0;
}
/// Every synapse and sensor weight pushed far up, keeping its sign: 4000 pA onto current cells; onto conductance cells twice its own value (the
/// reference's inhibition of a motor pool is already 28 nS: about four times the reference crashes NEST's integrator, see the report); sensors 4000
/// (poisson) or 8000 (dc).
inline void max_all( json& c )
{
  auto cond = [ & ]( const std::string& pop ) {
    for ( auto& p : c[ "populations" ] )
      if ( p[ "id" ] == pop ) return p[ "model" ].get< std::string >().find( "cond" ) != std::string::npos || p[ "model" ] == "sa_node_pacemaker";
    return false;
  };
  auto post_of = [ & ]( const json& p ) { return p[ "post" ].is_string() ? p[ "post" ].get< std::string >() : p[ "post" ].value( "pop", std::string() ); };
  for ( auto& p : c[ "projections" ] )
  {
    const bool cd = cond( post_of( p ) );
    auto big = [ & ]( double w ) { return cd ? 2.0 * w : ( w < 0 ? -4000.0 : 4000.0 ); };
    if ( p.contains( "synapse" ) ) p[ "synapse" ][ "weight" ] = big( p[ "synapse" ][ "weight" ].get< double >() );
    if ( p.contains( "edges" ) )
      for ( auto& e : p[ "edges" ] ) e[ 2 ] = big( e[ 2 ].get< double >() );
  }
  for ( auto& i : c[ "inputs" ] )
  {
    const double w = i[ "weight" ].get< double >();
    i[ "weight" ] = ( w < 0 ? -1.0 : 1.0 ) * ( i.value( "kind", "" ) == "poisson" ? 4000.0 : 8000.0 );
  }
}
inline json from_ref( const std::string& task, Mut m )
{
  json c = reference( task );
  m( c );
  return c;
}
inline json zero_weights( const std::string& task ) { return from_ref( task, zero_all ); }
inline json max_weights( const std::string& task ) { return from_ref( task, max_all ); }

}  // namespace cheap

namespace
{

struct Row
{
  std::string task, label;
  json circuit;
  std::string word;  // the verdict must contain it (case-insensitive); "" = no check
};

struct Result
{
  Score score;
  bool loaded = false;
  std::string error;
};

// Runs the quick episode, exactly as spike-run --quick does (the game's CHECK).
Result run_circuit( const std::string& id, const json& circuit_in, uint64_t seed, bool quick = true, const std::string& mode = "standard" )
{
  Result r;
  Engine e;
  json c = circuit_in;
  c[ "seed" ] = 1000 * seed + circuit_in.value( "seed", 1 );
  r.loaded = e.load( c.dump() );
  if ( !r.loaded )
  {
    r.error = e.error();
    return r;
  }
  auto t = make_task( id );
  t->set_mode( mode );
  t->reset( seed );
  const long steps = static_cast< long >( quick ? t->quick_duration_ms() : t->duration_ms() );
  for ( long s = 0; s < steps; ++s )
    t->step( e, 1.0 );
  r.score = t->score();
  return r;
}

std::string lower( std::string s )
{
  for ( auto& ch : s ) ch = static_cast< char >( std::tolower( static_cast< unsigned char >( ch ) ) );
  return s;
}

void rows( std::vector< Row >& R )
{
#define CHEAP_ROW( TASK, LABEL, ... ) R.push_back( { TASK, LABEL, __VA_ARGS__, "" } )
#define CHEAP_ROW_V( TASK, LABEL, WORD, ... ) R.push_back( { TASK, LABEL, __VA_ARGS__, WORD } )
  using namespace cheap;
  // each file is its own block (locals of one cannot clash with another's) and its own try: a row builder that throws is reported with its file
  try
  {
#include "cheap_rows_a.inc"
  }
  catch ( const std::exception& ex ) { CHECK_MSG( false, std::string( "cheap_rows_a.inc threw: " ) + ex.what() ); }
  try
  {
#include "cheap_rows_b.inc"
  }
  catch ( const std::exception& ex ) { CHECK_MSG( false, std::string( "cheap_rows_b.inc threw: " ) + ex.what() ); }
  try
  {
#include "cheap_rows_c.inc"
  }
  catch ( const std::exception& ex ) { CHECK_MSG( false, std::string( "cheap_rows_c.inc threw: " ) + ex.what() ); }
#undef CHEAP_ROW
#undef CHEAP_ROW_V
}

}  // namespace

int main( int argc, char** argv )
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );  // a crash inside NEST must not swallow the rows already printed
  if ( argc >= 4 && std::string( argv[ 1 ] ) == "run" )
  {
    json c = json::parse( chk::slurp( argv[ 3 ] ) );
    Result r = run_circuit( argv[ 2 ], c, argc > 4 ? std::stoull( argv[ 4 ] ) : 1 );
    std::printf( "value %5.1f %s |", r.score.value, r.score.passed ? "PASS" : "fail" );
    for ( auto& [ k, v ] : r.score.parts ) std::printf( " %s=%.3g", k.c_str(), v );
    std::printf( "\n  %s\n", r.score.verdict.c_str() );
    return 0;
  }

  std::vector< Row > R;
  rows( R );
  std::printf( "%zu cheap-circuit rows\n", R.size() );
  std::map< std::string, int > per_task;
  const auto t0 = std::chrono::steady_clock::now();
  // tuning aid: CHEAP_TASK=a,b only runs the rows (and controls) of tasks whose id equals one of the listed ids or starts with it followed by nothing else
  std::set< std::string > only;
  if ( const char* o = std::getenv( "CHEAP_TASK" ) )
  {
    std::stringstream ss( o );
    for ( std::string tok; std::getline( ss, tok, ',' ); ) only.insert( tok );
  }
  for ( const auto& row : R )
  {
    if ( !only.empty() && !only.count( row.task ) ) continue;
    ++per_task[ row.task ];
    CHECK_MSG( make_task( row.task ) != nullptr, "row for unknown task " + row.task );
    if ( !make_task( row.task ) ) continue;
    for ( uint64_t seed : { 1, 2 } )
    {
      Result r = run_circuit( row.task, row.circuit, seed );
      CHECK_MSG( r.loaded, row.task + " / " + row.label + ": circuit does not load: " + r.error );
      if ( !r.loaded ) break;
      if ( seed == 1 )
        std::printf( "%-22s %-52s %5.1f  %s\n", row.task.c_str(), row.label.c_str(), r.score.value, r.score.verdict.substr( 0, 110 ).c_str() );
      CHECK_MSG( !r.score.passed, row.task + " / " + row.label + " PASSES (seed " + std::to_string( seed ) + "): " + r.score.verdict );
      CHECK_MSG( !r.score.verdict.empty(), row.task + " / " + row.label + ": empty verdict" );
      if ( !row.word.empty() )
        CHECK_MSG( lower( r.score.verdict ).find( lower( row.word ) ) != std::string::npos,
          row.task + " / " + row.label + ": the verdict should mention '" + row.word + "': " + r.score.verdict );
    }
  }
  // control: the reference passes through the same harness, so "everything fails" cannot come from a broken harness
  std::printf( "controls (reference circuit, quick episode, seed 1):\n" );
  for ( const auto& [ task, n ] : per_task )
  {
    if ( !make_task( task ) ) continue;
    Result r = run_circuit( task, cheap::reference( task ), 1 );
    std::printf( "  %-22s %3d rows  reference %s\n", task.c_str(), n, r.score.passed ? "passes" : "FAILS" );
    CHECK_MSG( r.loaded && r.score.passed, task + ": the reference must pass the harness: " + r.score.verdict );
  }
  std::printf( "%.1f s\n", std::chrono::duration< double >( std::chrono::steady_clock::now() - t0 ).count() );
  return chk::finish( "tasks_cheap_test" );
}
