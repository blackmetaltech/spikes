// v0.3 item C: the two boss scenarios (boss_spinal_sprint, boss_autonomic_exam) and the six diagnosis cases (dx_*).
//
//   tasks_boss_dx_test                      the CTest
//   tasks_boss_dx_test run <task> [circuit.json] [seeds=1,2,3] [pre>post*factor ...]
//                                           tuning aid: run a task on its reference (or the given) circuit, print the score parts.
//
// Bosses: the reference circuit passes on several seeds, the lesions listed below fail, contract fields are filled (level_kind "boss",
// hints in 3 tiers, metrics, notebook with citation, modes, timeline and bands, channel_kinds for every channel).
// Diagnosis cases: the patient circuit is the reference circuit of the underlying task minus the answer's projections, the candidates
// are real lesion ids (each matches a projection of the reference), the symptom shows in the patient's own run, and score() accepts only the
// right answer submitted with set_variant(id).
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

struct Lesion { std::string pre, post; double factor; };

static json load_circuit( const std::string& file ) { return json::parse( chk::slurp( file ) ); }
static std::string circuits_dir() { return std::string( SPIKE_SOURCE_DIR ) + "/circuits/"; }

/// Scale the weight of every projection pre -> post (0 keeps the edge with weight 0). Returns the number of projections hit.
static int apply_lesion( json& c, const Lesion& l )
{
  int hit = 0;
  for ( auto& p : c[ "projections" ] )
  {
    if ( p[ "pre" ] != l.pre || p[ "post" ] != l.post )
      continue;
    ++hit;
    if ( p.contains( "synapse" ) && p[ "synapse" ].contains( "weight" ) )
      p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * l.factor;
    if ( p.contains( "edges" ) )
      for ( auto& e : p[ "edges" ] )
        e[ 2 ] = e[ 2 ].get< double >() * l.factor;
  }
  return hit;
}

/// Lesion id "a>b+c>d" -> list of pre>post pairs
static std::vector< Lesion > parse_lesion( const std::string& id )
{
  std::vector< Lesion > v;
  std::stringstream ss( id );
  for ( std::string tok; std::getline( ss, tok, '+' ); )
  {
    auto gt = tok.find( '>' );
    v.push_back( { tok.substr( 0, gt ), tok.substr( gt + 1 ), 0.0 } );
  }
  return v;
}

struct RunResult { Score score; std::map< std::string, double > rate_hz; double wall_s = 0; bool loaded = false; std::string error; };

/// Runs `task` on `circuit`; `after` (optional) is called with the task after the episode (e.g. to submit an answer).
static RunResult run_one( const std::string& task_id, const json& circuit_in, uint64_t seed, const std::string& mode = "standard" )
{
  RunResult r;
  Engine e;
  json circuit = circuit_in;
  circuit[ "seed" ] = 1000 * seed + circuit_in.value( "seed", 1 );
  r.loaded = e.load( circuit.dump() );
  if ( !r.loaded )
  {
    r.error = e.error();
    return r;
  }
  auto task = make_task( task_id );
  task->set_mode( mode );
  task->reset( seed );
  std::map< std::string, long > counts;
  std::map< std::string, int > sizes;
  for ( int i = 0; i < e.neuron_count(); ++i )
    ++sizes[ e.neuron_population( i ) ];
  auto t0 = std::chrono::steady_clock::now();
  const long steps = static_cast< long >( task->duration_ms() );
  for ( long s = 0; s < steps; ++s )
  {
    task->step( e, 1.0 );
    if ( s % 500 == 499 )
      for ( auto& sp : e.drain_spikes() )
        ++counts[ e.neuron_population( sp.neuron ) ];
  }
  for ( auto& sp : e.drain_spikes() )
    ++counts[ e.neuron_population( sp.neuron ) ];
  r.wall_s = std::chrono::duration< double >( std::chrono::steady_clock::now() - t0 ).count();
  for ( auto& [ pop, n ] : sizes )
    r.rate_hz[ pop ] = counts[ pop ] / ( double )n / ( task->duration_ms() / 1000.0 );
  r.score = task->score();
  r.error = e.error();
  return r;
}

