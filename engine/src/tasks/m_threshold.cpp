// Micro-puzzle "m_threshold" (concept 1: threshold and rheobase). Make one cell fire on a big step of current and ignore a small blip.
//
// WORLD (4000 ms; the stimulus is a dc sensor "pulse", whose level is multiplied by the sensor weight (pA per unit) in the circuit)
//   level 0.0 baseline; a small blip of level B on [800, 1400); a big step of level 1.0 on [2200, 3600); 0 elsewhere.
//   B = 0.6 (standard), 0.5 (guided), 0.72 (researcher). Standard noise: white 4 % per ms slice plus a 2 % per-episode gain
//   (x0 guided, x2 researcher).
//   Motor "spike": spikes of the cell(s) wired to it.
// SCORING
//   step_spikes : spikes in (2200, 3600]      needs >= 8 (guided 5)         the cell fires on the step
//   blip_spikes : spikes in (800, 1400]       needs 0 (guided <= 1)         ... and not on the blip
//   off_spikes  : spikes outside both pulses  needs 0 (guided <= 1)
//   latency_ms  : first step spike - 2200 (quality metric, lower is better)
//   value       : 100 when passed, else 85 * (criteria met / 3)
// Why it works out: an iaf_psc_alpha cell (the default part) has a rheobase of 375 pA (V_th - E_L = 15 mV across 40 MOhm), so with the dc
// sensor weight W the cell fires on the step iff W > 375 and stays silent on the blip iff 0.6 W < 375, i.e. 375 < W < 625 pA. The lab
// starts the sensor at 300 pA (nothing fires).
//
// Biology: Lapicque 1907 introduced rheobase (the weakest steady current that excites) and chronaxie; Hodgkin 1948 J Physiol 107:165
// showed class 1 and class 2 excitability: the rate starts from zero at rheobase (class 1) or jumps (class 2).
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;

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
    i.brief = "A neuron does nothing until its input is strong enough, and then it fires. Here a current source called 'pulse' gives a "
              "small blip (60 % of full strength) and later a big step (full strength). Build one neuron that stays silent during the "
              "blip but fires during the step: at least 8 spikes in the step, none at any other time. The strength reaching the cell is "
              "the sensor's weight times the level, so the weight is your knob. Every cell has a smallest steady current that makes it "
              "fire, its rheobase: weaker input than that is simply ignored.";
    i.chapter = "invertebrates";
    i.sensors = { "pulse" };
    i.motors = { "spike" };
    i.duration_ms = kDur;
    i.concepts_new = { "threshold" };
    i.par_neurons = 1;
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
    blip_ = mode_ == "guided" ? 0.5 : ( mode_ == "researcher" ? 0.72 : 0.6 );
    step_n_ = blip_n_ = off_n_ = 0;
    first_ = -1;
    rate_.tau_ms = 150;
    rate_.reset( 0 );
    level_ = 0;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "A neuron only fires when its input pushes the membrane past threshold. For this cell a steady current has to exceed its "
                  "rheobase (about 375 pA); below that, nothing happens however long it lasts.", "" },
      { 2, "Look at the weight of the 'pulse' sensor: the current the cell receives is that weight times the pulse level (0.6 for the blip, "
           "1.0 for the step).", "pulse" },
      { 3, "Wire pulse to one neuron and the neuron to 'spike'. Set the pulse weight between about 400 and 600 pA: at the start's 300 pA "
           "even the step is under the rheobase, and above 625 pA the 0.6 blip is over it too.", "pulse" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    return { { "latency_ms", { first_ >= 0 ? first_ - kStep0 : 999.0, "ms", true } } };
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
    std::ostringstream o;
    const double total = step_n_ + blip_n_ + off_n_;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done() )
      o << "Running: " << static_cast< int >( total ) << " spikes so far.";
    else if ( total == 0 )
      o << "The cell never fired. The step current stayed below its rheobase (about 375 pA for this cell), so nothing happened. "
           "Raise the weight of the pulse sensor.";
    else if ( !b && a )
      o << "Fired on the small blip as well (" << static_cast< int >( blip_n_ ) << " spikes): the blip was already above threshold. "
           "Lower the pulse weight until only the big step crosses it.";
    else if ( !a )
      o << "Only " << static_cast< int >( step_n_ ) << " spikes in the step (needs " << static_cast< int >( need )
        << "). The input is barely above threshold: raise the pulse weight a little.";
    else if ( !c )
      o << "The cell fired outside the pulses (" << static_cast< int >( off_n_ ) << " spikes). Check for a bias current or noise input.";
    else
      o << "Clean threshold: silent on the blip, " << static_cast< int >( step_n_ ) << " spikes on the step, first one after "
        << fmt( first_ - kStep0 ) << " ms.";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    level_ = 0;
    if ( t_ >= kBlip0 && t_ < kBlip1 ) level_ = blip_;
    if ( t_ >= kStep0 && t_ < kStep1 ) level_ = 1.0;
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
  double gain_ = 1, blip_ = 0.6, step_n_ = 0, blip_n_ = 0, off_n_ = 0, first_ = -1, level_ = 0;
  Lowpass rate_;
};

} // namespace

std::unique_ptr< Task > make_m_threshold() { return std::make_unique< MThreshold >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_threshold, 7, "m_threshold.json", make_m_threshold ) }
