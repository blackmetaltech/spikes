// Fix pass after the 0.5 playtest (flies and crab/dynamics reports): the diagnosis cases and the fly compass levels.
//   dx_*     scoring on probes (a wrong guess and every probe beyond the free ones lower the score; a revealed case scores nothing); a wrong pick says only "not that one"; symptoms do not name a
//            candidate or its wording; tier 3 narrows to a region, never to a pair of candidates; every patient circuit carries the healthy type of each projection (meta.dx_types), so the dead
//            wire's weight 0 does not give it away.
//   compass  a shifter that is too strong (right direction, the bump wraps past the half circle) is "too strong", not "wrong way"; the hints say equal strength in opposite directions.
//   c_ring   sixteen self-latches are not a ring; the brief names the cell drive and the synaptic time constant in words.
//   k_depress the brief no longer claims that baseline activity is needed.
#include <cstdio>
#include <set>
#include <sstream>
#include <string>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

namespace
{
const std::string kSrc = SPIKE_SOURCE_DIR;
json load( const std::string& path ) { return json::parse( chk::slurp( path ) ); }
std::string lower( std::string s )
{
  for ( auto& ch : s ) ch = static_cast< char >( std::tolower( static_cast< unsigned char >( ch ) ) );
  return s;
}
bool has( const std::string& hay, const std::string& needle ) { return lower( hay ).find( lower( needle ) ) != std::string::npos; }

Score run_json( const std::string& id, json c, uint64_t seed = 1 )
{
  Engine e;
  c[ "seed" ] = 1000 * seed + c.value( "seed", 1 );
  if ( !e.load( c.dump() ) ) return {};
  auto t = make_task( id );
  t->set_quick( true );
  t->reset( seed );
  const long steps = static_cast< long >( t->quick_duration_ms() );
  for ( long s = 0; s < steps; ++s ) t->step( e, 1.0 );
  return t->score();
}
void scale_edges( json& c, const std::string& pre, const std::string& post, double w )
{
  for ( auto& p : c[ "projections" ] )
    if ( p[ "pre" ] == pre && p[ "post" ] == post )
      for ( auto& e : p[ "edges" ] ) e[ 2 ] = w;
}

struct Dx
{
  const char* id;
  const char* reference;   // the healthy circuit the patient was made from
};
const Dx kDx[] = { { "dx_clonus", "renshaw.json" }, { "dx_vagal", "heart_autonomic.json" }, { "dx_orthostatic", "baroreflex.json" }, { "dx_apnoea", "breathing.json" },
  { "dx_horner", "pupil.json" }, { "dx_achalasia", "peristalsis.json" }, { "dx_worm", "worm_touch.json" }, { "dx_leech", "leech_heart.json" }, { "dx_aplysia", "aplysia_gill.json" },
  { "dx_compass", "fly_compass.json" }, { "dx_mb_apl", "fly_mb.json" }, { "dx_mb_dan", "fly_mb.json" }, { "dx_crab", "crab_pyloric_b.json" } };
// the cases whose symptoms were rewritten in the fix pass (no cell group or connection of the candidate list in the findings)
const std::set< std::string > kNoLeak = { "dx_worm", "dx_leech", "dx_aplysia", "dx_crab", "dx_compass", "dx_mb_apl", "dx_mb_dan" };

double first_weight( const json& p ) { return p.contains( "edges" ) ? p[ "edges" ][ 0 ][ 2 ].get< double >() : p[ "synapse" ][ "weight" ].get< double >(); }
}  // namespace

