// Micro-puzzle "m_threshold" (concept 1: threshold and rheobase). Make one cell fire on a big step of current and ignore a small blip.
//
// WORLD (4000 ms; the stimulus is a dc sensor "pulse", whose level is multiplied by the sensor weight (pA per unit) in the circuit)
//   level 0 baseline; a small blip of level B on [800, 1400); a big step of level S on [2200, 3600); 0 elsewhere.
//   B and S are NOT the same on every seed: the world draws them from the seed (the pass band of the weight is the intersection over
//   seeds, and it moves when the stimulus does, so no single number can be memorised and typed in):
//     standard  B = 0.68 +- 0.05, S = 1.00 +- 0.10     guided  B = 0.55 +- 0.03, S = 1.00 +- 0.05     researcher  B = 0.72 +- 0.05, S = 1.00 +- 0.08
//   plus the usual noise (white 4 % per ms slice, 2 % per-episode gain; x0 guided, x2 researcher).
//   Motor "spike": spikes of the cell(s) wired to it.
// SCORING
//   step_spikes : spikes in (2200, 3600]      needs >= 8 (guided 5)         the cell fires on the step
//   blip_spikes : spikes in (800, 1400]       needs 0 (guided <= 1)         ... and not on the blip
//   off_spikes  : spikes outside both pulses  needs 0 (guided <= 1)
//   latency_ms  : first step spike - 2200 (quality metric, lower is better)
//   value       : 100 when passed, else 85 * (criteria met / 3)
// MECHANISM (Engine::structure(), checked once before the run): the pulse must reach the cell that is read, and that cell must have no bias
// current of its own (a tonic cell is not a threshold device: it fires whatever the input does). The verdict names the missing part, never a
// number; the rheobase is not printed anywhere (verdict, hints 1-2): only recipe() has it.
// Why it works out: an iaf_psc_alpha cell (the default part) has a rheobase of 375 pA (V_th - E_L = 15 mV across 40 MOhm), so with the dc
// sensor weight W the cell fires on the step iff S W > 375 and stays silent on the blip iff B W < 375: over all seeds about 430 < W < 500 pA.
// The lab starts the sensor at 300 pA (nothing fires).
//
// Biology: Lapicque 1907 introduced rheobase (the weakest steady current that excites) and chronaxie; Hodgkin 1948 J Physiol 107:165
// showed class 1 and class 2 excitability: the rate starts from zero at rheobase (class 1) or jumps (class 2).
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;
using micro::Graph;

constexpr double kBlip0 = 800, kBlip1 = 1400, kStep0 = 2200, kStep1 = 3600, kDur = 4000;

class MThreshold : public MicroBase
{
public:
  MThreshold() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_threshold";
    i.title = "Threshold";
    i.brief = "A neuron does nothing until its input is strong enough (its rheobase, the smallest steady current that makes it fire: "
              "weaker input is simply ignored), and then it fires. Your knob is the weight of the 'pulse' sensor: the current the "
              "cell gets is the weight times the level. 'pulse' gives a small blip (about two thirds of full strength) and later a big step (about full "
              "strength), both a little different on every run of the world: build one neuron that stays silent during the blip but fires during the step, at least 8 spikes in the step "
              "and none at any other time.";
    i.chapter = "invertebrates";
    i.sensors = { "pulse" };
    i.motors = { "spike" };
    i.duration_ms = kDur;
    i.concepts_new = { "threshold" };
    i.par_neurons = 1;
    i.default_model = "iaf_psc_alpha";
    i.recipe = "Wire pulse to one integrate-and-fire cell and the cell to 'spike'. The cell's rheobase is 375 pA, and the stimulus levels "
               "differ a little from seed to seed (the blip 0.63-0.73, the step 0.9-1.1), so set the pulse weight between about 440 and 480 pA: "
               "at the lab's start of 300 even the step stays under the rheobase, and above about 500 a blip crosses it too.";
    i.kinds = { { "pulse", micro::dc( 1.0 ) }, { "spike", micro::rate() } };
    i.bands = { { "step_spikes", "spikes", 8, 200 }, { "blip_spikes", "spikes", 0, 0.5 } };
    i.timeline = { { kBlip0, kBlip1 - kBlip0, "small blip" }, { kStep0, kStep1 - kStep0, "big step" } };
    i.note = { "Rheobase",
      "In 1907 Louis Lapicque measured how much current it takes to excite a nerve with pulses of different lengths. The weakest "
      "current that works however long it lasts he called the rheobase: below it nothing happens, no matter how long you wait. "
      "A neuron is a threshold device, and its rheobase is the first number worth knowing about any cell.",
      "Lapicque L (1907) J Physiol Pathol Gen 9:620-635; English translation and commentary: Brunel N, van Rossum MCW (2007) Biol Cybern 97:337-339" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    gain_ = 1.0 + 0.02 * noise() * rng_.normal();
    const bool gd = mode_ == "guided", rs = mode_ == "researcher";
    blip_ = ( gd ? 0.55 : ( rs ? 0.72 : 0.68 ) ) + ( gd ? 0.03 : 0.05 ) * rng_.uniform( -1, 1 );
    step_ = 1.0 + ( gd ? 0.05 : ( rs ? 0.08 : 0.10 ) ) * rng_.uniform( -1, 1 );
    step_n_ = blip_n_ = off_n_ = 0;
    first_ = -1;
    rate_.tau_ms = 150;
    rate_.reset( 0 );
    level_ = 0;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "A neuron only fires when its input pushes the membrane past threshold. Below the rheobase (the weakest steady current "
                  "that works) nothing happens however long you wait, so a weak blip is ignored and a strong step is not.", "" },
      { 2, "Look at the weight of the 'pulse' sensor: the current the cell receives is that weight times the level of the pulse, and the "
           "blip and the step differ only in level. The stimulus is not exactly the same on every run, so a weight that only just works "
           "on one run may fail on another.", "pulse" },
      { 3, "Wire pulse to one neuron and the neuron to 'spike'. There is a band of weights between 'the step does not reach threshold' and "
           "'the blip reaches it too': raise the weight until the step fires, then lower it until the blip stays quiet, and settle in the "
           "middle of the band, away from both edges. Leave the cell's own bias current alone.", "pulse" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    return { { "latency_ms", { first_ >= 0 ? first_ - kStep0 : 999.0, "ms", true } } };
  }