static void print( const std::string& what, uint64_t seed, const RunResult& r )
{
  if ( !r.loaded )
  {
    std::printf( "  %-26s seed %llu LOAD ERROR: %s\n", what.c_str(), ( unsigned long long )seed, r.error.c_str() );
    return;
  }
  std::printf( "  %-26s seed %llu value %5.1f %s |", what.c_str(), ( unsigned long long )seed, r.score.value, r.score.passed ? "PASS" : "fail" );
  for ( auto& [ k, v ] : r.score.parts )
    std::printf( " %s=%.3g", k.c_str(), v );
  std::printf( "  (%.1fs)\n    %s\n", r.wall_s, r.score.verdict.c_str() );
  if ( !r.error.empty() )
    std::printf( "    ENGINE ERROR: %s\n", r.error.c_str() );
}

static void check_contract_common( const std::string& id, const std::vector< std::string >& ids )
{
  bool found = std::find( ids.begin(), ids.end(), id ) != ids.end();
  CHECK_MSG( found, id + " is not registered" );
  if ( !found )
    return;
  auto t = make_task( id );
  CHECK_MSG( t->id() == id, id );
  CHECK( !t->title().empty() );
  CHECK( !t->brief().empty() );
  CHECK( !t->timeline().empty() );
  CHECK( !t->bands().empty() );
  CHECK( !t->notebook().title.empty() && !t->notebook().text.empty() && !t->notebook().citation.empty() );
  CHECK( !t->hints().empty() );
  CHECK( reference_circuit( id ) == id + ".json" );
  CHECK_MSG( id.rfind( "dx_", 0 ) == 0 || t->next_event_ms( 0 ) > 0, id + " next_event_ms(0) = " + std::to_string( t->next_event_ms( 0 ) ) );
  // every channel has a declared kind (so the lab can build a skeleton)
  auto ck = t->channel_kinds();
  for ( auto& s : t->sensor_channels() ) CHECK_MSG( ck.count( s ), id + " channel_kinds misses " + s );
  for ( auto& m : t->motor_channels() ) CHECK_MSG( ck.count( m ), id + " channel_kinds misses " + m );
  // reference circuit declares every channel
  json c = load_circuit( circuits_dir() + reference_circuit( id ) );
  std::set< std::string > in_ch, out_ch;
  for ( auto& i : c[ "inputs" ] ) in_ch.insert( i[ "name" ].get< std::string >() );
  for ( auto& o : c[ "outputs" ] ) out_ch.insert( o[ "name" ].get< std::string >() );
  for ( auto& s : t->sensor_channels() ) CHECK_MSG( in_ch.count( s ), id + " input " + s );
  for ( auto& m : t->motor_channels() ) CHECK_MSG( out_ch.count( m ), id + " output " + m );
}

struct BossCase { std::string task; std::vector< Lesion > lesions; const char* what; };

static const std::vector< BossCase > kBossLesions = {
  { "boss_spinal_sprint", { { "Ia_F", "MN_F", 0.0 }, { "II_F", "MN_F", 0.0 }, { "Ia_E", "MN_E", 0.0 }, { "II_E", "MN_E", 0.0 } }, "spindle afferents cut from the motoneurons (no stretch reflex: the arm rides up)" },
  { "boss_spinal_sprint", { { "TONIC", "GAM", 0.0 } }, "tonic drive cut from the gamma pool (spindles go slack: floppy arm)" },
  { "boss_spinal_sprint", { { "TGT", "MN_F", 0.0 }, { "TGT", "MN_E", 0.0 } }, "target cannot shift the tonic balance (no hold command: the arm does not stay at the new angle)" },
  { "boss_spinal_sprint", { { "TONIC", "MN_F", 0.0 } }, "no antigravity tone in the flexor (the arm falls)" },
  { "boss_autonomic_exam", { { "RVLM", "IML", 0.0 } }, "vasomotor output cut (no sympathetic tone: pressure collapses on standing)" },
  { "boss_autonomic_exam", { { "NTS", "CVLM", 0.0 } }, "depressor pathway cut (pressure runs high and unregulated)" },
  { "boss_autonomic_exam", { { "HYP", "NA", 0.0 }, { "CC", "NA", 0.0 } }, "no vagal withdrawal (heart rate cannot rise in exercise or fright)" },
  { "boss_autonomic_exam", { { "CC", "VRG", 0.0 }, { "HYP", "VRG", 0.0 } }, "no central drive to breathing (ventilation does not follow exercise or fright)" },
};

