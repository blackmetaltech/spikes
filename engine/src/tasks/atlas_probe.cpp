// "atlas_probe": the DEVELOPMENT-ONLY level that proves the locked-wiring loop of the Atlas wing (docs/ROADMAP-CONNECTOME.md, iteration A1).
// It is not in the campaign: the game offers it only in a debug build with SP_ATLAS set (Campaign.dev_ids), and the release export leaves out its circuit files.
//
// The wiring is GIVEN and locked (circuits/atlas_probe_given.json): four cell types, each a population with explicit edges,
//     plus -> P -> M      (P: the plus relay, excitatory)     minus -> N -> I -> M      (N: the minus relay, excitatory; I: the interneuron)     M -> 'diff'
// The wiring says WHO connects to WHOM and not whether I excites or inhibits, or how strongly: the player supplies that. The level declares the free
// parameters of the cell type I (its sign and its scale, scale range 0.25 to 2.0); P and N are fixed. The mechanism check reads the groups: an interneuron type that still
// excites its targets makes minus add to plus. The behaviour is the one of m_subtraction: M ('diff') fires only when plus is stronger than minus.
//
// WORLD (3800 ms; two dc sensors "plus" and "minus", level x sensor weight in pA). Seven segments of 500 ms from t = 300 ms, (plus, minus) and the required answer:
//     0: (0, 0) silent   1: (3, 0) FIRE   2: (3, 3) silent   3: (0, 3) silent   4: (3, 2) FIRE   5: (2, 2) silent   6: (3, 0) FIRE
//   A segment is judged on (t0 + 100, t0 + 500]: FIRE needs >= 5 spikes of 'diff', silent needs <= 1 (guided <= 3). Noise: 4 % per slice, 2 % per episode, x0 guided, x2 researcher.
// SCORING  fire_ok (3 of 3), silent_ok (4 of 4). value 100 when passed, else 85 * (criteria met / 2). Too weak an inhibition lets equal inputs through; too strong silences plus 3 against minus 2.
//
// Locked-level contract (Task::given): the circuit that is run must equal the given one in everything locked or not free, or the run is rejected by make_task's guard.
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;

constexpr int kSeg = 7;
constexpr double kT0 = 300, kSlot = 500, kDur = 3800;
const double kPlus[ kSeg ] = { 0, 3, 3, 0, 3, 2, 3 };
const double kMinus[ kSeg ] = { 0, 0, 3, 3, 2, 2, 0 };
const bool kFire[ kSeg ] = { false, true, false, false, true, false, true };

