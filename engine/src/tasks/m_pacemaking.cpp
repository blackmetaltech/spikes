// Micro-puzzle "m_pacemaking" (concept 9: intrinsic pacemaking). With no input at all, make a cell beat like a heart.
//
// WORLD (4000 ms; no sensors at all; the task holds ACh = 0 and NE = 0)
//   Motor "beat": spikes of the cell(s) wired to it. Nothing drives them: whatever fires, fires on its own.
// SCORING over (500, 4000] ms
//   beats  : spikes of the pool in the window, per cell: bpm = 60 * (spikes per cell) / 3.5 s     needs 50 <= bpm <= 140 (guided 40-160)
//   cv     : coefficient of variation of the intervals of the (merged) beat train                needs <= 0.15 (guided 0.25, researcher 0.1)
//   at least 3 beats in the window
//   value 100 when passed, else 85 * (criteria met / 3).
// Only an intrinsic oscillator passes. A plain integrate-and-fire cell is silent at 0 pA and, given a bias current (I_e) over its rheobase of
// 375 pA, fires at 20 Hz or more (above 1200 per minute); the sinoatrial node cell beats at about 103 per minute with no wiring at all.
//
// Biology: the sinoatrial node beats on its own because of a slow diastolic depolarisation driven by the hyperpolarisation-activated
// 'funny' current I_f (HCN channels) and calcium currents (DiFrancesco 1993); a heart with autonomic input blocked or removed (transplant)
// beats at the node's intrinsic rate, about 100 per minute in humans (Jose & Collison 1970).
#include <algorithm>
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;
using micro::Graph;

constexpr double kDur = 4000, kW0 = 500;

class MPacemaking : public MicroBase
{
public:
  MPacemaking() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_pacemaking";
    i.title = "Pacemaking";
    i.brief = "Most neurons are silent until something excites them. A pacemaker is different: it fires rhythmically all by itself, from the "
              "slow currents of its own membrane. The heart's pacemaker cell sits in the sinoatrial node and sets the heart rate. Here "
              "there is no input at all, nothing to drive a cell. Build a cell and wire it to 'beat' so that it beats on its own, "
              "regularly, between 50 and 140 times a minute (the beat intervals may vary by no more than 15 %). Look in the parts bin "
              "for a cell designed for the job (the lab opens on a plain one, which stays silent): a cell you have to push, even with "
              "a constant bias current, is not a pacemaker.";
    i.chapter = "heart";
    i.sensors = {};
    i.motors = { "beat" };
    i.duration_ms = kDur;
    i.concepts_new = { "pacemaking" };
    i.concepts_used = { "threshold" };
    i.par_neurons = 1;
    i.default_model = "iaf_psc_alpha";  // the lab opens on a plain cell, which is silent: finding the pacemaker card IS the puzzle
    i.recipe = "Swap the plain cell the lab opens on for the SA node cell, and wire it to 'beat', nothing else: it beats about 100 times a minute on its own. No sensor, no input "
               "weight, no bias current.";
    i.kinds = { { "beat", micro::rate() } };
    i.bands = { { "bpm", "beats/min", 50, 140 }, { "cv_pct", "%", 0, 15 } };
    i.modulators = { "ACh", "NE" };
    i.note = { "The heart starts itself",
      "A heart removed from the body keeps beating, because the cells of its sinoatrial node fire on their own: a slow depolarisation "
      "between beats, driven by the hyperpolarisation-activated 'funny' current I_f and by calcium currents, carries the membrane to "
      "threshold again and again. With the autonomic nerves blocked, or cut as in a heart transplant, the human heart beats at the "
      "node's own rate, about 100 per minute; at rest the vagus holds it back to 60 to 70.",
      "DiFrancesco D (1993) Pacemaker mechanisms in cardiac tissue. Annu Rev Physiol 55:455-472; "
      "Jose AD, Collison D (1970) The normal range and determinants of the intrinsic heart rate in man. Cardiovasc Res 4:160-167" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    started_ = false;
    beats_.clear();
    n_win_ = 0;
    pool_ = 1;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "Pacemaking is rhythmic firing with no input: slow inward currents in the membrane bring it back to threshold again and "
                  "again. Most cells have none of that and need a push to fire.", "" },
      { 2, "The parts bin has a cell built for this: the sinoatrial (SA) node pacemaker. Add one and wire it to the 'beat' output. A cell that "
           "is pushed (by a bias current, or by other cells) is not a pacemaker.", "beat" },
      { 3, "One SA node cell wired to 'beat', nothing else: no sensor, no synapse onto it, no bias current. The lab opens on a plain cell, "
           "which stays silent: change the part.", "beat" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    Stats st = stats();
    return { { "interval_cv_pct", { st.n >= 3 ? 100.0 * st.cv : 99.0, "%", true } } };
  }

