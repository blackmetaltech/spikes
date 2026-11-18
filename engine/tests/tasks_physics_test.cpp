// World-physics tests for the tasks, with NO NEST: spike::Engine is replaced by a scripted stub defined in this file
// (the test is compiled with engine/src/tasks/*.cpp only and does not link spike::engine).
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include "spike/task.h"
#include "../src/tasks/task_util.h"

using namespace spike;

// ---- stub Engine -----------------------------------------------------------------------------------------------
struct StubBrain
{
  // Called once per step() with the model time at the START of the slice and the inputs set so far; fills motor Hz
  // (summed over the pool) per channel.
  std::function< void( double t_ms, const std::map< std::string, double >& in, std::map< std::string, double >& motor ) > f;
  std::map< std::string, int > pool_size;  // neurons per output channel
};
static StubBrain* g_brain = nullptr;

struct Engine::Impl
{
  std::map< std::string, double > in, out;
  double t = 0;
};
Engine::Engine() : impl_( new Impl ) {}
Engine::~Engine() { delete impl_; }
bool Engine::load( const std::string& ) { return true; }
const std::string& Engine::error() const { static std::string e; return e; }
int Structure::population_index( const std::string& id ) const
{
  for ( size_t i = 0; i < populations.size(); ++i )
    if ( populations[ i ].id == id ) return static_cast< int >( i );
  return -1;
}
const PopulationInfo* Structure::population( const std::string& id ) const
{
  const int i = population_index( id );
  return i < 0 ? nullptr : &populations[ static_cast< size_t >( i ) ];
}
Structure Engine::structure() const { return Structure(); }  // no circuit behind the stub: the tasks' mechanism checks find nothing to judge and stay quiet
double Engine::modulator( const std::string& ) const { return 0.0; }
bool Engine::set_modulator( const std::string&, double ) { return true; }
int32_t Engine::neuron_count() const { return 0; }
std::string Engine::neuron_population( int32_t ) const { return ""; }
std::vector< ChannelInfo > Engine::inputs() const
{
  // every sensor channel of the three tasks is wired to one neuron
  std::vector< ChannelInfo > v;
  for ( const char* name : { "drive", "touch", "ASEL", "ASER" } )
  {
    ChannelInfo c;
    c.name = name;
    c.kind = "poisson";
    c.neurons.push_back( 0 );
    v.push_back( c );
  }
  return v;
}
std::vector< ChannelInfo > Engine::outputs() const
{
  std::vector< ChannelInfo > v;
  for ( auto& [ name, n ] : g_brain->pool_size )
  {
    ChannelInfo c;
    c.name = name;
    c.kind = "rate";
    for ( int i = 0; i < n; ++i ) c.neurons.push_back( i );
    v.push_back( c );
  }
  return v;
}
void Engine::set_input( const std::string& ch, double v ) { impl_->in[ ch ] = v; }
void Engine::step( double ms )
{
  impl_->out.clear();
  g_brain->f( impl_->t, impl_->in, impl_->out );
  impl_->t += ms;
}
std::vector< Spike > Engine::drain_spikes() { return {}; }
uint64_t Engine::dropped() const { return 0; }
double Engine::output_rate( const std::string& ch ) const { auto it = impl_->out.find( ch ); return it == impl_->out.end() ? 0.0 : it->second; }
void Engine::watch( int32_t ) {}
double Engine::voltage( int32_t ) const { return 0; }
bool Engine::set_weight( int32_t, int32_t, double ) { return true; }
bool Engine::set_neuron_param( int32_t, const std::string&, double ) { return true; }
RunStats Engine::stats() const { return {}; }
void Engine::stop() {}
void spike::set_threads( int ) {}

// ---- helpers ---------------------------------------------------------------------------------------------------
static int g_fail = 0;
#define CHECK( c ) do { if ( !( c ) ) { std::printf( "  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while ( 0 )

static Score run_task( Task& t, StubBrain& b, uint64_t seed, double dt = 1.0 )
{
  g_brain = &b;
  Engine e;
  t.reset( seed );
  for ( double x = 0; x < t.duration_ms(); x += dt )
    t.step( e, dt );
  return t.score();
}
static void show( const char* what, const Score& s )
{
  std::printf( "  %-34s value %5.1f passed %d  |", what, s.value, s.passed );
  for ( auto& [ k, v ] : s.parts ) std::printf( " %s=%.3g", k.c_str(), v );
  std::printf( "\n    %s\n", s.verdict.c_str() );
}

