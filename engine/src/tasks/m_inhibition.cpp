// Micro-puzzle "m_inhibition" (concept 4: inhibition). A 'stop' signal must silence a cell that a 'drive' signal keeps firing.
//
// WORLD (3800 ms; two dc sensors, level x sensor weight in pA)
//   "drive": level 2.0 on [400, 3400).   "stop": level 2.0 on [1400, 2400).
//   Motor "out": spikes of the cell(s) wired to it.
//   At the lab's default sensor weight (300 pA) the drive gives 600 pA: an iaf cell fires at about 70 Hz.
// SCORING (noise: 4 % white per ms slice and 2 % per-episode gain on both levels, x0 guided, x2 researcher)
//   before : spikes in (600, 1400]   needs >= 8       the drive alone makes the cell fire
//   during : spikes in (1650, 2400]  needs <= 2 (guided <= 4)   ... and the stop silences it (the first 250 ms are allowed to take effect)
//   after  : spikes in (2650, 3400]  needs >= 8       ... and it fires again when the stop ends
//   value 100 when passed, else 85 * (criteria met / 3).
// A sensor can only excite, so the stop has to go through an inhibitory cell. Measured on the reference circuit (stop > B > A, B inhibitory):
// the rate of A during stop falls from 90 to 83 Hz at a B > A weight of -80 pA (the default), 60 at -300, 43 at -500, 28 at -600 and
// to 0 at -1000: roughly linear, so the weight that silences the cell is about 100 / (percent cut) times the weight tried.
// The verdict uses that: "cut by 33 %: about 3 times the weight".
//
// Biology: lateral inhibition in the compound eye of Limulus (Hartline, Wagner & Ratliff 1956): illuminating the neighbours of an
// ommatidium reduces its response; in the nervous system inhibition is carried by GABA and glycine neurons (Dale's principle: a neuron
// makes one kind of synapse, so an excitatory sensor needs an inhibitory interneuron to say stop).
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;
using micro::Graph;
using micro::Window;

constexpr double kDur = 3800, kDrive0 = 400, kDrive1 = 3400, kStop0 = 1400, kStop1 = 2400, kLevel = 2.0;

class MInhibition : public MicroBase
{
public:
  MInhibition() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_inhibition";
    i.title = "Inhibition";
    i.brief = "Excitation makes cells fire; inhibition makes them stop. A 'drive' input keeps a cell firing from 0.4 s to 3.4 s, and a "
              "'stop' input comes on for one second in the middle. Build a circuit in which 'out' fires with the drive alone, goes "
              "quiet (at most 2 spikes) while stop is on, and fires again when stop ends. A sensor can only excite a cell, so the stop "
              "has to act through a neuron of its own that you mark as inhibitory (the E / I switch): its spikes then pull the "
              "membrane of the cell it connects to down instead of up. Score: fires before, silent during, fires after.";
    i.chapter = "invertebrates";
    i.sensors = { "drive", "stop" };
    i.motors = { "out" };
    i.duration_ms = kDur;
    i.concepts_new = { "inhibition" };
    i.concepts_used = { "threshold", "excitation" };
    i.par_neurons = 2;
    i.default_model = "iaf_psc_alpha";
    i.recipe = "drive > cell A > out. stop > cell B (inhibitory) > cell A, with the B > A weight at about -1000 pA. The default -80 pA "
               "only slows A down a little (by about 8 %); the inhibition has to cut the whole drive, so the weight is roughly 12 "
               "times larger.";
    i.kinds = { { "drive", micro::dc( kLevel ) }, { "stop", micro::dc( kLevel ) }, { "out", micro::rate() } };
    i.bands = { { "during_spikes", "spikes", 0, 2 }, { "before_spikes", "spikes", 8, 400 } };
    i.timeline = { { kDrive0, kDrive1 - kDrive0, "drive on" }, { kStop0, kStop1 - kStop0, "stop on" } };
    i.note = { "Inhibition shapes the response",
      "Hartline and his colleagues recorded from single receptor units in the compound eye of the horseshoe crab Limulus and found that "
      "light on the neighbouring receptors reduces a unit's response: lateral inhibition. The effect is mutual and grows with the "
      "intensity and closeness of the neighbours, and it sharpens edges. Inhibition is the other half of every neural computation.",
      "Hartline HK, Wagner HG, Ratliff F (1956) Inhibition in the eye of Limulus. J Gen Physiol 39:651-673" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    gain_ = 1.0 + 0.02 * noise() * rng_.normal();
    before_ = { 600, 1400, 0 };
    during_ = { 1650, 2400, 0 };
    after_ = { 2650, 3400, 0 };
    rate_.tau_ms = 150;
    rate_.reset( 0 );
    drv_ = stp_ = 0;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "Inhibition is the opposite of excitation: the spikes of an inhibitory neuron lower the membrane of the cell they connect "
                  "to. A sensor can only excite, so the stop signal must act through a neuron of its own that you set to inhibitory.", "" },
      { 2, "The stop sensor goes to a second neuron (set it inhibitory with the E / I switch), and that neuron connects to the cell wired to "
           "'out'. Look at the weight of that last link: it has to be strong enough to cancel the drive.", "out" },
      { 3, "drive > cell A > out. stop > cell B (inhibitory) > cell A. The default inhibitory weight only slows A a little: the stop has to "
           "cancel the whole drive, so make that link several times stronger, and watch the verdict say whether stop slows the cell or "
           "silences it.", "out" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    return { { "during_spikes", { during_.n, "spikes", true } } };
  }

