// Micro-puzzle "m_negative_feedback" (concept 14: negative feedback control). Hold a level against pushes by letting the level itself oppose the drive.
//
// WORLD (3600 ms, plant in plain C++; sensors are dc, multiplied by the sensor weight (pA per unit) in the circuit)
//   Plant: a level y (think joint angle or blood pressure) with  tau dy/dt = u - y - d(t),  tau = 200 ms, u = effort in 0..1.2 = low-passed (tau 60 ms)
//   rate of the motor pool "effort" / 80 Hz; so y settles at u - d. The target is y = 0.5.
//   Push d(t): +0.30 on [b, b + 1000) with b = 1100 + J (J = U(0,100) ms seeded) pulls y down; -0.30 on [b + 1400, b + 2400) pushes it up.
//   Sensor "tone": level 2.0 all episode (600 pA at 300 pA per unit: a motor cell wired to it fires at about 85 spikes/s, effort 1.06, so
//   without feedback y sits near 1.0, far from the target).
//   Sensor "level": the sense organ: level = 3 * y, delayed by 20 ms (+2 % noise per slice, x0 guided, x2 researcher). At the target it is 1.5 (450 pA).
// SCORING (y averaged over windows)
//   baseline [700, b]                   |y - 0.5| <= 0.10 * tol     (tol 1.35 guided, 0.8 researcher)
//   push down  [b + 700, b + 1000]      |y - 0.5| <= 0.10 * tol
//   push up    [b + 2100, b + 2400]     |y - 0.5| <= 0.10 * tol
//   value 100 when passed, else 85 * credit of the three.
// Why it works out: the effort cell has 225 pA of drive above rheobase; every unit of y adds 900 pA of sensor current to an inhibitory cell
// that inhibits the effort cell, so the closed-loop error is the open-loop error / (1 + G) with G about 5 for the reference (3 inhibitory cells at -300 pA).
//
// Biology: Cannon WB (1929) Physiol Rev 9:399-431 (homeostasis as feedback organisation); the stretch reflex and the baroreflex are
// negative feedback loops with a sensor, a comparison and an effector.
#include "micro_b.h"