  void inspect( Engine& ) override
  {
    const auto beat = g_.out_pops( "beat" );
    for ( int p : beat )
    {
      const double ie = g_.param( p, "I_e" );
      if ( !std::isnan( ie ) && std::fabs( ie ) > 1e-9 )
      {
        mech_ = "The cell is being pushed by a bias current: that is a constant drive from outside, not pacemaking. A pacemaker beats from its own "
                "membrane currents, with nothing pushing it.";
        return;
      }
    }
    if ( g_.indegree( beat ) > 0 )
      mech_ = "The cell is driven by other cells: its beat comes from them, not from itself. A pacemaker needs no input at all.";
  }

  Score score() const override
  {
    Score s;
    Stats st = stats();
    const double lo = mode_ == "guided" ? 40 : 50, hi = mode_ == "guided" ? 160 : 140;
    const double cvmax = mode_ == "guided" ? 0.25 : ( mode_ == "researcher" ? 0.1 : 0.15 );
    s.parts[ "beats" ] = st.n;
    s.parts[ "bpm" ] = st.bpm;
    s.parts[ "cv_pct" ] = st.n >= 3 ? 100.0 * st.cv : -1.0;
    const bool a = st.n >= 3, b = a && st.bpm >= lo && st.bpm <= hi, c = a && st.cv <= cvmax;
    finish( s, ( a ? 1 : 0 ) + ( b ? 1 : 0 ) + ( c ? 1 : 0 ), 3 );
    if ( !done() )
      s.passed = false;
    apply_mech( s );
    std::ostringstream o;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done() )
      o << "Running: " << static_cast< int >( beats_.size() ) << " beats so far.";
    else if ( !mech_.empty() )
      o << mech_;
    else if ( !a )
      o << "The cell stays silent: it needs an input to fire, and there is none. A pacemaker cell fires by itself. "
           "Choose a cell from the parts bin that is described as a pacemaker.";
    else if ( st.bpm > hi )
      o << "It fires, but far too fast for a heart. A real pacemaker beats about once a second, from its own slow currents.";
    else if ( st.bpm < lo )
      o << "Too slow for a heart.";
    else if ( !c )
      o << "Rate fine but the beats are irregular. With several cells wired to 'beat' their beats merge: use one.";
    else
      o << "A pacemaker: it beats at a heart's pace with no input at all, and regularly.";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    if ( started_ ) return;
    started_ = true;
    e.set_modulator( "ACh", 0.0 );
    e.set_modulator( "NE", 0.0 );
  }

  void observe( Engine& e, double dt ) override
  {
    const double n = motor_.spike_count( e, "beat", dt );
    pool_ = motor_.pool_size( e, "beat" );
    if ( n > 0 )
    {
      beats_.push_back( t_ );
      if ( t_ > kW0 ) n_win_ += n;
    }
    Stats st = stats();
    readouts_[ "bpm" ] = st.n >= 2 ? st.bpm : 0.0;
    readouts_[ "cv_pct" ] = st.n >= 3 ? 100.0 * st.cv : 0.0;
    readouts_[ "beats" ] = static_cast< double >( beats_.size() );
  }

private:
  struct Stats
  {
    double n = 0, bpm = 0, cv = 0;
  };

  Stats stats() const
  {
    Stats st;
    st.n = n_win_;
    // rate per cell over the part of the window that has run so far
    const double len = std::max( 1.0, std::min( t_, kDur ) - kW0 ) / 1000.0;
    st.bpm = 60.0 * ( n_win_ / pool_ ) / len;
    std::vector< double > iv;
    double prev = -1;
    for ( double t : beats_ )
    {
      if ( t <= kW0 ) continue;
      if ( prev >= 0 ) iv.push_back( t - prev );
      prev = t;
    }
    if ( iv.size() >= 2 )
    {
      double m = 0;
      for ( double x : iv ) m += x;
      m /= iv.size();
      double v = 0;
      for ( double x : iv ) v += ( x - m ) * ( x - m );
      v /= iv.size() - 1;
      st.cv = m > 0 ? std::sqrt( v ) / m : 1.0;
    }
    else
      st.cv = 1.0;
    return st;
  }

  std::vector< double > beats_;
  double n_win_ = 0, pool_ = 1;
  bool started_ = false;
};

} // namespace

std::unique_ptr< Task > make_m_pacemaking() { return std::make_unique< MPacemaking >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_pacemaking, 198, "m_pacemaking.json", make_m_pacemaking ) }
