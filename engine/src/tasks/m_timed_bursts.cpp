// Micro-puzzle "m_timed_bursts" (concept 16: timed bursts, a pattern generator). One brief 'go' must release three bursts in a fixed order and rhythm.
//
// WORLD (3400 ms; the sensor "go" is dc, multiplied by the sensor weight (pA per unit) in the circuit)
//   Two trials. Trial k starts at g_k = 600 + J (k = 0) or 2000 + J (k = 1), J = U(0, 100) ms seeded; "go" is level 3.0 for 50 ms from g_k (900 pA at
//   300 pA per unit), 0 elsewhere. Motor pools "b1", "b2", "b3" are three muscle commands (agonist, antagonist, agonist: the triphasic pattern of a ballistic
//   movement).
//   A burst of a pool is the moment a sliding 20 ms window holds >= max(2, ceil(0.35 N)) spikes (N = cells in the pool); one burst per episode
//   of activity (a gap of 60 ms of quiet ends it).
// SCORING (per trial, window [g - 300, g + 900])
//   quiet     : the pools fire <= 2 spikes in all in [150, 500] (no spontaneous activity before the first go)
//   all three : each pool has a burst in each trial, none missing
//   single    : each pool has exactly one burst in each trial (no repeats, no persistent firing)
//   order     : start(b1) < start(b2) < start(b3) in both trials, start(b1) - g in [0, 120] ms
//   rhythm    : start(b2) - start(b1) and start(b3) - start(b2) in [100, 220] ms (guided [75, 295], researcher [115, 190]) in both trials
//   value 100 when passed, else 85 * credit of the five.
// Why it works out: a pool is a relay that fires while it is driven; a delay of about 120 ms on the link (the lab allows up to 250 ms) turns "b1 fired"
// into "b2 fires 120 ms later", and so on. Each burst only lasts as long as its input.
//
// Biology: ballistic limb movements show a triphasic EMG: a burst in the agonist, then one in the antagonist, then a second in the agonist
// (Hallett M, Shahani BT, Young RR (1975) J Neurol Neurosurg Psychiatry 38:1154-1162; Wachholder K, Altenburger H 1926 Pflugers Arch 214:642), with 100-200 ms between bursts.
#include "micro_b.h"