// ---- reflex ----------------------------------------------------------------------------------------------------
static void test_reflex()
{
  std::printf( "reflex\n" );
  auto t = make_task( "reflex" );
  CHECK( t->sensor_channels() == std::vector< std::string >{ "touch" } );
  // Ideal arc: a 100 ms, 3-cell burst starts 25 ms after the touch input exceeds 200 Hz (noise at 15 Hz is ignored).
  {
    double burst_start = -1;
    StubBrain b;
    b.pool_size[ "withdraw" ] = 3;
    b.f = [ & ]( double tm, const auto& in, auto& out ) {
      double touch = in.count( "touch" ) ? in.at( "touch" ) : 0;
      if ( touch > 200 && burst_start < 0 ) burst_start = tm + 25;
      if ( burst_start >= 0 && tm >= burst_start && tm < burst_start + 100 ) out[ "withdraw" ] = 3 * 100.0;  // 3 cells at 100 Hz
      if ( burst_start >= 0 && tm >= burst_start + 100 ) burst_start = -1;
    };
    Score s = run_task( *t, b, 3 );
    show( "ideal arc", s );
    CHECK( s.passed );
    CHECK( s.parts[ "misses" ] == 0 && s.parts[ "false_triggers" ] == 0 );
    CHECK( s.parts[ "latency_ms" ] > 20 && s.parts[ "latency_ms" ] < 40 );
    // determinism
    Score s2 = run_task( *t, b, 3 );
    CHECK( s2.value == s.value && s2.parts == s.parts );
  }
  // One false withdrawal on an otherwise perfect arc (a lone cell that answers every poke but also fires once on the chatter): Standard does not pass it,
  // Guided allows one. The convergence lesson is only enforced if a single stray burst costs the pass.
  {
    double burst_start = -1;
    StubBrain b;
    b.pool_size[ "withdraw" ] = 3;
    b.f = [ & ]( double tm, const auto& in, auto& out ) {
      double touch = in.count( "touch" ) ? in.at( "touch" ) : 0;
      if ( touch > 200 && burst_start < 0 ) burst_start = tm + 25;
      if ( burst_start >= 0 && tm >= burst_start && tm < burst_start + 100 ) out[ "withdraw" ] = 3 * 100.0;
      if ( burst_start >= 0 && tm >= burst_start + 100 ) burst_start = -1;
      if ( tm >= 700 && tm < 760 ) out[ "withdraw" ] = 3 * 100.0;  // a burst with no poke (the first poke is after 1500 ms)
    };
    for ( const char* mode : { "standard", "researcher", "guided" } )
    {
      burst_start = -1;
      t->set_mode( mode );
      Score s = run_task( *t, b, 3 );
      show( ( std::string( "one false burst, " ) + mode ).c_str(), s );
      CHECK( s.parts[ "false_triggers" ] == 1 && s.parts[ "misses" ] == 0 );
      CHECK( s.passed == ( std::string( mode ) == "guided" ) );
      CHECK( s.verdict.find( "Clean" ) == std::string::npos );  // a run with a false withdrawal is not called clean
    }
    t->set_mode( "standard" );
  }
  // Silent circuit: six misses.
  {
    StubBrain b; b.pool_size[ "withdraw" ] = 3; b.f = []( double, const auto&, auto& ) {};
    Score s = run_task( *t, b, 3 );
    show( "silent", s );
    CHECK( !s.passed && s.parts[ "misses" ] == 6 && s.value == 0 );
  }
  // Twitchy circuit: bursts on every noise spike (touch > 0 => burst, 25 ms later), so it fires constantly.
  {
    StubBrain b; b.pool_size[ "withdraw" ] = 3;
    b.f = []( double tm, const auto&, auto& out ) { if ( std::fmod( tm, 300.0 ) < 20.0 ) out[ "withdraw" ] = 400; };
    Score s = run_task( *t, b, 3 );
    show( "twitchy", s );
    CHECK( !s.passed && s.parts[ "false_triggers" ] > 5 );
  }
  // Slow circuit: responds but 90 ms late: counts as miss (deadline 150 ms incl. detector) or too slow.
  {
    double start = -1;
    StubBrain b; b.pool_size[ "withdraw" ] = 3;
    b.f = [ & ]( double tm, const auto& in, auto& out ) {
      double touch = in.count( "touch" ) ? in.at( "touch" ) : 0;
      if ( touch > 200 && start < 0 ) start = tm + 90;
      if ( start >= 0 && tm >= start && tm < start + 60 ) out[ "withdraw" ] = 300;
      if ( start >= 0 && tm >= start + 60 ) start = -1;
    };
    Score s = run_task( *t, b, 3 );
    show( "slow (90 ms)", s );
    CHECK( !s.passed );
  }
  // Different seeds give different touch times: check via a recorder of when touch goes high.
  {
    auto first_touch = [ & ]( uint64_t seed ) {
      double ft = -1;
      StubBrain b; b.pool_size[ "withdraw" ] = 3;
      b.f = [ & ]( double tm, const auto& in, auto& ) { if ( ft < 0 && in.at( "touch" ) > 200 ) ft = tm; };
      run_task( *t, b, seed );
      return ft;
    };
    CHECK( first_touch( 1 ) != first_touch( 2 ) );
    CHECK( first_touch( 5 ) == first_touch( 5 ) );
  }
}