  void inspect( Engine& ) override
  {
    const auto from = g_.in_pops( "pulse" ), to = g_.out_pops( "spike" );
    if ( !to.empty() && !from.empty() && !g_.path( from, to, 0, 3, true ) )
      mech_ = "The pulse does not reach the cell that is read on 'spike': a threshold is about what an input has to do to a cell, so the pulse must act on it.";
    for ( int p : to )
    {
      const double ie = g_.param( p, "I_e" );
      if ( !std::isnan( ie ) && std::fabs( ie ) > 1e-9 && mech_.empty() )
        mech_ = "The cell has a bias current of its own pushing it: a tonic cell fires whatever the input does, which is no threshold. Leave the bias at rest "
                "and let the pulse alone decide.";
    }
  }

  Score score() const override
  {
    Score s;
    const bool g = mode_ == "guided";
    const double need = g ? 5 : 8, max_other = g ? 1 : 0;
    s.parts[ "step_spikes" ] = step_n_;
    s.parts[ "blip_spikes" ] = blip_n_;
    s.parts[ "off_spikes" ] = off_n_;
    s.parts[ "latency_ms" ] = first_ >= 0 ? first_ - kStep0 : -1.0;
    const bool a = step_n_ >= need, b = blip_n_ <= max_other, c = off_n_ <= max_other;
    finish( s, ( a ? 1 : 0 ) + ( b ? 1 : 0 ) + ( c ? 1 : 0 ), 3 );
    if ( !done() )
      s.passed = false;
    apply_mech( s );
    std::ostringstream o;
    const double total = step_n_ + blip_n_ + off_n_;
    if ( total == 0 )
      s.value = 0;  // a cell that never fired has tried nothing yet: keeping silent through the blip is not partial credit
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done() )
      o << "Running.";
    else if ( !mech_.empty() )
      o << mech_;
    else if ( total == 0 )
      o << "The cell never fired: even the big step stayed below its threshold, so nothing happened. Strengthen the pulse input.";
    else if ( !b && a )
      o << "Fired on the small blip as well: the blip was already over threshold. Weaken the pulse input until only the big step crosses it.";
    else if ( !a )
      o << "The big step made it fire only a little: the input is barely over threshold, and on another run of the world it may not be. "
           "Strengthen the pulse input a little.";
    else if ( !c )
      o << "The cell fired outside the pulses: something other than the pulse is driving it (a bias current, or noise getting through).";
    else
      o << "Clean threshold: silent on the blip, firing on the step, and it holds on this run of the world.";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    level_ = 0;
    if ( t_ >= kBlip0 && t_ < kBlip1 ) level_ = blip_;
    if ( t_ >= kStep0 && t_ < kStep1 ) level_ = step_;
    double v = level_ > 0 ? level_ * gain_ * ( 1.0 + 0.04 * noise() * rng_.normal() ) : 0.0;
    e.set_input( "pulse", v );
  }

  void observe( Engine& e, double dt ) override
  {
    double n = motor_.spike_count( e, "spike", dt );
    if ( n > 0 )
    {
      if ( t_ > kStep0 && t_ <= kStep1 )
      {
        step_n_ += n;
        if ( first_ < 0 ) first_ = t_;
      }
      else if ( t_ > kBlip0 && t_ <= kBlip1 )
        blip_n_ += n;
      else
        off_n_ += n;
    }
    rate_.step( n / motor_.pool_size( e, "spike" ) * 1000.0 / dt, dt );
    readouts_[ "pulse_level" ] = level_;
    readouts_[ "rate_hz" ] = rate_.y;
    readouts_[ "step_spikes" ] = step_n_;
    readouts_[ "blip_spikes" ] = blip_n_;
  }

private:
  double gain_ = 1, blip_ = 0.6, step_ = 1.0, step_n_ = 0, blip_n_ = 0, off_n_ = 0, first_ = -1, level_ = 0;
  Lowpass rate_;
};

} // namespace

std::unique_ptr< Task > make_m_threshold() { return std::make_unique< MThreshold >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_threshold, 7, "m_threshold.json", make_m_threshold ) }