namespace spike::tasks
{
namespace
{
using namespace microb;

constexpr double kDur = 3400, kGo = 3.0, kGoLen = 50;
const char* const kPool[ 3 ] = { "b1", "b2", "b3" };

class MTimedBursts : public MicroB
{
public:
  MTimedBursts() : MicroB( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_timed_bursts";
    i.title = "Timed bursts";
    i.brief = "A fast reach is not steered while it happens: the brain fires a stereotyped sequence of bursts, one to start the movement, one to brake it, "
              "one to hold it, 100-200 ms apart, and the arm simply obeys. A pattern generator needs nothing but a trigger and delays. Here a brief "
              "'go' (50 ms) must release three bursts in order on 'b1', 'b2', 'b3', each 100-220 ms after the previous one, and then silence. It happens "
              "twice, at about 0.6 s and 2 s; nothing may fire on its own before. Build a chain: go onto a first group read out on b1; b1's group onto a "
              "second group read out on b2; that onto a third. The links must be strong enough to make the next group fire (the default weight "
              "is not) and slow: a delay of the order of 100 ms on each link (the delay slider reaches 250 ms). Cells stop firing when "
              "their input stops, so each burst ends by itself.";
    i.chapter = "spinal";
    i.sensors = { "go" };
    i.motors = { "b1", "b2", "b3" };
    i.duration_ms = kDur;
    i.concepts_new = { "timed_bursts" };
    i.concepts_used = { "excitation", "threshold" };
    i.par_neurons = 12;
    i.default_model = "iaf_psc_alpha";
    i.recipe = "go onto group 1 (4 cells, read out on b1). Group 1 onto group 2 (4 cells, b2) at about 300 pA with delay 120 ms; "
               "group 2 onto group 3 (4 cells, b3) the same. The weight needed scales inversely with the number of cells in the "
               "group before it: about 1200 pA in all, so 300 pA from 4 cells, 600 from 2, 1000 or more from a single cell.";
    i.kinds = { { "go", dc( kGo ) }, { "b1", rate() }, { "b2", rate() }, { "b3", rate() } };
    i.bands = {};
    i.timeline = { { 600, kGoLen, "go" }, { 2000, kGoLen, "go" } };
    i.concept_hint = "A pattern generator: a trigger plus delays between groups of cells gives a fixed sequence of bursts. A synaptic delay turns "
                     "'the first group fired' into 'the second fires a moment later'.";
    i.generic_hints = { { 1, i.concept_hint, "" },
      { 2, "Look at the links between the groups: each must be strong enough to fire the next group and carry a delay about as long as the pause you want between bursts.", "b2" },
      { 3, "go goes onto a first group read out on b1. The first group projects onto a second group (b2), and the second onto a third (b3). Make each link strong "
           "enough to fire the next group and give each a long delay, as long as the pause between bursts should be.", "b2" } };
    i.note = { "The triphasic pattern",
      "When you flick your forearm quickly, the biceps fires a burst, then the triceps brakes it, then the biceps fires again to hold "
      "the arm. The three bursts are 100-200 ms apart and come out the same each time, which shows that the sequence is generated centrally "
      "rather than steered by feedback (the arm is too fast for that).",
      "Hallett M, Shahani BT, Young RR (1975) EMG analysis of stereotyped voluntary movements in man. J Neurol Neurosurg Psychiatry 38:1154-1162" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    j_ = rng_.uniform( 0, 100 );
  }

  std::map< std::string, Metric > metrics() const override
  {
    std::map< std::string, double > p;
    std::vector< Crit > c;
    std::string m;
    measure( p, c, m );
    return { { "interval_error_ms", { p.count( "interval_error_ms" ) ? p[ "interval_error_ms" ] : 999.0, "ms", true } } };
  }

protected:
  void mechanism( const mechb::Graph& g, std::vector< Crit >& out ) const override
  {
    // a chain of three different groups, each driven by the one before it, started by 'go'
    const auto b1 = g.out_pops( "b1" ), b2 = g.out_pops( "b2" ), b3 = g.out_pops( "b3" );
    auto disjoint = []( const mechb::PopSet& a, const mechb::PopSet& b ) {
      for ( int x : a )
        if ( b.count( x ) )
          return false;
      return true;
    };
    const bool three = !b1.empty() && !b2.empty() && !b3.empty() && disjoint( b1, b2 ) && disjoint( b2, b3 ) && disjoint( b1, b3 );
    const bool l12 = three && g.exc( b1, b2, 3 ), l23 = three && g.exc( b2, b3, 3 );
    out.push_back( mech( "three_groups", three, "The three bursts are not read from three different groups: a chain needs a group for b1, another for b2 and another for b3.",
                         "Each burst needs its own group of cells.", "b2", "Make three groups, one read out on each of b1, b2 and b3." ) );
    out.push_back( mech( "chain", l12 && l23,
                         std::string( "The groups are not a chain: " ) + ( !l12 ? "nothing drives the b2 group from the b1 group" : "nothing drives the b3 group from the b2 group" ) +
                         ", so one burst cannot release the next.",
                         "The links between the groups: b1's group must excite b2's group, and b2's group must excite b3's group.", "b2",
                         "Link the groups forward with excitatory projections and give each link a delay." ) );
    out.push_back( mech( "trigger", g.exc( g.in_pops( "go" ), b1, 3 ), "The 'go' trigger does not drive the first group of the chain.",
                         "'go' must start the first group, the one read out on b1.", "go", "Connect 'go' to the group read out on b1." ) );
  }

  double go_start( int k ) const { return ( k == 0 ? 600.0 : 2000.0 ) + j_; }

  void drive( Engine& e, double ) override
  {
    bool on = false;
    for ( int k = 0; k < 2; ++k )
      if ( t_ >= go_start( k ) && t_ < go_start( k ) + kGoLen ) on = true;
    e.set_input( "go", on ? kGo : 0.0 );
  }

  void update_readouts() override
  {
    for ( int i = 0; i < 3; ++i )
      readouts_[ std::string( kPool[ i ] ) + "_hz" ] = hz_now( kPool[ i ] );
  }

  /// Start times of bursts of a pool in [t0, t1] (window 20 ms, >= need spikes, 60 ms of quiet separates two bursts).
  std::vector< double > bursts( const std::string& ch, double t0, double t1 ) const
  {
    const double need = std::max( 2.0, std::ceil( 0.35 * pool( ch ) - 1e-9 ) );
    std::vector< double > out;
    double last_active = -1e9;
    for ( double t = t0 + 20; t <= t1 + 1e-9; t += 1.0 )
      if ( spikes( ch, t - 20, t ) >= need - 1e-9 )
      {
        if ( t - last_active > 60 ) out.push_back( first_spike( ch, t - 20, t ) );
        last_active = t;
      }
    return out;
  }

  void measure( std::map< std::string, double >& parts, std::vector< Crit >& crits, std::string& pass_msg ) const override
  {
    const double tl = tol();
    double lo = 100, hi = 220;
    if ( tl > 1.0 ) lo = 75, hi = 295;
    if ( tl < 1.0 ) lo = 115, hi = 190;
    const double early = spikes( "b1", 150, 500 ) + spikes( "b2", 150, 500 ) + spikes( "b3", 150, 500 );
    bool all_fire = true, single = true, order = true, rhythm = true;
    double ierr = 0;
    int nint = 0;
    std::string miss, rep, bad_order, bad_rhythm;
    double gap_seen[ 2 ][ 2 ] = { { -1, -1 }, { -1, -1 } };
    for ( int k = 0; k < 2; ++k )
    {
      const double g = go_start( k );
      std::vector< double > st[ 3 ];
      for ( int i = 0; i < 3; ++i )
      {
        st[ i ] = bursts( kPool[ i ], g - 30, g + 900 );
        if ( st[ i ].empty() )
        {
          all_fire = false;
          if ( miss.empty() ) miss = std::string( kPool[ i ] ) + " never bursts after a go";
        }
        if ( st[ i ].size() > 1 )
        {
          single = false;
          if ( rep.empty() ) rep = std::string( kPool[ i ] ) + " bursts more than once after one go";
        }
      }
      if ( st[ 0 ].empty() || st[ 1 ].empty() || st[ 2 ].empty() ) continue;
      const double s1 = st[ 0 ][ 0 ], s2 = st[ 1 ][ 0 ], s3 = st[ 2 ][ 0 ];
      parts[ "b1_latency_ms_trial" + std::to_string( k + 1 ) ] = s1 - g;
      parts[ "gap12_ms_trial" + std::to_string( k + 1 ) ] = s2 - s1;
      parts[ "gap23_ms_trial" + std::to_string( k + 1 ) ] = s3 - s2;
      if ( !( s1 < s2 && s2 < s3 ) || s1 - g > 120 || s1 - g < -10 )
      {
        order = false;
        if ( bad_order.empty() )
          bad_order = std::string( s1 < s2 && s2 < s3 ? "the bursts are in order but the first starts too late" : "the bursts do not come in the order b1, b2, b3" );
      }
      const double g12 = s2 - s1, g23 = s3 - s2;
      gap_seen[ k ][ 0 ] = g12;
      gap_seen[ k ][ 1 ] = g23;
      for ( double gp : { g12, g23 } )
      {
        ierr += std::fabs( gp - 150.0 );
        ++nint;
        if ( gp < lo || gp > hi )
        {
          rhythm = false;
          if ( bad_rhythm.empty() ) bad_rhythm = std::string( "the bursts are " ) + ( gp > hi ? "too far apart" : "too close together" );
        }
      }
    }
    parts[ "spikes_before_go" ] = early;
    parts[ "interval_error_ms" ] = nint ? ierr / nint : 999.0;

    Crit q;
    q.key = "quiet";
    q.ok = early <= 2;
    q.soft = 1.0 - ramp( early, 2, 40 );
    q.fail = "The pools fire on their own before the first go: the cells are driven by something other than the trigger. "
             "Check the biases of the cells (a bias that is too high makes a cell fire alone).";
    q.place = "A pool that fires without a go: its cells have a bias current that makes them fire by themselves.";
    q.target = "b1";
    q.recipe = "Leave the bias at zero on every group: the cells should wait for their input.";
    crits.push_back( q );

    Crit a;
    a.key = "all_three";
    a.ok = all_fire;
    a.soft = all_fire ? 1.0 : 0.3;
    a.fail = miss + ": a group does not fire strongly enough. Strengthen the link into it (the default weight is far too weak for a relay), "
             "or give the previous group more cells.";
    a.place = "The first link in the chain that is too weak: the group after it never gets enough input to burst.";
    a.target = miss.rfind( "b3", 0 ) == 0 ? "b3" : "b2";
    a.recipe = "Make each link in the chain strong enough that the whole previous group firing makes the next group fire; more cells in a group lowers the weight needed.";
    crits.push_back( a );

    Crit s;
    s.key = "single";
    s.ok = single;
    s.soft = single ? 1.0 : 0.4;
    s.fail = ( rep.empty() ? std::string( "A pool fires more than once" ) : rep ) + ": a burst must end by itself. Do not let a group excite itself or the group before it.";
    s.place = "A pool that bursts repeatedly: a loop in the wiring keeps it going.";
    s.target = "b1";
    s.recipe = "Links only run forward (b1 -> b2 -> b3); no link back and no group onto itself.";
    crits.push_back( s );

    Crit o;
    o.key = "order";
    o.ok = order;
    o.soft = order ? 1.0 : 0.4;
    o.fail = "Wrong order or too slow a start: " + ( bad_order.empty() ? std::string( "a burst is missing" ) : bad_order ) + ". b1 must start soon after the go, then b2, then b3.";
    o.place = "The first group must be driven straight by 'go' and the others behind it, in order.";
    o.target = "go";
    o.recipe = "go onto group 1 only; group 1 onto group 2 only; group 2 onto group 3 only.";
    crits.push_back( o );

    Crit r;
    r.key = "rhythm";
    r.ok = rhythm;
    double mean_gap = nint ? ( gap_seen[ 0 ][ 0 ] + gap_seen[ 0 ][ 1 ] + gap_seen[ 1 ][ 0 ] + gap_seen[ 1 ][ 1 ] ) / 4.0 : 0.0;
    r.soft = rhythm ? 1.0 : 0.5;
    r.fail = "The gaps are off: " + ( bad_rhythm.empty() ? std::string( "bursts missing" ) : bad_rhythm ) + ". " +
             ( mean_gap > hi ? "Shorten the delay of the links." : "Lengthen the delay of the links so each burst waits for the one before." );
    r.place = "The delay of the links between the groups sets the interval between bursts.";
    r.target = "b2";
    r.recipe = "Set the delay of the link between the first and second group, and of the one between the second and third, to the pause you want between bursts.";
    crits.push_back( r );

    pass_msg = "Three bursts in order after each go, evenly spaced by the delays: a pattern generator from delays.";
  }

private:
  double j_ = 0;
};

} // namespace

std::unique_ptr< Task > make_m_timed_bursts() { return std::make_unique< MTimedBursts >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_timed_bursts, 129, "m_timed_bursts.json", make_m_timed_bursts ) }
