// Micro-puzzle "m_tonic" (concept: tonic drive, a bias current). A cell that hums at rest can say "less"; a silent cell cannot.
//
// WORLD (4000 ms; dc sensor "cue", level 2.0 x the sensor weight in pA, on during [1700, 2700))
//   Motor "spikes": the spikes of the cell(s) wired to it.
//   Nothing is ever applied to the cell except the cue: any firing before the cue has to come from the cell's own steady (bias) current, or from a
//   cell upstream that has one.
// SCORING (rates per neuron of the pool wired to 'spikes')
//   rest_hz   [600, 1600]  >= 5 spikes/s (guided 3.5) and <= 60 (a cell racing at rest has nothing left to say "more" with)
//   cue_hz    [1950, 2700] <= 25 % of rest_hz (the cue must reach the cell through an inhibitory path: a sensor can only excite)
//   after_hz  [3100, 4000] >= 60 % of rest_hz (the hum comes back when the cue goes)
//   value 100 when passed, else 85 * (criteria met / 3).
// Mechanism (structure): some cell on the way to 'spikes' (or that cell itself) carries a bias current (I_e > 0); the cue reaches 'spikes' with a net inhibitory sign.
// Noise: 2 % per-episode gain and 4 % white per ms slice on the cue level (x0 guided, x2 researcher).
//
// Biology: many neurons are tonically active at rest; a drop below the resting rate is then a signal. Retinal ON/OFF cells and the cerebellar Purkinje
// cells (about 50 spikes/s with no input at all) are standard examples (Raman & Bean 1999).
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;
using micro::Window;

constexpr double kDur = 4000, kOn = 1700, kOff = 2700, kLevel = 2.0;

class MTonic : public MicroBase
{
public:
  MTonic() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_tonic";
    i.title = "Tonic drive";
    i.brief = "Most cells in a nervous system are silent until something excites them. But a cell that is held just over its threshold by a steady "
              "current of its own, a bias, fires all the time at a low rate, and that gives it something a silent cell lacks: it can signal "
              "'less'. Build a cell that hums at rest, with nothing applied to it, between about 5 and 60 spikes per second. Then the 'cue' "
              "arrives. The cue means 'stop': it must almost silence the cell while it lasts (a quarter of the resting rate or less), and the "
              "hum must come back when the cue goes. The bias is a parameter of the cell itself (open its advanced parameters: the steady "
              "current I_e); the cue is a sensor, and a sensor can only excite, so it has to reach the humming cell through a neuron "
              "of its own that you set to inhibitory.";
    i.chapter = "invertebrates";
    i.sensors = { "cue" };
    i.motors = { "spikes" };
    i.duration_ms = kDur;
    i.concepts_new = { "tonic_drive" };
    i.concepts_used = { "inhibition" };
    i.par_neurons = 2;
    i.default_model = "iaf_psc_alpha";
    i.recipe = "cue > cell B (inhibitory) > cell A > spikes. Cell A gets a bias I_e of about 400 pA in its advanced parameters (the plain cell's rheobase is "
               "about 375 pA, so 400 is just over it: roughly 35 spikes/s; 450 already gives 50 and 600 far too much). The cue sensor at its default "
               "weight makes B fire hard; B's link onto A at about -1000 pA silences A while the cue lasts.";
    i.kinds = { { "cue", micro::dc( kLevel ) }, { "spikes", micro::rate() } };
    i.bands = { { "rest_hz", "Hz", 5, 60 }, { "cue_over_rest", "x", 0, 0.25 } };
    i.timeline = { { kOn, kOff - kOn, "cue (stop)" } };
    i.note = { "Humming cells",
      "Cell bodies of cerebellar Purkinje neurons, taken out of the brain and given no input at all, fire spontaneously at about 50 spikes per second: "
      "a sodium current that flows between the spikes keeps pushing them over threshold, a steady drive of the cell's own. Because the baseline is high, the rate can go down as well as up, and a pause in the hum is as strong a message "
      "as a burst. A cell with no resting activity can only ever report 'more'.",
      "Raman IM, Bean BP (1999) Ionic currents underlying spontaneous action potentials in isolated cerebellar Purkinje neurons. J Neurosci 19:1663-1674, doi:10.1523/JNEUROSCI.19-05-01663.1999" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    gain_ = 1.0 + 0.02 * noise() * rng_.normal();
    rest_ = { 600, 1600, 0 };
    cue_ = { 1950, kOff, 0 };
    after_ = { 3100, kDur, 0 };
    pool_ = 1;
    lvl_ = 0;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "A cell fires without input only if something keeps pushing it: a steady current of its own. That current is a property of the "
                  "cell (its bias), not a sensor. And a sensor can only excite, so a 'stop' cue has to act through an inhibitory neuron.", "" },
      { 2, "Open the advanced parameters of the cell that is wired to 'spikes' and find the steady current I_e. The cue goes through a second neuron "
           "set to inhibitory that connects onto that cell.", "spikes" },
      { 3, "cue > cell B (inhibitory) > cell A > spikes. Give A a bias just over the current at which the plain cell starts to fire (a little "
           "above its rheobase), so it hums slowly: too little and it is silent, too much and it races. Make B's link onto A strong enough to cancel "
           "the bias while the cue lasts.", "spikes" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    return { { "rest_hz", { rest_.rate_hz( pool_ ), "Hz", false } } };
  }