struct DxCase { std::string id, inner; const char* symptom_key; double symptom_lo, symptom_hi; };
// the patient's own run must show the symptom: inner score part `symptom_key` (prefixed run_) in [lo, hi]
static const std::vector< DxCase > kDx = {
  { "dx_clonus", "renshaw", "run_improvement", -1, 0.25 },          // the Renshaw brake does not reduce the tremor
  { "dx_vagal", "heart_autonomic", "run_rest_hr", 90, 140 },        // resting tachycardia
  { "dx_orthostatic", "baroreflex", "run_map_min_mmHg", 0, 65 },    // pressure collapses on standing
  { "dx_apnoea", "breathing", "run_ve_ratio", 0, 1.4 },             // no CO2 response
  { "dx_horner", "pupil", "run_dim2_mm", 0, 5.8 },                  // the pupil does not open in the dark
  { "dx_achalasia", "peristalsis", "run_arrival_s", 25, 100 },      // arrives late (or never: -1 is excluded by the lower bound check below)
};

int main( int argc, char** argv )
{
  if ( argc >= 3 && std::string( argv[ 1 ] ) == "run" )
  {
    std::string task = argv[ 2 ], file = circuits_dir() + reference_circuit( task );
    int i = 3;
    if ( i < argc && std::string( argv[ i ] ).find( ".json" ) != std::string::npos )
      file = argv[ i++ ];
    std::vector< uint64_t > seeds;
    std::string sl = ( i < argc && std::string( argv[ i ] ).find( '>' ) == std::string::npos ) ? argv[ i++ ] : "1,2,3";
    std::stringstream ss( sl );
    for ( std::string tok; std::getline( ss, tok, ',' ); )
      seeds.push_back( std::stoull( tok ) );
    json c = load_circuit( file );
    for ( ; i < argc; ++i )
    {
      std::string a = argv[ i ];
      auto gt = a.find( '>' ), st = a.find( '*' );
      apply_lesion( c, { a.substr( 0, gt ), a.substr( gt + 1, st - gt - 1 ), std::stod( a.substr( st + 1 ) ) } );
    }
    for ( auto s : seeds )
      print( task, s, run_one( task, c, s ) );
    return 0;
  }

  const auto ids = task_ids();

  // ================================================================ bosses
  for ( const std::string id : { "boss_spinal_sprint", "boss_autonomic_exam" } )
  {
    std::printf( "%s\n", id.c_str() );
    check_contract_common( id, ids );
    auto t = make_task( id );
    CHECK( t->level_kind() == "boss" );
    CHECK( t->on_spine() );
    CHECK( t->chapter() == ( id == "boss_spinal_sprint" ? "spinal" : "autonomic" ) );
    CHECK( t->par_neurons() > 0 );
    CHECK( t->duration_ms() >= 30000 );
    CHECK( !t->concepts_used().empty() );
    CHECK( !t->metrics().empty() );
    CHECK( t->bands().size() >= 2 );
    // order key in the x30-x39 band of its chapter, and after the chapter's last ordinary task
    int pos = static_cast< int >( std::find( ids.begin(), ids.end(), id ) - ids.begin() );
    CHECK( pos == static_cast< int >( ids.size() ) - 1 - 0 || pos < static_cast< int >( ids.size() ) );
    // hints: three tiers (after a failed run they depend on the score; here on the fresh state)
    auto hs = t->hints();
    std::set< int > tiers;
    for ( auto& h : hs ) tiers.insert( h.tier );
    CHECK( tiers.count( 1 ) && tiers.count( 2 ) && tiers.count( 3 ) );
    // modes: guided and researcher exist and are accepted (tolerance scales; structure does not)
    for ( const char* m : { "guided", "standard", "researcher" } )
    {
      t->set_mode( m );
      CHECK( t->mode() == m );
    }

    json c = load_circuit( circuits_dir() + reference_circuit( id ) );
    int neurons = 0;
    for ( auto& p : c[ "populations" ] ) neurons += p[ "n" ].get< int >();
    CHECK_MSG( neurons <= t->par_neurons() + 6 && neurons + 6 >= t->par_neurons(), id + " par_neurons " + std::to_string( t->par_neurons() ) + " vs " + std::to_string( neurons ) );
    std::vector< uint64_t > seeds = id == "boss_spinal_sprint" ? std::vector< uint64_t >{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } : std::vector< uint64_t >{ 1, 2, 3, 4, 5, 6 };
    double sum = 0;
    for ( uint64_t seed : seeds )
    {
      RunResult r = run_one( id, c, seed );
      print( "reference", seed, r );
      CHECK_MSG( r.loaded, r.error );
      CHECK_MSG( r.score.passed, id + " seed " + std::to_string( seed ) + ": " + r.score.verdict );
      CHECK( r.score.value >= 50.0 );
      CHECK( !r.score.verdict.empty() );
      CHECK( r.error.empty() );
      sum += r.score.value;
    }
    std::printf( "  mean score %.1f\n", sum / seeds.size() );
    {  // reproducible
      RunResult a = run_one( id, c, 2 ), b = run_one( id, c, 2 );
      CHECK( a.score.value == b.score.value );
      CHECK( a.score.parts == b.score.parts );
    }
    {  // guided is never harder than standard, researcher never easier (same circuit, same seed)
      RunResult g = run_one( id, c, 3, "guided" ), s = run_one( id, c, 3, "standard" ), r = run_one( id, c, 3, "researcher" );
      CHECK( g.score.passed || !s.score.passed );
      CHECK( s.score.passed || !r.score.passed );
    }
  }

  std::printf( "boss lesions\n" );
  for ( const auto& bc : kBossLesions )
  {
    json c = load_circuit( circuits_dir() + reference_circuit( bc.task ) );
    for ( auto& l : bc.lesions )
      CHECK_MSG( apply_lesion( c, l ) > 0, bc.task + ": lesion " + l.pre + ">" + l.post + " matched no projection" );
    for ( uint64_t seed : { 1, 2 } )
    {
      RunResult r = run_one( bc.task, c, seed );
      print( bc.what, seed, r );
      CHECK_MSG( r.loaded, r.error );
      CHECK_MSG( !r.score.passed, std::string( "lesion must fail: " ) + bc.what );
    }
  }

  // ================================================================ diagnosis cases
  std::set< std::string > answers;
  for ( const auto& dc : kDx )
  {
    const std::string& id = dc.id;
    std::printf( "%s\n", id.c_str() );
    check_contract_common( id, ids );
    auto t = make_task( id );
    CHECK( t->level_kind() == "diagnosis" );
    CHECK( !t->on_spine() );
    CHECK( t->variants().empty() );
    CHECK( t->metrics().count( "wrong_guesses" ) == 1 );
    auto hs = t->hints();
    std::set< int > tiers;
    for ( auto& h : hs ) tiers.insert( h.tier );
    CHECK( tiers.count( 1 ) && tiers.count( 2 ) && tiers.count( 3 ) );
    CHECK( !t->concepts_new().empty() );
    auto dg = t->diagnosis();
    CHECK( dg.circuit == id + ".json" );
    CHECK( !dg.symptoms.empty() );
    CHECK( dg.candidates.size() >= 4 && dg.candidates.size() <= 6 );
    CHECK( std::find( dg.candidates.begin(), dg.candidates.end(), dg.answer ) != dg.candidates.end() );
    CHECK( std::set< std::string >( dg.candidates.begin(), dg.candidates.end() ).size() == dg.candidates.size() );
    CHECK( t->brief().find( dg.answer ) != std::string::npos );  // listed with the other candidates
    answers.insert( dg.answer );

    // the patient circuit = reference of the inner task minus the answer's projections; every candidate is a real lesion of the reference
    json ref = load_circuit( circuits_dir() + reference_circuit( dc.inner ) ), pat = load_circuit( circuits_dir() + dg.circuit );
    for ( const auto& cand : dg.candidates )
    {
      json tmp = ref;
      for ( auto& l : parse_lesion( cand ) )
        CHECK_MSG( apply_lesion( tmp, l ) > 0, id + ": candidate " + cand + " matches no projection of " + reference_circuit( dc.inner ) );
    }
    {
      json expect = ref;
      std::vector< json > keep;
      auto drop = parse_lesion( dg.answer );
      for ( auto& p : expect[ "projections" ] )
      {
        bool d = false;
        for ( auto& l : drop )
          if ( p[ "pre" ] == l.pre && p[ "post" ] == l.post ) d = true;
        if ( !d ) keep.push_back( p );
      }
      CHECK( pat[ "projections" ] == json( keep ) );
      CHECK( pat[ "populations" ] == ref[ "populations" ] );
      CHECK( pat[ "inputs" ] == ref[ "inputs" ] );
      CHECK( pat[ "outputs" ] == ref[ "outputs" ] );
      CHECK( pat[ "projections" ].size() < ref[ "projections" ].size() );
      CHECK( pat.dump().find( dg.answer ) == std::string::npos );  // the file does not leak the answer
    }

    // the patient's own run shows the symptom, and the diagnosis is judged by the submitted answer
    RunResult r = run_one( id, pat, 1 );
    print( "patient", 1, r );
    CHECK_MSG( r.loaded, r.error );
    CHECK( r.error.empty() );
    CHECK_MSG( r.score.parts.count( "run_passed" ) && r.score.parts[ "run_passed" ] == 0.0, id + ": the patient's circuit must fail its world" );
    const double sv = r.score.parts.count( dc.symptom_key ) ? r.score.parts[ dc.symptom_key ] : -999;
    CHECK_MSG( sv >= dc.symptom_lo && sv <= dc.symptom_hi, id + ": symptom " + dc.symptom_key + " = " + std::to_string( sv ) );
    CHECK( !r.score.passed );  // nothing submitted yet
    CHECK( r.score.value == 0.0 );

    auto task = make_task( id );
    task->reset( 1 );
    CHECK( !task->score().passed );
    CHECK( task->score().verdict.find( "No diagnosis" ) == 0 );
    for ( const auto& cand : dg.candidates )
    {
      task->set_variant( cand );
      Score s = task->score();
      CHECK_MSG( s.passed == ( cand == dg.answer ), id + " submitted " + cand );
      CHECK( s.value == ( cand == dg.answer ? 100.0 : 0.0 ) );
      CHECK( !s.verdict.empty() );
      CHECK( task->variant() == cand );
    }
    task->set_variant( "not-a-lesion" );
    CHECK( !task->score().passed );
    CHECK( task->metrics().at( "wrong_guesses" ).value == static_cast< double >( dg.candidates.size() - 1 + 1 ) );
    task->set_variant( "" );
    CHECK( task->score().verdict.find( "No diagnosis" ) == 0 );
  }
  CHECK( answers.size() == 6 );  // six distinct cases

  // ---- order keys: bosses after their chapter's tasks, diagnosis cases after the bosses ----
  auto pos = [ & ]( const std::string& id ) { return std::find( ids.begin(), ids.end(), id ) - ids.begin(); };
  CHECK( pos( "boss_spinal_sprint" ) > pos( "reach" ) );
  CHECK( pos( "dx_clonus" ) > pos( "boss_spinal_sprint" ) );
  CHECK( pos( "boss_autonomic_exam" ) > pos( "fight_or_flight" ) );
  CHECK( pos( "dx_horner" ) > pos( "boss_autonomic_exam" ) );
  CHECK( pos( "dx_achalasia" ) > pos( "boss_autonomic_exam" ) );
  CHECK( pos( "dx_vagal" ) > pos( "heart_autonomic" ) && pos( "dx_orthostatic" ) > pos( "baroreflex" ) );

  return chk::finish( "tasks_boss_dx_test" );
}
