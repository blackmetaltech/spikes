// Micro-puzzle "m_rhythm" (concept 17: rhythm plus sensory gating). A bursting cell makes the rhythm; a sensor decides how fast it runs.
//
// WORLD (5000 ms; the sensor "chemo" is dc, multiplied by the sensor weight (pA per unit) in the circuit)
//   Level 1.0 (rest) until s = 2400 + J (J = U(0, 100) ms seeded), then ramped over 200 ms to 1.6 (the air is turning stale: CO2 rises) and held.
//   Motor "phrenic": ONE bursting cell. The detector counts bursts of its spikes (two or more spikes less than 25 ms apart is a burst); a pool of
//   several identical cells fires out of step and smears the rhythm, so it is refused.
//   The tank's rhythm is about ten times faster than a breath, so that a run fits in five seconds.
// SCORING
//   one cell : "phrenic" reads exactly one cell
//   rhythm   : in [800, s] the cell fires >= 5 bursts of >= 2 spikes (it is a burster, not tonic or silent), at a rate fA in [5, 14] bursts/s
//              (guided [3.7, 19], researcher [6.3, 11.2])
//   gating   : in [s + 500, 5000] the burst rate fB >= 1.4 fA  (guided 1.2, researcher 1.65) and the cell still bursts (not a single tonic run), fB <= 3.5 fA
//   value 100 when passed, else 85 * credit of the three.
// Why it works out: an Izhikevich chattering cell (a 0.02, b 0.2, c -50, d 2) bursts when its drive I is about 4-9: the bursts come every 140 ms at I = 4
// and every 58 ms at I = 9 (the drive acts as the gate on the rate); below about 3.5 it is silent, above about 11 the bursts fuse into
// tonic firing. The lab's 300 pA per unit would fuse them at once: the sensor weight has to come down to about 5 pA per unit (5 pA at rest, 8 pA stale).
//
// Biology: the preBotzinger complex is a network of bursting pacemaker neurons (Smith JC, Ellenberger HH, Ballanyi K, Richter DW, Feldman JL (1991)
// Science 254:726-729); chemosensory drive from the retrotrapezoid nucleus and carotid body raises its burst frequency (Guyenet PG, Stornetta RL,
// Bayliss DA (2010) J Comp Neurol 518:3883-3906).
#include "micro_b.h"

