// Micro-puzzle "m_feedforward" (concept 15: feed-forward plus feedback). Anticipate a disturbance you were warned about, and correct the one you were not.
//
// WORLD (4900 ms, plant in plain C++; sensors are dc, multiplied by the sensor weight (pA per unit) in the circuit)
//   Plant: a level y that must stay at 0.5 (heart rate, blood pressure), tau_p dy/dt = u - y - d(t), tau_p = 250 ms, where the effort u in 0..1.2 is the
//   motor pool "effort" rate / 80 Hz low-passed with tau_e = 200 ms (an effector that is slow, like a heart or a vessel).
//   Disturbances d: a "workload" of 0.50 on [b + 250, b + 1300) with b = 1100 + J (J = U(0,100) ms seeded), warned by the sensor "cue" that is on
//   from b to b + 1300 (the cue leads the workload by 250 ms), and a second workload of 0.50 on [3500 + J, 4700 + J) with no cue.
//   Sensor "tone": level 2.0 all episode (600 pA at 300 pA per unit: effort 1.06 with no feedback).
//   Sensor "level": level = 3 * y, delayed by 60 ms (+2 % noise per slice, x0 guided, x2 researcher).
//   Sensor "cue": level 0.4 while the cue is on (120 pA at 300 pA per unit).
// SCORING
//   baseline  [800, b]            |y - 0.5| <= band                  (band 0.10; guided 0.112, researcher 0.093; tol is 1.35 guided, 0.8 researcher)
//   warned    peak |y - 0.5| over [b, b + 1300] <= 0.23 * tol       the dip is caught early: feedback alone is too late (its peak is about 0.30)
//   warned end  mean y over [b + 1000, b + 1300]   within the band   and the remaining error is corrected
//   unwarned end mean y over [4200 + J, 4700 + J] within the band   with no cue only feedback can correct it
//   value 100 when passed, else 85 * credit of the four.
// Why it works out: the effector and the sense organ lag, so pure feedback lets y fall by about 0.30 before it reacts; pure feed-forward
// (cue onto the effort cells) cannot know the size of the second, unwarned workload. Cue plus feedback does both.
//
// Biology: central command in exercise raises heart rate and ventilation before and in proportion to the work, with feedback trimming the
// error (Krogh A, Lindhard J (1913) J Physiol 47:112-136).
#include "micro_b.h"