namespace spike::tasks
{
namespace
{
using namespace microb;

constexpr double kDur = 3600, kTone = 2.0, kTau = 200, kPush = 0.30, kSet = 0.5, kSenseGain = 3.0, kSenseDelay = 20, kRefHz = 80;

class MNegativeFeedback : public MicroB
{
public:
  MNegativeFeedback() : MicroB( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_negative_feedback";
    i.title = "Negative feedback";
    i.brief = "To hold a value steady, a body measures it and pushes against any change: negative feedback. Here a motor pool 'effort' lifts a "
              "level (a joint angle, a blood pressure) that must stay at 0.5 +- 0.1. The 'tone' drive on its own would carry the level to "
              "about 1.0, and at 1.1 s the level is pushed down by 0.3 for a second, later up by 0.3: it has to stay inside the band both "
              "times. The sensor 'level' reports the level (higher level, stronger signal). Wire tone onto the effort cells and read them out on "
              "'effort'. Then use the sensor to oppose the drive: the more level, the less effort. The sign matters: a signal that adds "
              "to the effort when the level is high makes things worse. Route the level through a cell set to inhibitory onto the effort cells. "
              "The loop needs gain: the sensor gives 900 pA per unit of level, so about 3 inhibitory cells and a weight of about -300 pA is "
              "the right size (the default -80 is too weak).";
    i.chapter = "spinal";
    i.sensors = { "tone", "level" };
    i.motors = { "effort" };
    i.duration_ms = kDur;
    i.concepts_new = { "negative_feedback" };
    i.concepts_used = { "inhibition", "excitation" };
    i.par_neurons = 4;
    i.kinds = { { "tone", dc( kTone ) }, { "level", dc( 1.5 ) }, { "effort", rate() } };
    i.bands = { { "level", "", 0.4, 0.6 } };
    i.timeline = { { 1100, 1000, "push down" }, { 2500, 1000, "push up" } };
    i.concept_hint = "Negative feedback: a sensor reports the controlled value and an inhibitory link turns an excess into less drive (and a deficit into more), "
                     "so the value returns to its set point whatever pushes it.";
    i.generic_hints = { { 1, i.concept_hint, "" },
      { 2, "The 'level' sensor has to oppose the drive: it must reach the effort cells through an inhibitory cell.", "level" },
      { 3, "tone onto the effort cells (2 cells); level onto a group of 3 cells set to inhibitory, which projects onto the effort cells at about -300 pA. "
           "If the level still moves too far under a push, add cells or strengthen the link.", "level" } };
    i.note = { "Negative feedback",
      "Walter Cannon coined homeostasis in the 1930s, but already in 1929 he described the organisation that holds the body's variables "
      "steady: sensors that notice a departure and effectors that oppose it. Every loop of this book (stretch reflex, posture, baroreflex) "
      "has this shape: measure, subtract from the set point, push back.",
      "Cannon WB (1929) Organization for physiological homeostasis. Physiol Rev 9:399-431" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    j_ = rng_.uniform( 0, 100 );
    y_ = kSet;
    u_ = 0;
    ytr_.assign( 1, y_ );
    ulp_.tau_ms = 60;
    ulp_.reset( 0 );
  }

  std::map< std::string, Metric > metrics() const override
  {
    std::map< std::string, double > p;
    std::vector< Crit > c;
    std::string m;
    measure( p, c, m );
    return { { "mean_abs_error", { p.count( "mean_abs_error" ) ? p[ "mean_abs_error" ] : 1.0, "", true } } };
  }

protected:
  double push_at( double t ) const
  {
    const double b = 1100 + j_;
    if ( t >= b && t < b + 1000 ) return kPush;
    if ( t >= b + 1400 && t < b + 2400 ) return -kPush;
    return 0;
  }

  void drive( Engine& e, double ) override
  {
    e.set_input( "tone", kTone );
    const double d = kSenseDelay;
    const size_t idx = static_cast< size_t >( std::max( 0.0, t_ - d ) );
    const double yd = ytr_[ std::min( idx, ytr_.size() - 1 ) ];
    e.set_input( "level", std::max( 0.0, kSenseGain * yd * ( 1.0 + 0.02 * noise() * rng_.normal() ) ) );
  }

  void plant( double dt ) override
  {
    u_ = clampd( ulp_.step( hz_now( "effort" ) / kRefHz, dt ), 0, 1.2 );
    y_ += dt / kTau * ( u_ - y_ - push_at( t_ ) );
    // one sample per ms of model time (dt is 1 ms in the game and the tests)
    while ( static_cast< double >( ytr_.size() ) <= t_ + 1e-6 )
      ytr_.push_back( y_ );
  }

  void update_readouts() override
  {
    readouts_[ "level" ] = y_;
    readouts_[ "effort_hz" ] = hz_now( "effort" );
    readouts_[ "push" ] = push_at( t_ );
  }

  double mean_y( double t0, double t1 ) const
  {
    double s = 0;
    int n = 0;
    for ( double t = t0; t < t1 && t < static_cast< double >( ytr_.size() ); t += 1.0, ++n )
      s += ytr_[ static_cast< size_t >( t ) ];
    return n ? s / n : kSet;
  }

  void measure( std::map< std::string, double >& parts, std::vector< Crit >& crits, std::string& pass_msg ) const override
  {
    const double tl = tol(), band = 0.10 * tl;
    const double b = 1100 + j_;
    const double y0 = mean_y( 700, b ), y1 = mean_y( b + 700, b + 1000 ), y2 = mean_y( b + 2100, b + 2400 );
    parts[ "level_baseline" ] = y0;
    parts[ "level_push_down" ] = y1;
    parts[ "level_push_up" ] = y2;
    double err = 0;
    int n = 0;
    for ( double t = 700; t < std::min( kDur, static_cast< double >( ytr_.size() ) ); t += 5.0, ++n )
      err += std::fabs( ytr_[ static_cast< size_t >( t ) ] - kSet );
    parts[ "mean_abs_error" ] = n ? err / n : 1.0;

    Crit c0;
    c0.key = "baseline";
    c0.ok = std::fabs( y0 - kSet ) <= band;
    c0.soft = 1.0 - std::fabs( y0 - kSet ) / 0.5;
    c0.fail = "Before any push the level sits at " + fmt( y0, 2 ) + ", the target is 0.50 +- " + fmt( band, 2 ) +
              ( y0 > kSet ? ": the effort is too high and nothing brings it down." : ": the effort is too low, or 'tone' does not reach the effort cells.") +
              " Feed the level back through an inhibitory cell: more level, less effort.";
    c0.place = "The level sensor should reduce the effort when the level is high: it needs an inhibitory cell between it and the effort cells.";
    c0.target = "level";
    c0.recipe = "tone onto 2 effort cells; level onto 3 inhibitory cells projecting onto them at about -300 pA.";
    crits.push_back( c0 );

    Crit c1;
    c1.key = "push_down";
    c1.ok = std::fabs( y1 - kSet ) <= band;
    c1.soft = 1.0 - std::fabs( y1 - kSet ) / 0.4;
    c1.fail = "While the level is pushed down it stays at " + fmt( y1, 2 ) + " (target 0.50 +- " + fmt( band, 2 ) + "): the loop does not compensate enough. "
              "Strengthen the feedback: a larger weight or more inhibitory cells, so a small drop in level releases a lot of effort.";
    c1.place = "The loop gain: the weight from the inhibitory cells onto the effort cells and how many cells there are.";
    c1.target = "effort";
    c1.recipe = "Use 3 inhibitory cells at -300 pA, or 6 cells at -150 pA.";
    crits.push_back( c1 );

    Crit c2;
    c2.key = "push_up";
    c2.ok = std::fabs( y2 - kSet ) <= band;
    c2.soft = 1.0 - std::fabs( y2 - kSet ) / 0.4;
    c2.fail = "While the level is pushed up it reaches " + fmt( y2, 2 ) + " (target 0.50 +- " + fmt( band, 2 ) + "): feedback works in both directions only if the effort cells "
              "are active enough to be turned down. Check the effort cells still fire when the level is low (tone strong enough) and the feedback is strong.";
    c2.place = "The effort cells: they must be able to speed up (a deficit) and slow down (an excess).";
    c2.target = "effort";
    c2.recipe = "Keep 'tone' at its default weight (300 pA per unit); 3 inhibitory cells at about -300 pA do the rest.";
    crits.push_back( c2 );

    pass_msg = "The level held at " + fmt( y0, 2 ) + " / " + fmt( y1, 2 ) + " / " + fmt( y2, 2 ) + " (target 0.50) before, during the push down and during the push up: "
               "negative feedback.";
  }

private:
  double j_ = 0, y_ = kSet, u_ = 0;
  std::vector< double > ytr_;
  Lowpass ulp_;
};

} // namespace

std::unique_ptr< Task > make_m_negative_feedback() { return std::make_unique< MNegativeFeedback >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_negative_feedback, 119, "m_negative_feedback.json", make_m_negative_feedback ) }
