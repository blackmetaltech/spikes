// "a_path" (chapter a_worm, Atlas wing, level_kind "atlas"): the path finder gives the shortest route from head touch to the backward motor neurons; test it.
//
// The wiring of the touch circuit is given and locked (circuits/a_path_given.json, made by circuits/tools/gen_atlas_worm.py from data/connectome/worm/touch.json). The five
// interneuron classes AVA, AVB, AVD, AVE and PVC are cell-type groups whose scale the player sets from 0 (silenced) to 1 (kept); the sign is fixed (every synapse excites).
// The player silences every class that is not on one shortest route from the head touch cells to the backward motor neurons and runs the touch.
//
// MECHANISM (the structure of the circuit as run, never the rates): the classes kept (scale 1) are exactly the interneuron classes of ONE shortest route of the given graph
// at class level (analysis::shortest_path / on_shortest_paths), every other class is silenced (scale 0). A class counts as silenced only at scale 0 and as kept only at scale 1.
//   not on a route / two routes kept / nothing silenced / everything silenced / a class half on: each fails with a verdict that says why.
// WHAT THE RUN SHOWED is told from the measured motor rates (atlas_worm.h): in this model one route kept alone does not make the worm back up. A route on paper is not a
// pathway that works. The level passes when the route is right and the run was made, whatever the run showed.
#include "atlas_worm.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_a_path();
namespace
{
using namespace atlas_worm;

class APath : public WormRun
{
public:
  APath() : WormRun( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i = world( "a_path", "Atlas: test the shortest route" );
    i.brief = "The path finder gives the shortest route from a touch on the head to the backward motor neurons. A route on a wiring diagram is a claim; test it. "
              "The wiring is given and cannot be edited. Each of the five interneuron classes has a card with a scale, from zero (silenced) to one (kept). Silence every "
              "interneuron class that is not on your route, keep the classes of the route, and run the touch. The level checks that the classes you kept are exactly those of one "
              "shortest route and that you ran the touch. Then it tells you what the run showed.";
    i.concepts_new = { "pathway" };
    i.concepts_used = { "excitation" };
    i.recipe = "Ask the path finder from the head touch cells (ALM, AVM) to the backward motor neurons (DA, VA). Set the scale of every interneuron class that is not on the route to zero "
               "and the scale of the classes on the route to one. A class is silenced only at exactly zero and kept only at exactly one.";
    i.note = { "A route on paper",
               "A wiring diagram gives routes: chains of cells from a sensor to a muscle. In this model, with every synapse excitatory, keeping one shortest route alone and silencing the "
               "other interneuron classes does not make the worm back up, so a route on paper is not yet a pathway that works. The animal is different from this model. Its touch circuit was "
               "worked out by electron-microscope reconstruction and by killing cells with a laser and watching the behaviour, which found two pathways for touch to the head, through the AVD and "
               "AVB interneurons (Chalfie et al. 1985). The wiring used here is from Cook et al. 2019.",
               "Chalfie M, Sulston JE, White JG, Southgate E, Thomson JN, Brenner S (1985) J Neurosci 5:956-964, doi:10.1523/JNEUROSCI.05-04-00956.1985; "
               "Cook SJ et al. (2019) Nature 571:63-71, doi:10.1038/s41586-019-1352-7" };
    return i;
  }

  std::string level_kind() const override { return "atlas"; }

  Given given() const override
  {
    Given g;
    g.circuit = "a_path_given.json";
    // printed by `spike-run --locked-digest circuits/a_path_given.json`; tasks_atlas_worm_test checks that the given and the reference circuits both have exactly these
    g.digest.pops = "738c6bdaeaf081583d69c8bfa5f74b51";
    g.digest.wiring = "7dc1ad137b57433fca1c823c254fc10a";
    g.digest.types = "7793bc66dc91308bfcea52245c731723";
    g.digest.channels = "bab362670876a5b510b31f294b1a2547";
    return g;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "A route is a chain of cells. A route that exists in the wiring is not the same as a route that works: to test it, take everything else away and watch the motor neurons.", "" },
      { 2, "Look at the backward motor neurons and the interneuron classes between them and the head touch cells. Each class card has a scale: zero silences the class, one keeps it.", "DA" },
      { 3, "Ask the path finder for the shortest route from the head touch cells to the backward motor neurons. Keep the interneuron classes on one such route and silence all the others, then run the touch. "
           "Keeping two routes, or silencing nothing, does not test one route.", "DA" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    const Rates r = rates();
    return { { "backward_hz", { r.a_resp, "Hz", false } } };
  }