class AtlasProbe : public MicroBase
{
public:
  AtlasProbe() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "atlas_probe";
    i.title = "Atlas probe: given wiring, free signs";
    i.brief = "The wiring of this circuit is given and cannot be edited: four cell types, who connects to whom, with the delays. What the wiring does not tell you is whether "
              "a cell type excites or inhibits its targets, and how strongly. Each cell type has a card; the ones the level marks free can be changed. Set them so that the "
              "output 'diff' fires only when 'plus' is stronger than 'minus': not when both are off, not when minus is on alone, not when the two are equal, but yes when plus is "
              "stronger even by a little. Score: all three firing segments answered, all four quiet ones kept silent.";
    i.chapter = "fly_compass";
    i.sensors = { "plus", "minus" };
    i.motors = { "diff" };
    i.duration_ms = kDur;
    i.concepts_new = { "subtraction" };
    i.concepts_used = { "inhibition" };
    i.par_neurons = 10;
    i.default_model = "iaf_psc_alpha";
    i.recipe = "Cell type I: sign inhibitory, scale about 1. Scales from 0.85 to 1.15 pass in every mode; below that equal inputs still fire, above it plus 3 against minus 2 goes quiet.";
    i.kinds = { { "plus", micro::dc( 3.0 ) }, { "minus", micro::dc( 3.0 ) }, { "diff", micro::rate() } };
    i.bands = { { "fire_ok", "of 3", 3, 3 }, { "silent_ok", "of 4", 4, 4 } };
    for ( int s = 0; s < kSeg; ++s )
      i.timeline.push_back( { kT0 + kSlot * s, kSlot, std::string( "plus " ) + fmt( kPlus[ s ] ) + " / minus " + fmt( kMinus[ s ] ) } );
    return i;
  }

  std::string level_kind() const override { return "atlas"; }

  Given given() const override
  {
    Given g;
    g.circuit = "atlas_probe_given.json";
    // printed by `spike-run --locked-digest circuits/atlas_probe_given.json`; tasks_atlas_test checks that the given and the reference circuits both have exactly these
    g.digest.pops = "62ec39c0728faf8b0b76361353c0a7f5";
    g.digest.wiring = "074f4fd8eb4d03ff456866f57f25d9e0";
    g.digest.types = "137d104b2dc39c5c31b77d627791b07e";
    g.digest.channels = "669c32be42ad7c73d985076b524c1f92";
    return g;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    gain_ = 1.0 + 0.02 * noise() * rng_.normal();
    for ( int s = 0; s < kSeg; ++s ) n_[ s ] = 0;
    p_ = m_ = 0;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "A wiring diagram says who connects to whom. It does not say whether a connection excites or inhibits, or how strong it is: those have to be supplied, and the circuit "
                  "works only if they are right. Subtraction needs one of the cell types to inhibit.", "" },
      { 2, "Look at the cell type that carries 'minus' toward the output through the interneurons. Its card has a sign and a scale: the sign decides whether minus adds or subtracts.", "I" },
      { 3, "Set the interneuron type to inhibitory, then adjust its scale: too weak and equal inputs still fire, too strong and a plus that is only a little stronger than minus goes quiet. "
           "The verdict says which way you are off.", "I" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    double fire = 0, nf = 0;
    for ( int s = 0; s < kSeg; ++s )
      if ( kFire[ s ] ) { fire += n_[ s ]; nf += 1; }
    return { { "fire_rate_hz", { nf > 0 ? fire / nf / 0.4 : 0.0, "Hz", true } } };
  }

  void inspect( Engine& ) override
  {
    const auto plus = g_.in_pops( "plus" ), minus = g_.in_pops( "minus" ), out = g_.out_pops( "diff" );
    if ( out.empty() || plus.empty() || minus.empty() ) return;
    // the groups: a cell type that projects onto the output and that the level lets the player set (its sign is the player's) must carry inhibition
    for ( const auto& gr : g_.st.groups )
    {
      if ( !gr.free_sign ) continue;
      for ( const auto& pr : g_.st.projections )
        if ( pr.n_edges > 0 && std::find( out.begin(), out.end(), pr.post_index ) != out.end() && g_.pop( pr.pre_index ).group == gr.id && gr.sign > 0 )
        {
          mech_ = "The cell type '" + gr.id + "' still excites the output, so 'minus' adds to 'plus' instead of subtracting. Open its card and set its sign to inhibitory.";
          return;
        }
    }
    if ( !g_.path( plus, out, +1, 3, true ) )
      mech_ = "'plus' does not excite the cell that is read on 'diff': the output needs excitation from plus.";
    else if ( !g_.path( minus, out, -1, 3 ) )
      mech_ = "'minus' reaches the output only by exciting it, so it adds to plus instead of subtracting. Some cell type on its way has to inhibit.";
  }

  Score score() const override
  {
    Score s;
    const double max_silent = mode_ == "guided" ? 3 : 1;
    double fire_ok = 0, silent_ok = 0;
    for ( int k = 0; k < kSeg; ++k )
    {
      if ( kFire[ k ] ) { if ( n_[ k ] >= 5 ) fire_ok += 1; }
      else if ( n_[ k ] <= max_silent ) silent_ok += 1;
    }
    s.parts[ "fire_ok" ] = fire_ok;
    s.parts[ "silent_ok" ] = silent_ok;
    for ( int k = 0; k < kSeg; ++k ) s.parts[ "seg" + std::to_string( k ) ] = n_[ k ];
    const bool a = fire_ok >= 3, b = silent_ok >= 4;
    finish( s, ( a ? 1 : 0 ) + ( b ? 1 : 0 ), 2 );
    if ( !done() ) s.passed = false;
    apply_mech( s );
    std::ostringstream o;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done() )
      o << "Running.";
    else if ( !mech_.empty() )
      o << mech_;
    else if ( fire_ok == 0 && n_[ 1 ] == 0 )
      o << "Silent even when only plus is on: the excitation that plus gives the output is too weak or has been taken away.";
    else if ( !b && a )
      o << "Fired in a segment where minus should have cancelled plus: the inhibition is too weak. Raise the scale of the inhibitory cell type.";
    else if ( !a && b )
      o << "Silent in a segment where plus was stronger than minus: the inhibition is too strong. Lower the scale of the inhibitory cell type.";
    else if ( !a && !b )
      o << "Neither right: some segments that should fire do not and some that should be quiet fire. Check the sign of the inhibitory cell type first, then its scale.";
    else
      o << "Output = plus minus minus: it fires when plus clearly beats minus, and stays quiet whenever minus is as strong as plus.";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    p_ = m_ = 0;
    const int k = seg_at( t_ );
    if ( k >= 0 ) { p_ = kPlus[ k ]; m_ = kMinus[ k ]; }
    e.set_input( "plus", p_ > 0 ? p_ * gain_ * ( 1.0 + 0.04 * noise() * rng_.normal() ) : 0.0 );
    e.set_input( "minus", m_ > 0 ? m_ * gain_ * ( 1.0 + 0.04 * noise() * rng_.normal() ) : 0.0 );
  }

  void observe( Engine& e, double dt ) override
  {
    const double n = motor_.spike_count( e, "diff", dt );
    for ( int k = 0; k < kSeg; ++k )
    {
      const double t0 = kT0 + kSlot * k;
      if ( t_ > t0 + 100 && t_ <= t0 + kSlot ) n_[ k ] += n;
    }
    double fire_ok = 0, silent_ok = 0;
    for ( int k = 0; k < kSeg; ++k )
    {
      if ( kFire[ k ] && n_[ k ] >= 5 ) fire_ok += 1;
      if ( !kFire[ k ] && n_[ k ] <= ( mode_ == "guided" ? 3 : 1 ) && t_ > kT0 + kSlot * k + kSlot ) silent_ok += 1;
    }
    readouts_[ "plus" ] = p_;
    readouts_[ "minus" ] = m_;
    readouts_[ "difference" ] = p_ - m_;
    readouts_[ "fire_ok" ] = fire_ok;
    readouts_[ "silent_ok" ] = silent_ok;
  }

private:
  static int seg_at( double t )
  {
    for ( int k = 0; k < kSeg; ++k )
      if ( t >= kT0 + kSlot * k && t < kT0 + kSlot * ( k + 1 ) ) return k;
    return -1;
  }

  double gain_ = 1, n_[ kSeg ] = {}, p_ = 0, m_ = 0;
};

} // namespace

std::unique_ptr< Task > make_atlas_probe() { return std::make_unique< AtlasProbe >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( atlas_probe, 990, "atlas_probe.json", make_atlas_probe ) }
