// "a_motif" (chapter a_worm, Atlas wing): a PREDICTION BET and a null-model duel on the whole chemical wiring of the C. elegans hermaphrodite (302 cells, circuits/a_motif.json,
// made by circuits/tools/gen_atlas_worm.py from data/connectome/worm/whole.json; wiring locked, no stimulus).
//
// Question: under which null model is the feed-forward loop more frequent than chance in this wiring?  Options: "edges" (the same number of edges, placed at random),
// "degree" (the same in- and out-degree for every cell), "blocks" (the same degrees and the same number of connections between the cell-type blocks: the groups of the circuit are the
// blocks, sensory, interneuron, motor and other). The ENGINE computes the three z-scores itself on the circuit it runs: analysis::motif_zscores( g, NullModel, block_of, 100 shuffles,
// seed 1 ), for the induced three-cell class 030T. The correct option is the one whose z is significant and positive (analysis::motif_is_significant); tasks_atlas_worm_test asserts that
// exactly one option is, and that it equals Diagnosis::answer ("edges", expected from research/atlas-a.md 4b).
//
// With no commit the level is not passed and the verdict asks for the bet (exit 3 in spike-run, as a case).
// level_kind "diagnosis": the level is decided by the committed answer and not by a world (the 302-cell circuit is no bench to build on), exactly as a diagnosis case; it hangs off
// a_path. The commit travels in the variant slot: "edges", "edges@N", "edges@N:W", "edges@revealed" (dx_common.h). Diagnosis::bet is true, Diagnosis::budget the free instrument reads.
#include "atlas_worm.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_a_motif();
namespace
{
using namespace atlas_worm;

const char* kAnswer = "edges";
constexpr int kShuffles = 100;
constexpr uint64_t kSeed = 1;

struct Bench
{
  double z[ 3 ] = { 0, 0, 0 };
  bool significant[ 3 ] = { false, false, false };
  int64_t count = 0;
  double null_mean[ 3 ] = { 0, 0, 0 };
};
const char* kIds[ 3 ] = { "edges", "degree", "blocks" };

int ffl_index()
{
  for ( int i = 0; i < analysis::kMotifClasses; ++i )
    if ( std::string( analysis::motif_info( i ).man ) == "030T" ) return i;
  return 0;
}

/// The null bench of the circuit that is run: the engine's own census and shuffles. Cached by the shape of the edge list (the given circuit is the same every time).
Bench compute( const Engine& e )
{
  const Structure st = e.structure();
  int32_t n = 0;
  for ( const auto& p : st.populations ) n += p.n;
  std::vector< analysis::Edge > edges;
  uint64_t h = 1469598103934665603ULL;
  for ( const auto& c : e.connections() )
  {
    edges.push_back( { c.pre, c.post, c.weight } );
    h = ( h ^ static_cast< uint64_t >( c.pre * 7919 + c.post ) ) * 1099511628211ULL;
  }
  h ^= static_cast< uint64_t >( n );
  static std::map< uint64_t, Bench > cache;
  if ( auto it = cache.find( h ); it != cache.end() ) return it->second;
  // blocks: the group of a population is the cell-type block of its cells
  std::map< std::string, int32_t > block_id;
  for ( size_t k = 0; k < st.groups.size(); ++k ) block_id[ st.groups[ k ].id ] = static_cast< int32_t >( k );
  std::vector< int32_t > block_of( static_cast< size_t >( n ), -1 );
  for ( const auto& p : st.populations )
    if ( block_id.count( p.group ) )
      for ( int32_t k = 0; k < p.n; ++k ) block_of[ static_cast< size_t >( p.first_index + k ) ] = block_id[ p.group ];
  const analysis::Graph g = analysis::make_graph( edges, n, analysis::Sign::Any );
  const analysis::NullModel models[ 3 ] = { analysis::NullModel::EdgeCount, analysis::NullModel::Degree, analysis::NullModel::Blocks };
  const int f = ffl_index();
  Bench b;
  for ( int m = 0; m < 3; ++m )
  {
    const analysis::MotifResult r = analysis::motif_zscores( g, models[ m ], block_of, kShuffles, kSeed );
    b.z[ m ] = r.z[ static_cast< size_t >( f ) ];
    b.significant[ m ] = analysis::motif_is_significant( r, f ) && b.z[ m ] > 0;
    b.count = r.counts[ static_cast< size_t >( f ) ];
    b.null_mean[ m ] = r.null_mean[ static_cast< size_t >( f ) ];
  }
  cache[ h ] = b;
  return b;
}

class AMotif : public micro::MicroBase
{
public:
  AMotif() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "a_motif";
    i.title = "Atlas: chance or design?";
    i.chapter = "a_worm";
    i.view = "dashboard";
    i.duration_ms = 4000;   // nothing is driven: the circuit is only read
    i.par_neurons = 302;
    i.default_model = "iaf_psc_alpha";
    i.brief = "In the wiring of the whole worm, a cell that drives a second cell and also drives a third that the second drives too forms a feed-forward loop. Are there more of them than chance? "
              "That depends on what chance keeps fixed. Under which null model is the feed-forward loop more frequent than chance? Options: shuffles that keep only the number of edges; shuffles that keep "
              "the number of connections of every cell; shuffles that keep those and also the number of connections between the cell types. Commit to one. The engine then runs all three shuffles "
              "on the wiring and tells you what it found.";
    i.concepts_new = { "null_model" };
    i.concepts_used = { "pathway" };
    i.recipe = "Commit to one null model on the bet panel. The wiring is given and cannot be edited; the engine counts the feed-forward loops of the real wiring and of one hundred shuffled graphs of each kind.";
    i.note = { "Compared with what?",
               "A motif is a pattern of connections that occurs significantly more often than in randomised networks. Whether a count is high depends on the randomised networks it is compared with: "
               "in this wiring table the feed-forward loop is more frequent than in graphs with the same number of edges, and less frequent than in graphs that also keep the number of connections "
               "of every cell. Milo et al. defined network motifs in this way and found them in networks including the synaptic network of C. elegans; their data and null model differ from these, "
               "so the two results are not the same claim (Milo et al. 2002). The wiring used here is from Cook et al. 2019.",
               "Milo R, Shen-Orr S, Itzkovitz S, Kashtan N, Chklovskii D, Alon U (2002) Science 298:824-827, doi:10.1126/science.298.5594.824; "
               "Cook SJ et al. (2019) Nature 571:63-71, doi:10.1038/s41586-019-1352-7" };
    return i;
  }