// ---- lamprey ---------------------------------------------------------------------------------------------------
static StubBrain osc( double f_hz, double phase_deg, double peak_hz, int n = 4 )
{
  StubBrain b;
  b.pool_size[ "left_motor" ] = n;
  b.pool_size[ "right_motor" ] = n;
  b.f = [ = ]( double tm, const auto&, auto& out ) {
    double ph = 2 * tasks::kPi * f_hz * tm / 1000.0;
    double l = std::sin( ph ), r = std::sin( ph + phase_deg * tasks::kPi / 180.0 );
    out[ "left_motor" ] = n * peak_hz * std::max( 0.0, l );
    out[ "right_motor" ] = n * peak_hz * std::max( 0.0, r );
  };
  return b;
}
static void test_lamprey()
{
  std::printf( "lamprey\n" );
  auto t = make_task( "lamprey" );
  for ( double f : { 1.5, 3.0, 5.0 } )
  {
    StubBrain b = osc( f, 180, 60 );
    char name[ 64 ]; std::snprintf( name, sizeof name, "anti-phase %.1f Hz", f );
    Score s = run_task( *t, b, 1 );
    show( name, s );
    CHECK( s.passed );
    CHECK( std::fabs( s.parts[ "freq_hz" ] - f ) < 0.15 * f );
    CHECK( std::fabs( s.parts[ "phase_deg" ] - 180 ) < 15 );
  }
  { StubBrain b = osc( 3, 0, 60 );   Score s = run_task( *t, b, 1 ); show( "in-phase 3 Hz", s );  CHECK( !s.passed ); }
  { StubBrain b = osc( 3, 90, 60 );  Score s = run_task( *t, b, 1 ); show( "quadrature 3 Hz", s ); CHECK( !s.passed ); }
  { StubBrain b = osc( 13, 180, 60 ); Score s = run_task( *t, b, 1 ); show( "13 Hz", s );          CHECK( !s.passed ); }
  { StubBrain b = osc( 0.5, 180, 60 ); Score s = run_task( *t, b, 1 ); show( "0.5 Hz", s );        CHECK( !s.passed ); }
  {
    StubBrain b; b.pool_size[ "left_motor" ] = 4; b.pool_size[ "right_motor" ] = 4;
    b.f = []( double, const auto&, auto& out ) { out[ "left_motor" ] = 4 * 40.0; out[ "right_motor" ] = 4 * 40.0; };
    Score s = run_task( *t, b, 1 ); show( "tonic both", s ); CHECK( !s.passed );
  }
  { StubBrain b; b.pool_size[ "left_motor" ] = 4; b.pool_size[ "right_motor" ] = 4; b.f = []( double, const auto&, auto& ) {};
    Score s = run_task( *t, b, 1 ); show( "silent", s ); CHECK( !s.passed && s.parts[ "speed" ] == 0 ); }
  // ---- scoring loophole: both sides must oscillate ----
  // One side bursts at 3 Hz, the other is completely silent: not a rhythm, no phase reported, must fail.
  {
    StubBrain b; b.pool_size[ "left_motor" ] = 4; b.pool_size[ "right_motor" ] = 4;
    b.f = []( double tm, const auto&, auto& out ) { out[ "left_motor" ] = 4 * 60.0 * std::max( 0.0, std::sin( 2 * tasks::kPi * 3 * tm / 1000.0 ) ); };
    Score s = run_task( *t, b, 1 ); show( "left only (right silent)", s );
    CHECK( !s.passed && s.parts[ "phase_deg" ] == 0 && s.parts[ "freq_hz" ] == 0 );
    CHECK( s.verdict.find( "right_motor never fired" ) != std::string::npos );
  }
  // One side bursts, the other fires tonically (never silent): still one-sided.
  {
    StubBrain b; b.pool_size[ "left_motor" ] = 4; b.pool_size[ "right_motor" ] = 4;
    b.f = []( double tm, const auto&, auto& out ) {
      out[ "left_motor" ] = 4 * 60.0 * std::max( 0.0, std::sin( 2 * tasks::kPi * 3 * tm / 1000.0 ) );
      out[ "right_motor" ] = 4 * 20.0;
    };
    Score s = run_task( *t, b, 1 ); show( "left bursts, right tonic", s );
    CHECK( !s.passed && s.parts[ "phase_deg" ] == 0 );
    CHECK( s.verdict.find( "only left_motor rises and falls" ) != std::string::npos );
  }
  // Spike ripple of tonically firing cells (one spike per 5 ms per cell, i.e. 200 Hz ripple, on each side, with a
  // 0.5 Hz drift on the left): must not be read as a fast rhythm or pass.
  {
    StubBrain b; b.pool_size[ "left_motor" ] = 1; b.pool_size[ "right_motor" ] = 1;
    b.f = []( double tm, const auto&, auto& out ) {
      if ( std::fmod( tm, 5.0 ) < 1.0 ) { out[ "left_motor" ] = 1000.0; out[ "right_motor" ] = 1000.0; }
    };
    Score s = run_task( *t, b, 1 ); show( "tonic spike ripple both sides", s );
    CHECK( !s.passed );
    CHECK( s.parts[ "freq_hz" ] < 1.0 || s.parts[ "freq_hz" ] > 10.0 || s.parts[ "phase_deg" ] == 0 );
  }
  // Silent on both sides: say so plainly.
  {
    StubBrain b; b.pool_size[ "left_motor" ] = 4; b.pool_size[ "right_motor" ] = 4; b.f = []( double, const auto&, auto& ) {};
    Score s = run_task( *t, b, 1 );
    CHECK( s.verdict.find( "Both motor channels are silent" ) != std::string::npos );
  }
  // Unwired motor: a circuit that declares no right_motor neurons.
  {
    StubBrain b; b.pool_size[ "left_motor" ] = 4; b.pool_size[ "right_motor" ] = 0;
    b.f = []( double, const auto&, auto& ) {};
    Score s = run_task( *t, b, 1 ); show( "right_motor unwired", s );
    CHECK( s.verdict.find( "motor 'right_motor'" ) != std::string::npos );
  }
  // per-neuron normalisation is pool-size independent
  {
    StubBrain b1 = osc( 2, 180, 60, 2 ), b2 = osc( 2, 180, 60, 9 );
    Score s1 = run_task( *t, b1, 1 ), s2 = run_task( *t, b2, 1 );
    CHECK( std::fabs( s1.parts[ "speed" ] - s2.parts[ "speed" ] ) < 1e-9 );
    CHECK( s1.parts[ "speed" ] > 0.25 && s1.parts[ "speed" ] < 1.5 );  // smooth 60 Hz-peak bursts; a lamprey swims about 0.5-2 BL/s
  }
  // body geometry: 11 points, head first; steady bias (left > right) turns left
  {
    StubBrain b; b.pool_size[ "left_motor" ] = 1; b.pool_size[ "right_motor" ] = 1;
    b.f = []( double, const auto&, auto& out ) { out[ "left_motor" ] = 40.0; };
    g_brain = &b; Engine e; t->reset( 1 );
    for ( int i = 0; i < 3000; ++i ) t->step( e, 1.0 );
    BodyState bs = t->body();
    CHECK( bs.points.size() == 11 && bs.heading > 0 );
  }
  // 5 ms slices give nearly the same answer as 1 ms
  {
    StubBrain b = osc( 3, 180, 60 );
    Score s1 = run_task( *t, b, 1, 1.0 ), s5 = run_task( *t, b, 1, 5.0 );
    CHECK( std::fabs( s1.parts[ "speed" ] - s5.parts[ "speed" ] ) < 0.1 * s1.parts[ "speed" ] );
  }
}