int main()
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  const std::string circuits = kSrc + "/circuits/";

  for ( const Dx& d : kDx )
  {
    std::printf( "%s\n", d.id );
    auto t = make_task( d.id );
    CHECK_MSG( t != nullptr, d.id );
    if ( !t ) continue;
    const auto dg = t->diagnosis();

    // ---- scoring on probes
    t->reset( 1 );
    t->set_variant( dg.answer );
    CHECK_MSG( t->score().passed && t->score().value == 100.0, std::string( d.id ) + ": no wrong guess and no probe: 100" );
    t->set_variant( dg.answer + "@3:0" );
    CHECK_MSG( t->score().value == 100.0, std::string( d.id ) + ": three probes are free" );
    t->set_variant( dg.answer + "@6:0" );
    CHECK_MSG( t->score().value == 91.0, std::string( d.id ) + ": three probes beyond the free ones cost 3 each: " + std::to_string( t->score().value ) );
    t->set_variant( dg.answer + "@3:2" );
    CHECK_MSG( t->score().value == 70.0, std::string( d.id ) + ": two wrong guesses cost 15 each" );
    CHECK_MSG( has( t->score().verdict, "two wrong guesses" ) && has( t->score().verdict, "three probes" ), t->score().verdict );   // the result shows what was used, in words
    if ( kNoLeak.count( d.id ) )   // (the authored reasons of the older autonomic cases quote physiological numbers, e.g. a tremor frequency)
      for ( char ch : t->score().verdict ) CHECK_MSG( !( ch >= '0' && ch <= '9' ), std::string( d.id ) + ": digit in a verdict: " + t->score().verdict );
    t->set_variant( dg.answer + "@200:9" );
    CHECK_MSG( t->score().value == 25.0 && t->score().passed, std::string( d.id ) + ": a floor, and still solved" );
    CHECK( t->metrics().at( "probes" ).value == 200.0 );
    t->set_variant( dg.answer + "@revealed" );
    CHECK_MSG( !t->score().passed && t->score().value == 0.0, std::string( d.id ) + ": a revealed case scores nothing" );
    CHECK( has( t->score().verdict, "revealed" ) );

    // ---- a wrong pick says only "not that one": none of the authored reasons is in the verdict
    for ( const auto& cand : dg.candidates )
    {
      if ( cand == dg.answer ) continue;
      t->set_variant( cand + "@2:0" );
      const Score s = t->score();
      CHECK_MSG( !s.passed && s.value == 0.0, std::string( d.id ) + ": wrong pick " + cand );
      CHECK_MSG( s.verdict.find( dg.explain.at( cand ) ) == std::string::npos && s.verdict.find( dg.explain.at( dg.answer ) ) == std::string::npos, std::string( d.id ) + ": a wrong pick explains itself: " + s.verdict );
      CHECK_MSG( s.verdict.size() < 90, std::string( d.id ) + ": a wrong pick's verdict is short: " + s.verdict );
    }
    t->set_variant( "" );

    // ---- tier 3 is a region, not a pair of candidates
    std::string t3;
    for ( const auto& h : t->hints() )
      if ( h.tier == 3 ) t3 = h.text;
    CHECK( !t3.empty() );
    CHECK_MSG( !has( t3, "it is either" ), std::string( d.id ) + ": tier 3 names a pair: " + t3 );
    int named = 0;
    for ( const auto& cand : dg.candidates )
      if ( t3.find( cand ) != std::string::npos ) ++named;
    CHECK_MSG( named == 0, std::string( d.id ) + ": tier 3 names a candidate: " + t3 );

    // ---- the patient circuit: the healthy type of every projection, so that weight 0 gives nothing away
    const json patient = load( circuits + dg.circuit ), ref = load( circuits + d.reference );
    CHECK_MSG( patient[ "meta" ].contains( "dx_types" ), std::string( d.id ) + ": meta.dx_types" );
    if ( patient[ "meta" ].contains( "dx_types" ) )
    {
      const auto& ty = patient[ "meta" ][ "dx_types" ];
      CHECK( ty.size() == patient[ "projections" ].size() && ty.size() == ref[ "projections" ].size() );
      for ( size_t i = 0; i < ty.size() && i < ref[ "projections" ].size(); ++i )
        CHECK_MSG( ty[ i ].get< std::string >() == ( first_weight( ref[ "projections" ][ i ] ) < 0 ? "I" : "E" ), std::string( d.id ) + ": type of projection " + std::to_string( i ) );
    }

    // ---- the findings of the rewritten cases do not name a candidate or repeat its wording
    if ( kNoLeak.count( d.id ) )
    {
      const std::string sym = lower( dg.symptoms );
      for ( const auto& cand : dg.candidates )
      {
        std::stringstream ss( cand );
        for ( std::string tok; std::getline( ss, tok, '+' ); )
        {
          const std::string pre = lower( tok.substr( 0, tok.find( '>' ) ) ), post = lower( tok.substr( tok.find( '>' ) + 1 ) );
          CHECK_MSG( sym.find( pre + ">" + post ) == std::string::npos && sym.find( pre + " > " + post ) == std::string::npos, std::string( d.id ) + ": findings name " + tok );
        }
        CHECK_MSG( sym.find( lower( dg.labels.at( cand ) ) ) == std::string::npos, std::string( d.id ) + ": findings repeat the wording of " + cand );
      }
      // no group of the candidate list is named in the findings as a cell group (a case that says "the dopamine neurons stay silent" has told the answer)
      for ( const auto& pop : patient[ "populations" ] )
      {
        const std::string id = pop[ "id" ].get< std::string >();
        bool in_cand = false;
        for ( const auto& cand : dg.candidates ) in_cand = in_cand || cand.find( id ) != std::string::npos;
        if ( !in_cand || id.size() < 3 ) continue;
        std::string up = dg.symptoms;
        CHECK_MSG( up.find( id ) == std::string::npos, std::string( d.id ) + ": findings name the group " + id );
      }
    }
  }

  // ---- the compass: too strong is "too strong", not "wrong way" (the bump wraps past the half circle)
  {
    json c = load( circuits + "fly_compass.json" );
    for ( double w : { 200.0 } )
    {
      json f = c;
      scale_edges( f, "PENL", "EPG", w );
      scale_edges( f, "PENR", "EPG", w );
      const Score s = run_json( "fly_compass", f );
      CHECK_MSG( !s.passed, "fly_compass: shifters x" + std::to_string( w ) + " do not pass" );
      CHECK_MSG( !has( s.verdict, "wrong way" ), "fly_compass: strong shifters are not 'wrong way': " + s.verdict );
      CHECK_MSG( has( s.verdict, "weaken" ) || has( s.verdict, "overshoot" ), "fly_compass: strong shifters: weaken them: " + s.verdict );
    }
    json cs = load( circuits + "c_shift.json" );
    scale_edges( cs, "PENL", "EPG", 200.0 );
    scale_edges( cs, "PENR", "EPG", 200.0 );
    const Score s = run_json( "c_shift", cs );
    CHECK_MSG( !s.passed && !has( s.verdict, "wrong way" ) && has( s.verdict, "weaken" ), "c_shift: shifters at 200: too strong, not wrong way: " + s.verdict );
    // the honest circuit still passes, and the circuit with the shifts swapped is still "wrong way"
    CHECK_MSG( run_json( "c_shift", load( circuits + "c_shift.json" ) ).passed, "c_shift: the reference passes" );
    CHECK_MSG( run_json( "fly_compass", load( circuits + "fly_compass.json" ) ).passed, "fly_compass: the reference passes" );
    json sw = load( circuits + "fly_compass.json" );
    for ( auto& p : sw[ "projections" ] )
      if ( p[ "post" ] == "EPG" && ( p[ "pre" ] == "PENL" || p[ "pre" ] == "PENR" ) )
        for ( auto& e : p[ "edges" ] ) e[ 1 ] = ( ( e[ 0 ].get< int >() + ( p[ "pre" ] == "PENL" ? 1 : -1 ) ) % 16 + 16 ) % 16;
    const Score sws = run_json( "fly_compass", sw );
    CHECK_MSG( !sws.passed && has( sws.verdict, "wrong way" ), "fly_compass: swapped shifts are still 'wrong way': " + sws.verdict );
    for ( const char* id : { "c_shift", "fly_compass" } )
    {
      auto t = make_task( id );
      bool equal = false;
      for ( const auto& h : t->hints() )
        if ( h.tier >= 2 && has( h.text, "equal strength" ) && has( h.text, "opposite directions" ) ) equal = true;
      CHECK_MSG( equal, std::string( id ) + ": a hint says equal strength, opposite directions" );
      CHECK_MSG( !has( t->brief(), "the two groups the same" ), std::string( id ) );
    }
    // the playtester's circuit with both groups pushing the same way reads the new wording, not "make them the same"
    const Score same = run_json( "c_shift", load( kSrc + "/engine/tests/playtest/shift_wrongway.json" ) );
    CHECK_MSG( !same.passed && !has( same.verdict, "the same" ), "c_shift: both groups pushing one way: " + same.verdict );
    CHECK_MSG( has( same.verdict, "opposite directions" ), "c_shift: both groups pushing one way: " + same.verdict );
  }

  // ---- c_ring: sixteen self-latches are no ring; the brief says what a default cell lacks
  {
    for ( const char* f : { "ring_purelatch.json", "ring_selflatch.json" } )
    {
      const Score s = run_json( "c_ring", load( kSrc + "/engine/tests/playtest/" + f ) );
      CHECK_MSG( !s.passed && has( s.verdict, "neighbours" ), std::string( f ) + ": " + s.verdict );
    }
    auto t = make_task( "c_ring" );
    CHECK( has( t->brief(), "bias" ) && has( t->brief(), "time constant" ) );
    // the default-cell ring of the playtest is told about the cell's own drive and its synaptic time constant (words, no digits)
    const Score s = run_json( "c_ring", load( kSrc + "/engine/tests/playtest/ring_nb300.json" ) );
    CHECK_MSG( !s.passed && has( s.verdict, "bias" ) && has( s.verdict, "time constant" ), "c_ring default cells: " + s.verdict );
    for ( char ch : s.verdict ) CHECK_MSG( !( ch >= '0' && ch <= '9' ), "c_ring: digit in a verdict: " + s.verdict );
    CHECK( run_json( "c_ring", load( circuits + "c_ring.json" ) ).passed );
  }

  // ---- k_depress: no baseline claim; the playtester's circuit without the bias passes, so the brief must not demand it
  {
    auto t = make_task( "k_depress" );
    CHECK_MSG( !has( t->brief(), "baseline activity" ), "k_depress: the brief demands baseline activity" );
    CHECK_MSG( run_json( "k_depress", load( kSrc + "/engine/tests/playtest/kd_nobias.json" ) ).passed, "k_depress: no bias passes" );
  }
  return chk::finish( "tasks_dx_fixes_test" );
}