namespace spike::tasks
{
namespace
{
using namespace microb;

constexpr double kDur = 4900, kCue = 0.4, kSet = 0.5, kRefHz = 80;
constexpr double kTone = 2.0, kWork = 0.50, kLead = 250, kTauP = 250, kTauE = 200, kSenseDelay = 60, kSenseGain = 3.0;

class MFeedforward : public MicroB
{
public:
  MFeedforward() : MicroB( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_feedforward";
    i.title = "Feed-forward and feedback";
    i.brief = "Feedback alone is always a little late: it reacts after the error has appeared. A body that is warned (a light flashes before "
              "the exercise starts) can act first: feed-forward. Here a level (heart rate, say) has to stay at 0.5 +- 0.1 while a workload "
              "pulls it down by 0.5. The effector is slow, so plain feedback lets the level sag by about 0.3. The first workload is announced by "
              "the sensor 'cue', a quarter of a second ahead; the second, at 3.5 s, is not. Keep the level within 0.23 of the target during the "
              "announced workload and inside the band at the end of both. The effort cells are read on 'effort'. "
              "You have two sensors that carry information, 'level' (the error, late) and 'cue' (the warning, early).";
    i.chapter = "heart";
    i.sensors = { "tone", "level", "cue" };
    i.motors = { "effort" };
    i.duration_ms = kDur;
    i.concepts_new = { "feedforward_feedback" };
    i.concepts_used = { "negative_feedback", "excitation" };
    i.par_neurons = 5;   // the reference circuit's neuron count (par rule: tests/tasks_rules_test.cpp)
    i.default_model = "iaf_psc_alpha";
    i.recipe = "tone onto 2 effort cells. level onto 3 inhibitory cells that project onto the effort cells at about -300 pA (the "
               "sensor gives 900 pA per unit of level). cue onto the effort cells (the first connection's weight of 300 pA per "
               "unit is right, 150 is too little).";
    i.kinds = { { "tone", dc( kTone ) }, { "level", dc( 1.5 ) }, { "cue", dc( kCue ) }, { "effort", rate() } };
    i.bands = { { "level", "", 0.4, 0.6 } };
    i.timeline = { { 1100, 1300, "cue (warning)" }, { 1350, 1050, "workload (warned)" }, { 3500, 1200, "workload (no cue)" } };
    i.concept_hint = "Feed-forward acts on a warning, before the error exists; feedback corrects the error that remains and the disturbances nobody "
                     "announced. A controller with both is fast and accurate.";
    i.generic_hints = { { 1, i.concept_hint, "" },
      { 2, "You need two routes into the effort cells: 'level' through an inhibitory cell (feedback) and 'cue' directly (feed-forward).", "cue" },
      { 3, "Tone goes onto the effort cells. Level goes onto a group of inhibitory cells that projects onto the effort cells (feedback). Cue goes onto the "
           "effort cells directly (feed-forward); the first connection's weight is about right.", "cue" } };
    i.note = { "Central command",
      "In 1913 August Krogh and Johannes Lindhard showed that breathing and heart rate rise within the first seconds of exercise, "
      "too fast for blood chemistry to explain: the motor command itself drives the cardiovascular and respiratory centres "
      "(feed-forward), and the reflexes from muscles, baroreceptors and chemoreceptors trim the result (feedback).",
      "Krogh A, Lindhard J (1913) The regulation of respiration and circulation during the initial stages of muscular work. J Physiol 47:112-136" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    j_ = rng_.uniform( 0, 100 );
    y_ = kSet;
    ytr_.assign( 1, y_ );
    ulp_.tau_ms = kTauE;
    ulp_.reset( 0 );
  }

  std::map< std::string, Metric > metrics() const override
  {
    std::map< std::string, double > p;
    std::vector< Crit > c;
    std::string m;
    measure( p, c, m );
    return { { "warned_dip", { p.count( "warned_peak_dev" ) ? p[ "warned_peak_dev" ] : 1.0, "", true } } };
  }

protected:
  void mechanism( const mechb::Graph& g, std::vector< Crit >& out ) const override
  {
    // both halves of the controller: the cue reaches the effort cells (feed-forward) AND the level reaches them with the opposite sign (feedback)
    const auto cue = g.in_pops( "cue" ), lvl = g.in_pops( "level" ), eff = g.out_pops( "effort" );
    out.push_back( mech( "feedback_path", g.hops( lvl, eff, -1, 4 ) >= 1,
                         "No feedback path: the level sensor does not reach the effort cells through an inhibitory cell, so a workload nobody announced cannot be corrected.",
                         "The feedback path: 'level' needs an inhibitory cell between it and the effort cells.", "level",
                         "Route 'level' through a group set to inhibitory onto the effort cells." ) );
    out.push_back( mech( "cue_path", g.exc( cue, eff, 4 ),
                         "No feed-forward path: the cue does not reach the effort cells, so the warning is not used and the effort cannot rise before the workload.",
                         "The warning: 'cue' has to excite the effort cells.", "cue",
                         "Connect 'cue' to the effort cells (directly, with an excitatory link)." ) );
  }

  double b() const { return 1100 + j_; }
  bool cue_at( double t ) const { return t >= b() && t < b() + 1300; }
  double work_at( double t ) const
  {
    if ( t >= b() + kLead && t < b() + 1300 ) return kWork;
    if ( t >= 3500 + j_ && t < 4700 + j_ ) return kWork;
    return 0;
  }

  void drive( Engine& e, double ) override
  {
    e.set_input( "tone", kTone );
    e.set_input( "cue", cue_at( t_ ) ? kCue : 0.0 );
    const size_t idx = static_cast< size_t >( std::max( 0.0, t_ - kSenseDelay ) );
    const double yd = ytr_[ std::min( idx, ytr_.size() - 1 ) ];
    e.set_input( "level", std::max( 0.0, kSenseGain * yd * ( 1.0 + 0.02 * noise() * rng_.normal() ) ) );
  }

  void plant( double dt ) override
  {
    const double u = clampd( ulp_.step( hz_now( "effort" ) / kRefHz, dt ), 0, 1.2 );
    y_ += dt / kTauP * ( u - y_ - work_at( t_ ) );
    while ( static_cast< double >( ytr_.size() ) <= t_ + 1e-6 )
      ytr_.push_back( y_ );
    u_ = u;
  }

  void update_readouts() override
  {
    readouts_[ "level" ] = y_;
    readouts_[ "effort_hz" ] = hz_now( "effort" );
    readouts_[ "workload" ] = work_at( t_ );
    readouts_[ "cue" ] = cue_at( t_ ) ? 1.0 : 0.0;
  }

  double mean_y( double t0, double t1 ) const
  {
    double s = 0;
    int n = 0;
    for ( double t = t0; t < t1 && t < static_cast< double >( ytr_.size() ); t += 1.0, ++n )
      s += ytr_[ static_cast< size_t >( t ) ];
    return n ? s / n : kSet;
  }
  double peak_dev( double t0, double t1 ) const
  {
    double m = 0;
    for ( double t = t0; t < t1 && t < static_cast< double >( ytr_.size() ); t += 1.0 )
      m = std::max( m, std::fabs( ytr_[ static_cast< size_t >( t ) ] - kSet ) );
    return m;
  }

  void measure( std::map< std::string, double >& parts, std::vector< Crit >& crits, std::string& pass_msg ) const override
  {
    const double tl = tol(), band = 0.10 * ( 1.0 + 0.35 * ( tl - 1.0 ) ), peak_lim = 0.23 * tl;
    const double y0 = mean_y( 800, b() ), pk = peak_dev( b(), b() + 1300 ), y1 = mean_y( b() + 1000, b() + 1300 ), y2 = mean_y( 4200 + j_, 4700 + j_ );
    parts[ "level_baseline" ] = y0;
    parts[ "warned_peak_dev" ] = pk;
    parts[ "level_warned_end" ] = y1;
    parts[ "level_unwarned_end" ] = y2;

    Crit c0;
    c0.key = "baseline";
    c0.ok = std::fabs( y0 - kSet ) <= band;
    c0.soft = 1.0 - std::fabs( y0 - kSet ) / 0.5;
    c0.fail = std::string( "Before the cue the level sits " ) + ( y0 > kSet ? "above" : "below" ) + " the target band: the base effort is wrong. Feed the level back through "
              "an inhibitory cell onto the effort cells so that it settles at the target by itself.";
    c0.place = "The feedback path: 'level' needs an inhibitory cell between it and the effort cells.";
    c0.target = "level";
    c0.recipe = "level onto a group of inhibitory cells projecting onto the effort cells, strong enough to settle the level; tone onto the effort cells.";
    crits.push_back( c0 );

    Crit c1;
    c1.key = "warned_dip";
    c1.ok = pk <= peak_lim;
    c1.soft = 1.0 - ramp( pk, peak_lim, 0.6 );
    c1.fail = "The announced workload still pulls the level too far from its target: feedback alone reacts too late. "
              "Use the cue: wire 'cue' to the effort cells so that effort rises before the workload arrives (and not so strongly that it overshoots).";
    c1.place = "The warning: 'cue' comes a moment before the workload, but it is not yet used strongly enough.";
    c1.target = "cue";
    c1.recipe = "Connect 'cue' to the effort cells; the first connection's weight is about right, and a clearly weaker one leaves the dip.";
    crits.push_back( c1 );

    Crit c2;
    c2.key = "warned_end";
    c2.ok = std::fabs( y1 - kSet ) <= band;
    c2.soft = 1.0 - std::fabs( y1 - kSet ) / 0.4;
    c2.fail = std::string( "At the end of the announced workload the level is " ) + ( y1 > kSet ? "above" : "below" ) + " the target band: the cue's push does not match the workload, "
              "and the feedback is too weak to trim the difference.";
    c2.place = "Feedback has to trim what the anticipation gets wrong: check the inhibitory link from 'level' is strong enough.";
    c2.target = "level";
    c2.recipe = "Make the inhibitory level feedback stronger (weight or number of cells).";
    crits.push_back( c2 );

    Crit c3;
    c3.key = "unwarned_end";
    c3.ok = std::fabs( y2 - kSet ) <= band;
    c3.soft = 1.0 - std::fabs( y2 - kSet ) / 0.4;
    c3.fail = std::string( "With no cue the second workload leaves the level " ) + ( y2 > kSet ? "above" : "below" ) + " the target band: nothing warned you, so only feedback can "
              "correct it. Wire 'level' back onto the effort cells through an inhibitory cell and make it strong enough.";
    c3.place = "The feedback loop: it is the only defence against a surprise.";
    c3.target = "level";
    c3.recipe = "level onto a group of inhibitory cells projecting onto the effort cells, with a strong enough weight.";
    crits.push_back( c3 );

    pass_msg = "A warned workload was met before it arrived and an unwarned one was corrected afterwards: feed-forward caught the first, feedback the second.";
  }

private:
  double j_ = 0, y_ = kSet, u_ = 0;
  std::vector< double > ytr_;
  Lowpass ulp_;
};

} // namespace

std::unique_ptr< Task > make_m_feedforward() { return std::make_unique< MFeedforward >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_feedforward, 207, "m_feedforward.json", make_m_feedforward ) }
