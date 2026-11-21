// Diagnosis cases (full review, P0-4): the candidate order must not give the answer away, and every case lists the probes that tell its top candidates apart.
//   * the answer is never the first candidate, and across the cases it does not sit in one place (a button list where the answer is always third is as bad)
//   * the order is stable: the same list on every call and every construction
//   * every candidate is still there once, and the answer is one of them
//   * every case lists one or two probes, of kind "scope" or "cut", each with a target and a plain sentence
#include <algorithm>
#include <map>
#include <set>
#include "check.h"
#include "spike/task.h"

using namespace spike;

int main()
{
  std::map< size_t, int > place;
  int cases = 0;
  for ( const auto& id : task_ids() )
  {
    auto t = make_task( id );
    if ( t->level_kind() != "diagnosis" ) continue;
    ++cases;
    const auto d = t->diagnosis(), d2 = make_task( id )->diagnosis();
    CHECK_MSG( d.candidates == d2.candidates, id + ": the candidate order changes between constructions" );
    CHECK_MSG( std::set< std::string >( d.candidates.begin(), d.candidates.end() ).size() == d.candidates.size(), id + ": duplicate candidate" );
    const auto at = std::find( d.candidates.begin(), d.candidates.end(), d.answer );
    CHECK_MSG( at != d.candidates.end(), id + ": the answer is not a candidate" );
    if ( at == d.candidates.end() ) continue;
    const size_t pos = static_cast< size_t >( at - d.candidates.begin() );
    CHECK_MSG( pos != 0, id + ": the answer is the first candidate" );
    ++place[ pos ];
    CHECK_MSG( d.probes.size() >= 1 && d.probes.size() <= 2, id + ": a case lists one or two probes" );
    for ( const auto& p : d.probes )
      CHECK_MSG( ( p.kind == "scope" || p.kind == "cut" ) && !p.target.empty() && !p.tells.empty(), id + ": a probe needs kind scope|cut, a target and a sentence" );
    if ( !d.probes.empty() )
      CHECK_MSG( std::any_of( d.probes.begin(), d.probes.end(), []( const Task::Diagnosis::Probe& p ) { return p.kind == "cut"; } ) ||
                   std::any_of( d.probes.begin(), d.probes.end(), []( const Task::Diagnosis::Probe& p ) { return p.kind == "scope"; } ), id + ": no usable probe" );
  }
  CHECK_MSG( cases >= 10, "the diagnosis cases are missing" );
  CHECK_MSG( place.size() >= 3, "the answer sits in too few places of the candidate list" );
  for ( auto& [ k, n ] : place ) std::printf( "  answer at position %zu in %d cases\n", k, n );
  return chk::finish( "tasks_diagnosis_order_test" );
}