namespace spike::tasks
{
namespace
{
using namespace microb;

constexpr double kDur = 5000, kRest = 1.0, kStale = 1.6, kRamp = 200;

class MRhythm : public MicroB
{
public:
  MRhythm() : MicroB( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_rhythm";
    i.title = "Rhythm and gating";
    i.brief = "Breathing is a rhythm made by a cell that bursts on its own, and the body's chemical state decides how fast it runs: when CO2 rises, "
              "the chemoreceptors raise the drive and the bursts come faster. In this tank the rhythm is ten times faster than a breath, so "
              "a run fits in five seconds. Build one bursting cell read out on 'phrenic' and feed it the sensor 'chemo' (the CO2 signal: level 1.0 "
              "at rest, 1.6 from about 2.5 s). At rest it must burst between 5 and 14 times a second (bursts of two spikes or more, not a steady "
              "train), and after the CO2 rise at least 1.4 times faster. Take the part 'Izhikevich chattering' (a bursting preset, the lab opens on it) "
              "as a single cell: a pool of identical cells would burst out of step and smear the rhythm. The sensor's weight is the current per unit of level, "
              "and a bursting cell only bursts inside a window of drive: too much makes it fire continuously.";
    i.chapter = "breathing";
    i.sensors = { "chemo" };
    i.motors = { "phrenic" };
    i.duration_ms = kDur;
    i.concepts_new = { "rhythm_gating" };
    i.concepts_used = { "pacemaking", "excitation" };
    i.par_neurons = 1;
    i.default_model = "izhikevich_CH";
    i.recipe = "One Izhikevich chattering cell (n = 1) read out on phrenic, with chemo wired to it. This cell bursts for a drive "
               "of about 4 to 9 pA, so set the chemo weight to about 5 pA per unit: 5 pA "
               "at rest bursts about 10 times a second, 8 pA after the CO2 rise about 15. Any higher weight, such as 300, makes "
               "the cell fire continuously.";
    i.kinds = { { "chemo", dc( kRest ) }, { "phrenic", rate() } };  // the lab's plain first-connect weight: the right one is for the player to find, not a default
    i.bands = { { "burst_rate_hz", "bursts/s", 5, 14 } };
    i.timeline = { { 2400, 2600, "CO2 rises" } };
    i.concept_hint = "A rhythm generator is a cell that bursts on its own; sensory input does not make the rhythm, it gates and tunes it: more drive, "
                     "faster bursts, up to the point where bursting turns into a steady train.";
    i.generic_hints = { { 1, i.concept_hint, "" },
      { 2, "The sensor's weight sets the drive: a bursting cell has a window of drive in which it bursts, and speeds up inside it. Look at the weight of 'chemo'.", "chemo" },
      { 3, "A single bursting cell read out on phrenic, with chemo connected to it. The lab's first-connection sensor weight is far too large for a bursting cell: "
           "turn it down a long way until the cell bursts at rest, then check it bursts faster, not steadily, after the CO2 rise.", "chemo" } };
    i.note = { "A rhythm that listens",
      "The preBotzinger complex in the medulla contains neurons that burst rhythmically by themselves; Jack Smith and Jack Feldman found "
      "in 1991 that a slice containing it keeps producing a respiratory rhythm. The rhythm is not rigid: chemoreceptors that sense CO2 and "
      "pH add drive and the bursts speed up, so breathing matches the body's needs. The tank here is ten times faster than a breath.",
      "Smith JC, Ellenberger HH, Ballanyi K, Richter DW, Feldman JL (1991) Science 254:726-729; Guyenet PG, Stornetta RL, Bayliss DA (2010) Central respiratory chemoreception. J Comp Neurol 518:3883-3906" };
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
    return { { "gating_ratio", { p.count( "rate_ratio" ) ? p[ "rate_ratio" ] : 0.0, "x", false } } };
  }

protected:
  void mechanism( const mechb::Graph& g, std::vector< Crit >& out ) const override
  {
    // an intrinsic burst mechanism in the cell that is read out, and the sensor reaching it
    const auto ph = g.out_pops( "phrenic" );
    bool bursty = !ph.empty();
    std::string model;
    for ( int p : ph )
    {
      bursty = bursty && mechb::Graph::can_burst( g.pop( p ).model );
      model = g.pop( p ).model;
    }
    out.push_back( mech( "bursting_cell", bursty,
                         "The cell read out on 'phrenic' cannot burst on its own: this model has no intrinsic burst mechanism, so no sensor weight will give a rhythm. "
                         "Use a bursting part (such as Izhikevich chattering).",
                         "The part of the cell read out on 'phrenic': it has to be a bursting model.", "phrenic",
                         "Replace the cell by the bursting part the lab opens on (Izhikevich chattering) and read it out on phrenic." ) );
    out.push_back( mech( "chemo_reaches", g.exc( g.in_pops( "chemo" ), g.out_pops( "phrenic" ), 3 ),
                         "The chemosensory input does not reach the bursting cell, so the sensor cannot tune the rhythm.",
                         "'chemo' has to drive the cell read out on 'phrenic'.", "chemo", "Connect chemo to the bursting cell." ) );
  }

  double s0() const { return 2400 + j_; }
  double level_at( double t ) const { return kRest + ( kStale - kRest ) * clampd( ( t - s0() ) / kRamp, 0, 1 ); }

  void drive( Engine& e, double ) override { e.set_input( "chemo", level_at( t_ ) * ( 1.0 + 0.01 * noise() * rng_.normal() ) ); }

  /// Burst starts (>= 2 spikes closer than 25 ms) in (t0, t1]; also the spikes in the bursts and the number of spikes outside any burst.
  void count_bursts( double t0, double t1, int& nb, double& spikes_in_bursts, int& lone ) const
  {
    nb = lone = 0;
    spikes_in_bursts = 0;
    double last = -1e9;
    int run = 0;
    auto close_run = [ & ]() {
      if ( run >= 2 ) ++nb, spikes_in_bursts += run;
      else if ( run == 1 ) ++lone;
      run = 0;
    };
    for ( double t = t0 + 1; t <= t1 + 1e-9; t += 1.0 )
    {
      const double n = spikes( "phrenic", t - 1, t );
      if ( n < 0.5 ) continue;
      if ( t - last > 25 ) close_run();
      run += static_cast< int >( n + 0.5 );
      last = t;
    }
    close_run();
  }

  void update_readouts() override
  {
    readouts_[ "chemo" ] = level_at( t_ );
    readouts_[ "phrenic_hz" ] = hz_now( "phrenic" );
    // burst rate over the last second (bursts per second)
    int nb, lone;
    double sp;
    const double t0 = std::max( 0.0, t_ - 1000 );
    count_bursts( t0, t_, nb, sp, lone );
    readouts_[ "burst_rate_hz" ] = t_ > 600 ? nb / ( ( t_ - t0 ) / 1000.0 ) : 0.0;
  }