  std::string level_kind() const override { return "diagnosis"; }
  bool on_spine() const override { return false; }
  std::string parent() const override { return "a_path"; }

  Given given() const override
  {
    Given g;
    g.circuit = "a_motif_given.json";
    // printed by `spike-run --locked-digest circuits/a_motif_given.json`
    g.digest.pops = "99c385f3147f3a750b5dcac056446121";
    g.digest.wiring = "7883ad115f9b8f4971f2a1ca79e44e3f";
    g.digest.types = "6154d30029574d4924c59699e5a76f29";
    g.digest.channels = "643d5437104296e21d906ecb15b2c96a";
    return g;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    have_ = false;
  }
  void set_variant( const std::string& v ) override
  {
    Task::set_variant( v );
    commit_ = parse_commit( v );
    if ( commit_.has && !commit_.revealed && commit_.cand != kAnswer ) ++wrong_;
  }
  std::map< std::string, std::string > variants() const override { return {}; }

  Diagnosis diagnosis() const override
  {
    Diagnosis d;
    d.circuit = "a_motif_given.json";
    d.symptoms = "The whole chemical wiring of the C. elegans hermaphrodite: three hundred and two cells, one given circuit.";
    d.answer = kAnswer;
    d.candidates = ordered( "a_motif", { "edges", "degree", "blocks" }, kAnswer );
    d.labels = { { "edges", "the same number of edges, placed at random" }, { "degree", "the same number of connections in and out of every cell" },
      { "blocks", "the same connections of every cell and the same number of connections between the cell types" } };
    d.explain = { { "edges", "Against graphs that keep only the number of edges the feed-forward loop is more frequent than chance in this wiring." },
      { "degree", "Against graphs that keep the connections of every cell the feed-forward loop is not more frequent than chance in this wiring." },
      { "blocks", "Keeping the cell types as well does not change that: against these graphs the feed-forward loop is not more frequent than chance in this wiring." } };
    d.probes = { { "scope", "AVA", "opens the activity of a cell class: an instrument read, counted against the budget" } };
    d.bet = true;
    d.budget = 3;
    return d;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "A count means little without a comparison. A shuffled graph is a comparison, and what the shuffle keeps fixed decides what the count is compared with.", "" },
      { 2, "Some cells have far more connections than others. Whether a shuffle keeps that decides what the real count is compared with.", "AVA" },
      { 3, "The three shuffles keep more and more of the real wiring. Decide which of them a count has to beat to mean something, and commit before the engine runs them: a prediction earns more than trial and error.", "AVA" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    return { { "wrong_guesses", { static_cast< double >( wrong_ ), "guesses", true } }, { "probes", { static_cast< double >( commit_.probes ), "probes", true } } };
  }

