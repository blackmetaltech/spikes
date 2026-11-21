// Micro-puzzle "m_adaptation" (concept 6: spike-frequency adaptation). A constant step must make a cell fire fast at first and then slow down.
//
// WORLD (4000 ms; dc sensor "step", level 3.0 x sensor weight in pA, on during [500, 3500))
//   Motor "spikes": spikes of the cell(s) wired to it.
// SCORING
//   early_hz : mean rate in (500, 600]       late_hz : mean rate in (2500, 3500], both per neuron of the pool wired to 'spikes'
//   ratio    : early_hz / late_hz            needs >= 3.5 (guided 2.5, researcher 4)
//   late_hz  >= 3 Hz (the cell must keep firing: a cell that only emits a few spikes and stops has not adapted, it is silent)
//   early_hz >= 15 Hz (a real burst at the onset)
//   value 100 when passed, else 85 * (criteria met / 3).
// Noise: 2 % per-episode gain and 4 % white per ms slice on the level (x0 guided, x2 researcher).
// Measured at the lab's 300 pA/unit sensor weight (900 pA): a plain integrate-and-fire cell has ratio 1.0; the adaptive exponential cell as it comes
// (NEST default a 4 nS, b 80.5 pA, tau_w 144 ms) 2.3 and the Adapting preset (a 0, b 60, tau_w 100) 1.6: it takes a longer tau_w (400 ms: ratios 4-6)
// or a much larger b to reach 3.5 (docs/circuits/m_adaptation.md).
//
// Biology: spike-frequency adaptation is a slow, spike-triggered hyperpolarising current (Ca2+-activated K+, M-current): cortical
// regular-spiking pyramidal cells fire at 100-200 Hz at the start of a current step and settle at tens of Hz within 100-200 ms
// (Connors & Gutnick 1990; Brette & Gerstner 2005 reproduce it with the adaptation variable w, spike-triggered jump b, time constant tau_w).
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;
using micro::Graph;
using micro::Window;

constexpr double kDur = 4000, kOn = 500, kOff = 3500, kLevel = 3.0;

class MAdaptation : public MicroBase
{
public:
  MAdaptation() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_adaptation";
    i.title = "Adaptation";
    i.brief = "Hold a steady current on a neuron and a plain integrate-and-fire cell fires at one fixed rate for as long as you like. Real "
              "neurons slow down: they fire fast at the start of a step and then settle at a lower rate, because each spike leaves behind a "
              "slow current that holds the membrane back (spike-frequency adaptation). Build a cell that does that: its rate in the first "
              "100 ms of the step must be at least 3.5 times its rate in the last second, and it must keep firing (at least 3 per second). "
              "The cell you need is the adaptive exponential one (the lab opens on it): its parameter b is the jump of the adaptation "
              "current at each spike, and tau_w how slowly it fades. The step is the sensor weight times 3.";
    i.chapter = "invertebrates";
    i.sensors = { "step" };
    i.motors = { "spikes" };
    i.duration_ms = kDur;
    i.concepts_new = { "adaptation" };
    i.concepts_used = { "threshold" };
    i.par_neurons = 1;
    i.default_model = "aeif_cond_alpha";
    i.recipe = "step > one aeif cell > spikes. Keep the default step weight (300) and the cell's NEST-default values, and in its advanced "
               "parameters set tau_w to 300-400 ms: the cell starts near 50 spikes/s and settles near 10. The longer the adaptation current "
               "takes to fade, the more of it builds up; b alone only helps a little (even 200 pA gives a ratio of 2.7), and with b = 0 there "
               "is no adaptation at all.";
    i.kinds = { { "step", micro::dc( kLevel ) }, { "spikes", micro::rate() } };
    i.bands = { { "ratio", "x", 3.5, 30 }, { "late_hz", "Hz", 3, 200 } };
    i.timeline = { { kOn, kOff - kOn, "current step" } };
    i.note = { "Why neurons slow down",
      "Pyramidal cells of the neocortex given a steady current step fire a fast burst and then slow to a lower steady rate within 100 to "
      "200 ms. The slow potassium currents that cause it, activated by calcium entering with each spike or by sub-threshold depolarisation "
      "(the M-current), make the cell report change rather than constant input. The adaptive exponential integrate-and-fire model "
      "captures this with one extra variable, w, that jumps by b at every spike and decays with time constant tau_w.",
      "Connors BW, Gutnick MJ (1990) Intrinsic firing patterns of diverse neocortical neurons. Trends Neurosci 13:99-104; "
      "Brette R, Gerstner W (2005) Adaptive exponential integrate-and-fire model as an effective description of neuronal activity. "
      "J Neurophysiol 94:3637-3642" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    gain_ = 1.0 + 0.02 * noise() * rng_.normal();
    early_ = { kOn, kOn + 100, 0 };
    late_ = { 2500, kOff, 0 };
    total_ = 0;
    lvl_ = 0;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "Adaptation is a slow current switched on by the neuron's own spikes: each spike adds a little hyperpolarising current that "
                  "fades slowly, so under a steady drive the rate falls. A plain integrate-and-fire cell has no such current.", "" },
      { 2, "Use the adaptive exponential cell (aeif) in the parts bin, not the plain integrate-and-fire one. Its parameters b (the jump at each "
           "spike) and tau_w (how slowly it fades) set how much and how long it adapts. If the cell never fires, the 'step' weight is too small.",
        "spikes" },
      { 3, "step > one aeif cell > spikes. The adaptation current only builds up if it fades more slowly than the cell spikes: lengthen tau_w "
           "well beyond its default, and give b a little more as well if you want it deeper. With b at zero there is no adaptation at all, and "
           "too much stops the cell firing at the end.", "spikes" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    return { { "adaptation_ratio", { late_.rate_hz() > 0 ? early_.rate_hz() / late_.rate_hz() : 0.0, "x", false } } };
  }