  void measure( std::map< std::string, double >& parts, std::vector< Crit >& crits, std::string& pass_msg ) const override
  {
    const double tl = tol();
    const double s = s0();
    int na, nb, la, lb;
    double sa, sb;
    count_bursts( 800, s, na, sa, la );
    count_bursts( s + 500, kDur, nb, sb, lb );
    const double fa = na / ( ( s - 800 ) / 1000.0 ), fb = nb / ( ( kDur - s - 500 ) / 1000.0 );
    parts[ "burst_rate_rest_hz" ] = fa;
    parts[ "burst_rate_co2_hz" ] = fb;
    parts[ "bursts_rest" ] = na;
    parts[ "spikes_per_burst_rest" ] = na ? sa / na : 0;
    parts[ "rate_ratio" ] = fa > 0 ? fb / fa : 0;
    parts[ "cells" ] = pool( "phrenic" );
    const double lo = 5.0 / tl, hi = 14.0 * tl, gate = 1.0 + 0.4 / tl;

    Crit one;
    one.key = "one_cell";
    one.ok = pool( "phrenic" ) <= 1.0 + 1e-9;
    one.soft = 0.5;
    one.fail = "'phrenic' reads more than one cell. This detector counts the bursts of a single cell; identical cells burst out of step and smear the rhythm together. Use one cell.";
    one.place = "The size of the bursting group: it must be exactly one cell.";
    one.target = "phrenic";
    one.recipe = "Set the group to a single cell and read it out on phrenic.";
    crits.push_back( one );

    const bool rhythm_ok = na >= 5 && ( na ? sa / na >= 2.0 : false ) && fa >= lo && fa <= hi;
    Crit r;
    r.key = "rhythm";
    r.ok = rhythm_ok;
    r.soft = fa <= 0 ? 0.0 : ( fa < lo ? ramp( fa, 0, lo ) : 1.0 - ramp( fa, hi, 2 * hi ) );
    if ( na <= 2 && sa + la >= 20 )
      r.fail = "At rest the cell fires a steady train instead of bursts: the drive is far above the bursting window. The chemo weight is the current per "
               "unit of level, and a plain dc sensor starts far too high for a bursting cell: lower it a long way.";
    else if ( na == 0 && la == 0 )
      r.fail = "At rest the cell is silent: its drive is below the bursting window. Raise the chemo weight a little, or check that chemo reaches the cell.";
    else if ( na == 0 || ( na ? sa / na < 2.0 : true ) )
      r.fail = "At rest the cell fires but does not burst (single spikes, or a steady train): the drive is outside the bursting window. "
               "Too much drive makes the cell fire steadily; lower the chemo weight.";
    else if ( fa > hi )
      r.fail = "At rest the rhythm is too fast: lower the chemo weight.";
    else
      r.fail = "At rest the rhythm is too slow: raise the chemo weight a little.";
    r.place = "The weight of 'chemo': it sets the drive of the bursting cell, and the cell only bursts inside a window of drive.";
    r.target = "chemo";
    r.recipe = "Use the bursting part (Izhikevich chattering) as one cell and connect chemo to it; the sensor weight has to be small, because the cell only bursts inside a narrow window of drive.";
    crits.push_back( r );

    const bool gate_ok = nb >= 3 && ( nb ? sb / nb >= 2.0 : false ) && fb >= gate * fa && fb <= 3.5 * std::max( fa, 1.0 );
    Crit g;
    g.key = "gating";
    g.ok = gate_ok && rhythm_ok;
    g.soft = ramp( fa > 0 ? fb / fa : 0, 1.0, gate );
    if ( nb < 3 || ( nb ? sb / nb < 2.0 : true ) )
      g.fail = "After the CO2 rise the cell stops bursting: the extra drive pushed it out of the "
               "bursting window into steady firing. Lower the chemo weight so that both the rest and the stale level stay inside it.";
    else
      g.fail = "The CO2 rise does not speed the rhythm up enough: the sensory drive must tune the burst rate. Check chemo is wired to the bursting cell, and that the weight lets "
               "the drive move the cell across a useful part of its bursting window.";
    g.place = "The gate: the same weight of 'chemo' must keep the cell bursting at both levels and make it faster at the second.";
    g.target = "chemo";
    g.recipe = "A small chemo weight in the middle of the cell's bursting window: the rest level should burst slowly and the stale level should burst clearly faster.";
    crits.push_back( g );

    pass_msg = "Rest: " + fmt( fa, 1 ) + " bursts/s; after the CO2 rise: " + fmt( fb, 1 ) + " bursts/s (x" + fmt( fa > 0 ? fb / fa : 0, 2 ) + "): a rhythm tuned by a sensor.";
  }

private:
  double j_ = 0;
};

} // namespace

std::unique_ptr< Task > make_m_rhythm() { return std::make_unique< MRhythm >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_rhythm, 299, "m_rhythm.json", make_m_rhythm ) }