  /// The engine's own z-scores (after the first slice), for the tests and the panel's bench.
  bool bench( double z[ 3 ], bool sig[ 3 ] ) const
  {
    if ( !have_ ) return false;
    for ( int m = 0; m < 3; ++m ) { z[ m ] = bench_.z[ m ]; sig[ m ] = bench_.significant[ m ]; }
    return true;
  }

  Score score() const override
  {
    Score s;
    s.parts[ "wrong_guesses" ] = wrong_;
    s.parts[ "probes" ] = commit_.probes;
    s.parts[ "committed" ] = commit_.has ? 1.0 : 0.0;
    if ( have_ )
      for ( int m = 0; m < 3; ++m )
      {
        s.parts[ std::string( "z_" ) + kIds[ m ] ] = bench_.z[ m ];
        s.parts[ std::string( "significant_" ) + kIds[ m ] ] = bench_.significant[ m ] ? 1.0 : 0.0;
      }
    const bool known = commit_.has && ( commit_.cand == "edges" || commit_.cand == "degree" || commit_.cand == "blocks" );
    int pick = -1;
    for ( int m = 0; m < 3; ++m )
      if ( known && commit_.cand == kIds[ m ] ) pick = m;
    s.passed = false;
    s.value = 0;
    if ( !wiring_.ok() ) s.verdict = "Not wired: " + wiring_.message() + ".";
    else if ( !mech_.empty() ) s.verdict = mech_;
    else if ( !have_ ) s.verdict = "The engine has not counted the loops yet. Run the circuit.";
    else if ( !commit_.has ) s.verdict = "No prediction committed yet. Commit to a null model first, then the engine runs the three shuffles and tells you what it found.";
    else if ( commit_.revealed ) s.verdict = std::string( "The answer was revealed: " ) + kAnswer + ". Nothing is scored for a revealed bet.";
    else if ( !known ) s.verdict = "'" + commit_.cand + "' is not one of the three null models.";
    else
    {
      const double z = bench_.z[ pick ];
      std::string finding;
      if ( bench_.significant[ pick ] ) finding = "against that shuffle the feed-forward loop is more frequent than chance";
      else if ( z < 0 && std::fabs( z ) > analysis::kSignificantZ ) finding = "against that shuffle the feed-forward loop is less frequent than chance, not more";
      else finding = "against that shuffle the feed-forward loop is not clearly more frequent than chance";
      s.passed = bench_.significant[ pick ];
      s.value = s.passed ? bet_value( commit_.wrong, commit_.probes, 3 ) : 0.0;
      s.verdict = ( s.passed ? "Correct: " : "Not that one: " ) + finding + ".";
      if ( s.passed ) s.verdict += " What chance keeps fixed decides whether a motif counts: under the other two shuffles the same loops are not more frequent than chance.";
    }
    return s;
  }

protected:
  void inspect( Engine& e ) override
  {
    bench_ = compute( e );
    have_ = true;
  }
  void drive( Engine&, double ) override {}
  void observe( Engine&, double ) override {}

private:
  Commit commit_;
  Bench bench_;
  bool have_ = false;
  int wrong_ = 0;
};
}  // namespace

std::unique_ptr< Task > make_a_motif() { return std::make_unique< AMotif >(); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( a_motif, 992, "a_motif.json", make_a_motif ) }
