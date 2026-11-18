// Micro-puzzle "m_recruitment" (concept 11: recruitment, the size principle). Grade a muscle's force by recruiting motor units in order of size.
//
// WORLD (4000 ms; the command is a dc sensor "command", multiplied by the sensor weight (pA per unit) in the circuit)
//   Effort steps of 900 ms from t = 500 + J (J = U(0, 100) ms seeded): light (level 1.2), medium (2.2), maximal (3.2); 0 elsewhere.
//   Standard noise: 2 % white per ms slice on the level (x0 guided, x2 researcher).
//   Three motor pools are the muscle's motor units, smallest first: "mu_small", "mu_mid", "mu_large". Each pool's force is
//        F_i = cap_i * clamp(rate_i / 20 Hz, 0, 1),   cap = 0.10, 0.30, 0.60   (small units are weak, big ones strong; tetanic fusion
//   saturates a unit at about 20 Hz), the muscle's force is the sum. The force the task wants is the staircase of the size principle:
//   0.10 at light effort (only the small units), 0.40 at medium, 1.00 at maximal; 0 at rest.
// SCORING (windows are the last 600 ms of each step, rest windows 100-400 ms after t=0 and the last 600 ms of the episode)
//   rest   : force <= 0.04
//   level k: |force - target_k| <= 0.09 * tol      (tol 1.35 guided, 0.8 researcher)
//   value  : 100 when passed, else 85 * (credit of the four criteria); pass: all four.
// Why it works out: with the dc weight W an iaf_psc_alpha cell has net drive W * level + I_e against its 375 pA rheobase, so the three levels
// (360, 660, 960 pA at W = 300) cross the three thresholds in order only if the pools have different thresholds: a bias I_e of about
// +100, 0 and -350 pA (smaller cells have the higher input resistance, so the same input depolarises them more: Henneman).
//
// Biology: Henneman E, Somjen G, Carpenter DO (1965) J Neurophysiol 28:560-580: the threshold of a motoneuron to a synaptic input is inversely
// related to its size, so motor units are recruited from small and weak to large and strong in a fixed order.
#include "micro_b.h"