  Score score() const override
  {
    Score s;
    fill_parts( s );
    s.passed = done() && mech_.empty() && wiring_.ok();
    s.value = !done() ? 0.0 : ( s.passed ? 100.0 : 30.0 );
    apply_mech( s );
    if ( !wiring_.ok() )
      s.verdict = "Not wired: " + wiring_.message() + ".";
    else if ( !done() )
      s.verdict = "Running.";
    else if ( !mech_.empty() )
      s.verdict = mech_;
    else
      s.verdict = std::string( "One shortest route is kept and every other interneuron class is silenced. " ) + outcome() +
                  ( backs_up() ? " In this model that route alone carries the response."
                               : " In this model a route on paper is not yet a pathway that works." );
    return s;
  }

protected:
  void inspect( Engine& ) override
  {
    const Setting st = setting();
    if ( !st.missing.empty() ) { mech_ = "The interneuron class cards are missing: load the given circuit again."; return; }
    if ( !st.partial.empty() )
    {
      mech_ = "The class " + st.partial.front() + " is only partly on. In this test a class is silenced only at exactly zero and kept only at exactly one.";
      return;
    }
    if ( st.silenced.empty() ) { mech_ = "Nothing is silenced: every interneuron class is still on, so this is the whole circuit and not one route. Silence the classes that are not on your route."; return; }
    if ( st.kept.empty() ) { mech_ = "Every interneuron class is silenced, so no route is left to test. Keep the classes of one route."; return; }
    // the given graph at class level: one node per population, an edge where a projection has connections, even at weight 0 (n_stored_edges; the wiring is locked, so this is the given graph)
    const int n = static_cast< int >( g_.st.populations.size() );
    std::vector< analysis::Edge > edges;
    for ( const auto& pr : g_.st.projections )
      if ( pr.n_stored_edges > 0 && pr.pre_index != pr.post_index ) edges.push_back( { pr.pre_index, pr.post_index, 1.0 } );
    const analysis::Graph gr = analysis::make_graph( edges, n );
    const std::vector< int32_t > src = g_.in_pops( "touch_head" ), dst = g_.out_pops( "reverse" );
    const std::vector< int32_t > best = analysis::shortest_path( gr, src, dst );
    if ( best.empty() ) { mech_ = "The path finder finds no route from the head touch cells to the backward motor neurons."; return; }
    const int L = static_cast< int >( best.size() ) - 1;
    auto index_of = [ & ]( const std::string& id ) {
      for ( int p = 0; p < n; ++p )
        if ( g_.st.populations[ static_cast< size_t >( p ) ].id == id ) return p;
      return -1;
    };
    std::set< int > cmd, kept;
    for ( const auto& c : commands() ) cmd.insert( index_of( c ) );
    for ( const auto& c : st.kept ) kept.insert( index_of( c ) );
    // the interneuron classes that lie on at least one shortest route
    std::set< int > on_route;
    for ( int v : analysis::on_shortest_paths( gr, src, dst ) )
      if ( cmd.count( v ) ) on_route.insert( v );
    for ( int k : kept )
      if ( !on_route.count( k ) )
      {
        mech_ = "The class " + g_.st.populations[ static_cast< size_t >( k ) ].id + " is not on any shortest route from the head touch cells to the backward motor neurons. Keep only the classes of one shortest route.";
        return;
      }
    // every shortest route that uses only kept classes: one of them must use exactly the kept classes
    std::vector< int32_t > banned;
    for ( int c : cmd )
      if ( !kept.count( c ) ) banned.push_back( c );
    std::vector< bool > ban( static_cast< size_t >( n ), false ), target( static_cast< size_t >( n ), false );
    for ( int b : banned ) ban[ static_cast< size_t >( b ) ] = true;
    for ( int d : dst ) target[ static_cast< size_t >( d ) ] = true;
    bool exact = false, any = false;
    std::vector< int > path;
    std::function< void( int ) > walk = [ & ]( int u ) {
      if ( static_cast< int >( path.size() ) - 1 == L )
      {
        if ( !target[ static_cast< size_t >( u ) ] ) return;
        any = true;
        std::set< int > used;
        for ( int v : path )
          if ( cmd.count( v ) ) used.insert( v );
        if ( used == kept ) exact = true;
        return;
      }
      for ( int v : gr.out[ static_cast< size_t >( u ) ] )
      {
        if ( ban[ static_cast< size_t >( v ) ] || std::find( path.begin(), path.end(), v ) != path.end() ) continue;
        path.push_back( v );
        walk( v );
        path.pop_back();
      }
    };
    for ( int s : src )
    {
      path = { s };
      walk( s );
    }
    if ( !exact )
      mech_ = any ? "More than one route is kept: the classes you kept make up two or more shortest routes. Keep the classes of one route and silence the rest."
                  : "The classes you kept do not form a shortest route from the head touch cells to the backward motor neurons.";
  }
};
}  // namespace

std::unique_ptr< Task > make_a_path() { return std::make_unique< APath >(); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( a_path, 991, "a_path.json", make_a_path ) }