  void inspect( Engine& ) override
  {
    const auto stop = g_.in_pops( "stop" ), out = g_.out_pops( "out" ), drive = g_.in_pops( "drive" );
    if ( out.empty() || stop.empty() ) return;
    if ( !g_.path( stop, out, 0, 3, true ) )
      mech_ = "The stop does not reach the cell that fires: there is no path from stop to 'out'.";
    else if ( !g_.path( stop, out, -1, 3 ) )
      mech_ = "The stop reaches the cell only by exciting it: a sensor can only excite. Send the stop through a neuron of its own that is inhibitory "
              "(its outgoing link negative), onto the cell that fires.";
    else if ( !drive.empty() && !g_.path( drive, out, 0, 3, true ) )
      mech_ = "The drive does not reach the cell that fires.";
  }

  Score score() const override
  {
    Score s;
    const double max_during = mode_ == "guided" ? 4 : 2;
    s.parts[ "before_spikes" ] = before_.n;
    s.parts[ "during_spikes" ] = during_.n;
    s.parts[ "after_spikes" ] = after_.n;
    s.parts[ "before_hz" ] = before_.rate_hz();
    s.parts[ "during_hz" ] = during_.rate_hz();
    const bool a = before_.n >= 8, b = during_.n <= max_during && a, c = after_.n >= 8;
    const double cut = before_.rate_hz() > 0 ? 100.0 * ( 1.0 - during_.rate_hz() / before_.rate_hz() ) : 0.0;
    finish( s, ( a ? 1 : 0 ) + ( b ? 1 : 0 ) + ( c ? 1 : 0 ), 3 );
    if ( !done() )
      s.passed = false;
    apply_mech( s );
    std::ostringstream o;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done() )
      o << "Running.";
    else if ( !mech_.empty() )
      o << mech_;
    else if ( !a )
      o << "The cell does not fire with the drive alone. The drive is a current: check that it reaches the cell and is over its rheobase.";
    else if ( !b && cut < 4.0 )
      o << "The stop made no difference to the cell. Is the neuron it goes through inhibitory, and does that neuron connect to the cell that fires?";
    else if ( !b )
      o << "Stop slows the cell but does not silence it. The inhibition is too weak against the drive: strengthen the inhibitory link.";
    else if ( !c )
      o << "Silent during stop, but it does not recover afterwards. Is the inhibitory cell still firing when the stop is off, or the drive disconnected?";
    else
      o << "Stop silences the cell and it recovers when the stop ends.";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    drv_ = t_ >= kDrive0 && t_ < kDrive1 ? kLevel : 0.0;
    stp_ = t_ >= kStop0 && t_ < kStop1 ? kLevel : 0.0;
    e.set_input( "drive", drv_ > 0 ? drv_ * gain_ * ( 1.0 + 0.04 * noise() * rng_.normal() ) : 0.0 );
    e.set_input( "stop", stp_ > 0 ? stp_ * gain_ * ( 1.0 + 0.04 * noise() * rng_.normal() ) : 0.0 );
  }

  void observe( Engine& e, double dt ) override
  {
    double n = motor_.spike_count( e, "out", dt );
    for ( Window* w : { &before_, &during_, &after_ } )
      if ( w->has( t_ ) ) w->n += n;
    rate_.step( n / motor_.pool_size( e, "out" ) * 1000.0 / dt, dt );
    readouts_[ "drive" ] = drv_;
    readouts_[ "stop" ] = stp_;
    readouts_[ "rate_hz" ] = rate_.y;
    readouts_[ "before_spikes" ] = before_.n;
    readouts_[ "during_spikes" ] = during_.n;
  }

private:
  double gain_ = 1, drv_ = 0, stp_ = 0;
  Window before_, during_, after_;
  Lowpass rate_;
};

} // namespace

std::unique_ptr< Task > make_m_inhibition() { return std::make_unique< MInhibition >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_inhibition, 17, "m_inhibition.json", make_m_inhibition ) }