namespace spike::tasks
{
namespace
{
using namespace microb;

constexpr double kDur = 4000, kStepLen = 900, kT0 = 500;
constexpr double kLevel[ 3 ] = { 1.2, 2.2, 3.2 };
constexpr double kCap[ 3 ] = { 0.10, 0.30, 0.60 }, kSat = 20.0;
constexpr double kTarget[ 4 ] = { 0.0, 0.10, 0.40, 1.00 };
const char* const kPool[ 3 ] = { "mu_small", "mu_mid", "mu_large" };

class MRecruitment : public MicroB
{
public:
  MRecruitment() : MicroB( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_recruitment";
    i.title = "Recruitment";
    i.brief = "A muscle is made of motor units. A small motoneuron drives a few weak fibres and needs only a little input to fire; a big "
              "one drives many strong fibres and needs more. That is how the nervous system grades force: as the command grows, "
              "motoneurons are recruited from the small to the large (the size principle). Here a command 'command' steps through three "
              "efforts (light, medium, maximal) and the muscle has three motor pools, mu_small (force 0.10), mu_mid (0.30) and "
              "mu_large (0.60 at full rate). The force must follow the staircase 0.10 / 0.40 / 1.00 (within 0.09) and be zero at rest. "
              "Wire the command to a group of cells for each pool and read each group out on its channel. With identical cells every "
              "pool switches on at the same effort, so the force jumps from 0 to 1.00: give the three pools different thresholds. A "
              "bias current (ADVANCED, I_e) that lowers or raises each pool's threshold sorts the pools out.";
    i.chapter = "spinal";
    i.sensors = { "command" };
    i.motors = { "mu_small", "mu_mid", "mu_large" };
    i.duration_ms = kDur;
    i.concepts_new = { "recruitment" };
    i.concepts_used = { "threshold", "excitation" };
    i.par_neurons = 3;
    i.default_model = "iaf_psc_alpha";
    i.recipe = "A cell's rheobase is 375 pA and the command delivers 360, 660 and 960 pA (weight 300 pA per unit times the level "
               "1.2 / 2.2 / 3.2). Three groups, each driven by 'command' and read out on its own channel. Give the small pool a "
               "bias of about +100 pA, the mid pool none, the large pool about -350 pA: then 360 pA recruits only the small "
               "pool, 660 pA the mid pool too, 960 pA all three.";
    i.kinds = { { "command", dc( 2.2 ) }, { "mu_small", rate() }, { "mu_mid", rate() }, { "mu_large", rate() } };
    i.bands = { { "force_error", "", -0.09, 0.09 } };
    i.timeline = { { kT0, kStepLen, "light effort" }, { kT0 + kStepLen, kStepLen, "medium effort" }, { kT0 + 2 * kStepLen, kStepLen, "maximal effort" } };
    i.concept_hint = "Force is graded by recruitment: small motoneurons have low thresholds and weak force, big ones have high thresholds and strong "
                     "force, so a growing command switches the units on one size after the other. Identical cells all switch on together.";
    i.generic_hints = { { 1, i.concept_hint, "" },
      { 2, "The three pools must have different thresholds. Look at the ADVANCED bias current of each group.", "mu_large" },
      { 3, "Three groups, each driven by 'command' and read out on its own channel. Give the small pool a positive bias, leave the mid pool alone and give the "
           "large pool a negative one: a growing command then crosses the thresholds in order, small pool first.",
        "command" } };
    i.note = { "The size principle",
      "In 1965 Elwood Henneman recorded from single motoneurons while a weak or a strong stimulus drove them. The smallest cells fired first and "
      "the largest only for strong stimuli, always in the same order. Small cells have a high input resistance, so the same synaptic current "
      "depolarises them more. Because small units are weak and slow to fatigue, fine control and posture come first and raw force last.",
      "Henneman E, Somjen G, Carpenter DO (1965) Functional significance of cell size in spinal motoneurons. J Neurophysiol 28:560-580" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    j_ = rng_.uniform( 0, 100 );
    force_ = 0;
    level_ = 0;
  }

  std::map< std::string, Metric > metrics() const override
  {
    std::map< std::string, double > p;
    std::vector< Crit > c;
    std::string m;
    measure( p, c, m );
    return { { "force_error", { p.count( "mean_abs_err" ) ? p[ "mean_abs_err" ] : 1.0, "", true } } };
  }

protected:
  void mechanism( const mechb::Graph& g, std::vector< Crit >& out ) const override
  {
    // at least two motor-unit pools, each driven by the command, with thresholds that differ (otherwise they are recruited together)
    std::vector< mechb::PopSet > pools;
    for ( int i = 0; i < 3; ++i )
      pools.push_back( g.out_pops( kPool[ i ] ) );
    const auto cmd = g.in_pops( "command" );
    int driven = 0;
    for ( const auto& p : pools )
      if ( !p.empty() && g.exc( cmd, p, 4 ) )
        ++driven;
    // thresholds: the pools' firing-threshold fingerprints (cell parameters and the weights of what feeds them) must differ pairwise
    bool distinct = true;
    for ( int i = 0; i < 3; ++i )
      for ( int j = i + 1; j < 3; ++j )
      {
        if ( pools[ i ].empty() || pools[ j ].empty() )
          continue;
        const bool same_pop = pools[ i ] == pools[ j ];
        bool same_sig = true;
        if ( !same_pop )
        {
          std::string a, b;
          for ( int p : pools[ i ] ) a += g.threshold_signature( p ) + ";";
          for ( int p : pools[ j ] ) b += g.threshold_signature( p ) + ";";
          same_sig = a == b;
        }
        if ( same_pop || same_sig )
          distinct = false;
      }
    out.push_back( mech( "pools_driven", driven >= 2, "Recruitment needs several motor-unit pools driven by the same command: fewer than two of the three pools are reached by 'command'.",
                         "The command must reach every motor-unit pool, each group read out on its own channel.", "command",
                         "Make a separate group for each pool, drive each from 'command' and read each out on its own channel." ) );
    out.push_back( mech( "thresholds_differ", distinct,
                         "The motor-unit pools are identical, so they switch on together: recruitment needs pools of different threshold, the small ones lowest.",
                         "Compare the pools' thresholds: the ADVANCED bias current (or any cell property) of each group should differ.", "mu_large",
                         "Give the three groups different thresholds: the small pool the lowest, the large pool the highest." ) );
  }

  int stage( double t ) const
  {
    for ( int k = 0; k < 3; ++k )
      if ( t >= kT0 + j_ + k * kStepLen && t < kT0 + j_ + ( k + 1 ) * kStepLen )
        return k + 1;
    return 0;
  }

  void drive( Engine& e, double ) override
  {
    const int s = stage( t_ );
    level_ = s ? kLevel[ s - 1 ] : 0.0;
    e.set_input( "command", level_ > 0 ? level_ * ( 1.0 + 0.02 * noise() * rng_.normal() ) : 0.0 );
  }

  void update_readouts() override
  {
    force_ = 0;
    for ( int i = 0; i < 3; ++i )
      force_ += kCap[ i ] * clampd( hz_now( kPool[ i ] ) / kSat, 0, 1 );
    const double target = kTarget[ stage( t_ ) ];
    readouts_[ "force" ] = force_;
    readouts_[ "target_force" ] = target;
    readouts_[ "force_error" ] = force_ - target;
    readouts_[ "command" ] = level_;
    for ( int i = 0; i < 3; ++i )
      readouts_[ std::string( kPool[ i ] ) + "_hz" ] = hz_now( kPool[ i ] );
  }

  double force_of( double t0, double t1, double* r = nullptr ) const
  {
    double f = 0;
    for ( int i = 0; i < 3; ++i )
    {
      const double h = hz( kPool[ i ], t0, t1 );
      if ( r ) r[ i ] = h;
      f += kCap[ i ] * clampd( h / kSat, 0, 1 );
    }
    return f;
  }

  void measure( std::map< std::string, double >& parts, std::vector< Crit >& crits, std::string& pass_msg ) const override
  {
    const double tl = tol();
    const double b = kT0 + j_;
    double rest_r[ 3 ], r[ 3 ][ 3 ], f[ 4 ];
    const double rest_a = std::min( 400.0, b - 100 );
    f[ 0 ] = std::max( force_of( 100, rest_a, rest_r ), force_of( b + 3 * kStepLen + 300, kDur, nullptr ) );
    for ( int k = 0; k < 3; ++k )
    {
      const double a = b + k * kStepLen;
      f[ k + 1 ] = force_of( a + 300, a + kStepLen, r[ k ] );
      for ( int i = 0; i < 3; ++i )
        parts[ std::string( kPool[ i ] ) + "_hz_effort" + std::to_string( k + 1 ) ] = r[ k ][ i ];
      parts[ "force_effort" + std::to_string( k + 1 ) ] = f[ k + 1 ];
    }
    parts[ "force_rest" ] = f[ 0 ];
    double err = 0;
    for ( int k = 1; k <= 3; ++k )
      err += std::fabs( f[ k ] - kTarget[ k ] ) / 3.0;
    parts[ "mean_abs_err" ] = err;

    Crit rest;
    rest.key = "rest";
    rest.ok = f[ 0 ] <= 0.04 * tl;
    rest.soft = 1.0 - f[ 0 ] / 0.5;
    rest.fail = "The muscle is active at rest: some pool fires with no command. Check the biases of the cells; a pool must stay silent until the command reaches its threshold.";
    int loud = 0;
    for ( int i = 1; i < 3; ++i )
      if ( rest_r[ i ] > rest_r[ loud ] ) loud = i;
    rest.target = kPool[ loud ];
    rest.place = "A pool that fires at rest has a bias current so high that the cell fires by itself.";
    rest.recipe = "Lower the biases until every cell is silent until the command arrives; only a bias that is too high makes a cell fire alone.";
    crits.push_back( rest );

    static const char* const kEffort[ 3 ] = { "light", "medium", "maximal" };
    for ( int k = 1; k <= 3; ++k )
    {
      Crit c;
      c.key = std::string( "effort" ) + std::to_string( k );
      const double e = f[ k ] - kTarget[ k ];
      c.ok = std::fabs( e ) <= 0.09 * tl;
      c.soft = 1.0 - std::fabs( e ) / 0.4;
      // which pool is wrong: on when it should be off, or off when it should be on (a pool is "on" above 8 Hz)
      std::string who;
      std::string where = "mu_mid";
      for ( int i = 0; i < 3; ++i )
      {
        const bool should_on = i < k;
        const bool on = r[ k - 1 ][ i ] >= 8.0;
        if ( on != should_on && who.empty() )
        {
          who = std::string( kPool[ i ] ) + ( on ? " fired" : " stayed silent" ) + " at " + kEffort[ k - 1 ] + " effort";
          where = kPool[ i ];
        }
      }
      c.target = where;
      c.fail = "At " + std::string( kEffort[ k - 1 ] ) + " effort the force is " + ( e > 0 ? "above" : "below" ) + " the staircase step" +
               ( who.empty() ? "." : ": " + who + "." ) +
               ( e > 0 ? " A pool was recruited too early: raise its threshold (lower its bias) so it needs more command."
                       : " A pool was not recruited yet: lower its threshold (raise its bias) or check it is wired to 'command'.");
      c.place = "Compare the pools' thresholds: the pool '" + where + "' switches on at the wrong effort. Its threshold is set by the bias current of its cells.";
      c.recipe = "Thresholds in order: the small pool needs the lowest threshold (positive bias), the large pool the highest (negative bias), the mid pool in between. "
                 "Every pool is wired from 'command' and read out on its own channel.";
      crits.push_back( c );
    }
    pass_msg = "A clean staircase: force " + fmt( f[ 1 ], 2 ) + " / " + fmt( f[ 2 ], 2 ) + " / " + fmt( f[ 3 ], 2 ) +
               " as the command grew, small units first (mean error " + fmt( err, 3 ) + ").";
  }

private:
  double j_ = 0, force_ = 0, level_ = 0;
};

} // namespace

std::unique_ptr< Task > make_m_recruitment() { return std::make_unique< MRecruitment >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_recruitment, 98, "m_recruitment.json", make_m_recruitment ) }