  void inspect( Engine& ) override
  {
    const auto cue = g_.in_pops( "cue" ), out = g_.out_pops( "spikes" );
    if ( out.empty() || cue.empty() ) return;
    bool biased = false;
    for ( int p = 0; p < static_cast< int >( g_.st.populations.size() ); ++p )
    {
      const double ie = g_.param( p, "I_e" );
      if ( !std::isnan( ie ) && ie > 0 && g_.path( { p }, out, +1, 3, true ) ) biased = true;
    }
    if ( !biased )
      mech_ = "No cell on the way to 'spikes' has a steady drive of its own: nothing keeps the cell humming. Give a cell a bias current in its advanced parameters.";
    else if ( !g_.path( cue, out, -1, 3 ) )
      mech_ = "The cue does not reach the humming cell with an inhibitory sign: a sensor can only excite. Send the cue through a neuron of its own that is "
              "inhibitory (its outgoing link negative), onto the cell that fires.";
  }

  Score score() const override
  {
    Score s;
    const double rest = rest_.rate_hz( pool_ ), cue = cue_.rate_hz( pool_ ), aft = after_.rate_hz( pool_ );
    const double lo = mode_ == "guided" ? 3.5 : ( mode_ == "researcher" ? 6.0 : 5.0 );
    s.parts[ "rest_hz" ] = rest;
    s.parts[ "cue_hz" ] = cue;
    s.parts[ "after_hz" ] = aft;
    s.parts[ "cue_over_rest" ] = rest > 0 ? cue / rest : 0.0;
    const bool a = rest >= lo && rest <= 60, b = a && cue <= 0.25 * rest * ( mode_ == "guided" ? 1.4 : 1.0 ), c = a && aft >= 0.6 * rest;
    finish( s, ( a ? 1 : 0 ) + ( b ? 1 : 0 ) + ( c ? 1 : 0 ), 3 );
    if ( !done() )
      s.passed = false;
    apply_mech( s );
    std::ostringstream o;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done() )
      o << "Running.";
    else if ( rest < lo && !mech_.empty() )
      o << "The cell is silent while nothing is going on. " << mech_;
    else if ( rest < lo )
      o << "The cell is silent (or nearly) at rest: its bias current is under the level at which the cell starts to fire. Raise it a little.";
    else if ( rest > 60 )
      o << "The cell races at rest: its bias is far over threshold. A cell that already fires flat out cannot signal 'more'; lower the bias until it hums slowly.";
    else if ( !mech_.empty() )
      o << mech_;
    else if ( !b )
      o << "The cue does not silence the cell enough. Strengthen the inhibitory link from the cue's neuron onto the humming cell: it has to cancel the bias.";
    else if ( !c )
      o << "The hum does not come back after the cue. Is the humming cell still biased, and is the cue's inhibition switching off with the cue?";
    else
      o << "The cell hums at rest, falls silent for the cue and picks the hum up again: a cell with a bias can signal 'less'.";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    lvl_ = t_ >= kOn && t_ < kOff ? kLevel : 0.0;
    e.set_input( "cue", lvl_ > 0 ? lvl_ * gain_ * ( 1.0 + 0.04 * noise() * rng_.normal() ) : 0.0 );
  }

  void observe( Engine& e, double dt ) override
  {
    const double n = motor_.spike_count( e, "spikes", dt );
    pool_ = motor_.pool_size( e, "spikes" );
    if ( rest_.has( t_ ) ) rest_.n += n;
    if ( cue_.has( t_ ) ) cue_.n += n;
    if ( after_.has( t_ ) ) after_.n += n;
    rate_.tau_ms = 150;
    rate_now_ = rate_.step( n / std::max( pool_, 1.0 ) * 1000.0 / dt, dt );
    readouts_[ "cue" ] = lvl_;
    readouts_[ "rate_hz" ] = rate_now_;
    readouts_[ "rest_hz" ] = rest_.rate_hz( pool_ );
  }

private:
  double gain_ = 1, lvl_ = 0, rate_now_ = 0, pool_ = 1;
  Window rest_, cue_, after_;
  Lowpass rate_;
};

} // namespace

std::unique_ptr< Task > make_m_tonic() { return std::make_unique< MTonic >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_tonic, 28, "m_tonic.json", make_m_tonic ) }