  void inspect( Engine& ) override
  {
    for ( int p : g_.out_pops( "spikes" ) )
    {
      const std::string& m = g_.pop( p ).model;
      // a cell has an adaptation current of its own when its model carries one that is switched on: the jump b of the adaptive exponential cells, the recovery jump d of an Izhikevich cell,
      // the spike-triggered conductances q_sfa / q_rr of the sfa cell, the spike-triggered threshold steps alpha_1 / alpha_2 of the mat2 cells (off by default in NEST), the spike-triggered
      // kernels of the gif cells. Whether it adapts ENOUGH is the behaviour (the ratio below), not this check.
      auto on = [ & ]( const char* k, bool default_on ) { const double v = g_.param( p, k ); return std::isnan( v ) ? default_on : v > 0; };
      const bool adaptive = ( m.rfind( "aeif", 0 ) == 0 && on( "b", true ) ) || ( m == "izhikevich" && on( "d", true ) ) ||
                            ( m.rfind( "iaf_cond_exp_sfa_rr", 0 ) == 0 && ( on( "q_sfa", true ) || on( "q_rr", true ) ) ) ||
                            ( m.rfind( "gif_", 0 ) == 0 ) || ( ( m == "mat2_psc_exp" || m == "amat2_psc_exp" ) && ( on( "alpha_1", false ) || on( "alpha_2", false ) ) );
      if ( !adaptive )
      {
        mech_ = "The cell you read has no adaptation current of its own (a plain integrate-and-fire cell, or an adaptive one with the jump set to zero): "
                "it fires at one fixed rate however long the step lasts. Adaptation is a property of the cell, so use one that has it.";
        return;
      }
    }
  }

  Score score() const override
  {
    Score s;
    const double need = mode_ == "guided" ? 2.5 : ( mode_ == "researcher" ? 4.0 : 3.5 );
    const double e = early_.rate_hz(), l = late_.rate_hz();
    const double ratio = l > 0 ? e / l : 0.0;
    s.parts[ "early_hz" ] = e;
    s.parts[ "late_hz" ] = l;
    s.parts[ "ratio" ] = ratio;
    const bool a = e >= 15, b = l >= 3, c = ratio >= need;
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
    else if ( total_ < 1 )
      o << "The cell never fired. The step current is below its rheobase: strengthen the step sensor (this cell needs more current than "
           "a plain integrate-and-fire cell).";
    else if ( !a )
      o << "The cell hardly fires at the start of the step: the drive is barely over threshold. Strengthen the step.";
    else if ( !b )
      o << "The cell fired fast at first, then fell nearly silent. It adapts, but it has to keep firing: lower the adaptation or strengthen the step.";
    else if ( !c )
      o << "The rate falls only a little over the step. Give the cell more adaptation: let the adaptation current fade more slowly (tau_w); "
           "raising b alone helps only a little.";
    else
      o << "The cell adapts: fast at the start of the step, much slower by the end, and still firing.";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    lvl_ = t_ >= kOn && t_ < kOff ? kLevel : 0.0;
    e.set_input( "step", lvl_ > 0 ? lvl_ * gain_ * ( 1.0 + 0.04 * noise() * rng_.normal() ) : 0.0 );
  }

  void observe( Engine& e, double dt ) override
  {
    const double n = motor_.spike_count( e, "spikes", dt );
    const double pool = motor_.pool_size( e, "spikes" );
    total_ += n;
    // per-neuron rates: the windows count the summed spikes, so divide by the pool
    if ( early_.has( t_ ) ) early_.n += n / pool;
    if ( late_.has( t_ ) ) late_.n += n / pool;
    rate_.tau_ms = 150;
    rate_now_ = rate_.step( n / pool * 1000.0 / dt, dt );
    readouts_[ "step" ] = lvl_;
    readouts_[ "rate_hz" ] = rate_now_;
    readouts_[ "early_hz" ] = early_.rate_hz();
    readouts_[ "late_hz" ] = late_.rate_hz();
    readouts_[ "ratio" ] = late_.rate_hz() > 0 ? early_.rate_hz() / late_.rate_hz() : 0.0;
  }

private:
  double gain_ = 1, total_ = 0, lvl_ = 0, rate_now_ = 0;
  Window early_, late_;
  Lowpass rate_;
};

} // namespace

std::unique_ptr< Task > make_m_adaptation() { return std::make_unique< MAdaptation >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_adaptation, 28, "m_adaptation.json", make_m_adaptation ) }