// ---- worm ------------------------------------------------------------------------------------------------------
static void test_worm()
{
  std::printf( "worm\n" );
  auto t = make_task( "worm" );
  CHECK( t->sensor_channels() == ( std::vector< std::string >{ "ASEL", "ASER" } ) );
  auto controller = []( StubBrain& b ) {
    // ideal logic: OFF cell (ASER) drives right turns, ON cell (ASEL) suppresses turning
    b.pool_size[ "turn_left" ] = 2; b.pool_size[ "turn_right" ] = 2;
    b.f = []( double, const auto& in, auto& out ) {
      double on = in.count( "ASEL" ) ? in.at( "ASEL" ) : 0, off = in.count( "ASER" ) ? in.at( "ASER" ) : 0;
      double turn = std::clamp( off / 100.0 - on / 100.0 + 0.0, 0.0, 1.0 );
      out[ "turn_right" ] = 2 * 60.0 * turn;
    };
  };
  {
    StubBrain b; controller( b );
    int pass = 0;
    for ( uint64_t seed = 1; seed <= 6; ++seed )
    {
      Score s = run_task( *t, b, seed );
      char name[ 64 ]; std::snprintf( name, sizeof name, "ideal logic seed %llu", (unsigned long long)seed );
      show( name, s );
      pass += s.parts[ "t_reach_s" ] > 0;  // physics only: reaching the peak at all (the 45 s pass bar needs a better controller than this stub)
    }
    CHECK( pass >= 5 );
  }
  {
    StubBrain b; b.pool_size[ "turn_left" ] = 2; b.pool_size[ "turn_right" ] = 2; b.f = []( double, const auto&, auto& ) {};
    int pass = 0;
    for ( uint64_t seed = 1; seed <= 6; ++seed ) { Score s = run_task( *t, b, seed ); pass += s.passed; if ( seed == 1 ) { show( "never turns", s ); CHECK( s.verdict.find( "Neither turn_left nor turn_right ever fired" ) != std::string::npos ); } }
    CHECK( pass == 0 );
  }
  {
    StubBrain b; b.pool_size[ "turn_left" ] = 2; b.pool_size[ "turn_right" ] = 2;
    b.f = []( double, const auto&, auto& out ) { out[ "turn_right" ] = 2 * 60.0; };  // circles forever
    int pass = 0;
    for ( uint64_t seed = 1; seed <= 6; ++seed ) { Score s = run_task( *t, b, seed ); pass += s.passed; if ( seed == 1 ) show( "always turns", s ); }
    CHECK( pass <= 1 );
  }
  {
    // field(): peak is 1, falls off, geometry
    StubBrain b; controller( b ); run_task( *t, b, 4 );
    BodyState bs = t->body();
    double px = bs.readouts[ "peak_x" ], py = bs.readouts[ "peak_y" ];
    CHECK( std::fabs( t->field( px, py ) - 1.0 ) < 1e-12 );
    CHECK( t->field( px + 3, py ) < t->field( px + 1, py ) );
    CHECK( bs.points.size() == 12 );
    auto wb = t->world_bounds();
    CHECK( wb[ 0 ] == -10 && wb[ 2 ] == 10 );
    // determinism
    StubBrain b2; controller( b2 );
    Score a = run_task( *t, b2, 9 ), c = run_task( *t, b2, 9 );
    CHECK( a.parts == c.parts );
    Score d = run_task( *t, b2, 10 );
    CHECK( d.parts != a.parts );
  }
}

int main()
{
  CHECK( task_ids().size() >= 3 );
  for ( auto& id : task_ids() ) { CHECK( make_task( id ) != nullptr ); CHECK( !reference_circuit( id ).empty() ); CHECK( !make_task( id )->brief().empty() ); }
  CHECK( make_task( "nope" ) == nullptr );
  test_reflex();
  test_lamprey();
  test_worm();
  std::printf( g_fail ? "%d FAILED\n" : "all passed\n", g_fail );
  return g_fail ? 1 : 0;
}
