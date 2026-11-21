// Scaffolding shared by the micro-puzzles of concepts 11-19 (m_recruitment ... m_divergent; docs/DESIGN-0.3.md section 3).
// A micro-puzzle is a fish-tank task: 1-4 neuron groups, a 2-5 s episode, one new concept, a dashboard with a few clear readouts.
// Nothing here touches NEST beyond Engine; the worlds are plain C++ and deterministic given the seed.
//
// A puzzle supplies: info() (channels, bands, notebook ...), reset(), drive() (write the sensors for the next 1 ms slice), plant()
// (advance a body model from the motor rates, optional) and measure() (read the windows of the recorded spike trains, fill the score
// parts and a list of acceptance criteria). The base class does the rest: wiring check, spike recording (cumulative counts, so any window
// is O(1)), smoothed rates for the dashboard, the verdict (the first criterion that fails speaks), the three-tier hints (each criterion
// carries its own place and partial recipe), and the mode scaling.
//
// DIFFICULTY (set_mode, docs/DESIGN-0.3.md section 5): a mode scales only the tolerance of the acceptance bands (tol()) and the
// noise of the stimulus (noise()); the structure of the puzzle never changes.
#pragma once
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "mech_b.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks::microb
{

/// ChannelKind builders (aggregate initialisation of ChannelKind trips -Wmissing-field-initializers).
inline Task::ChannelKind dc( double level, double weight = 300.0 )
{
  Task::ChannelKind k;
  k.kind = "dc";
  k.value = level;
  k.weight = weight;  // default first-connect weight, pA per unit of level
  return k;
}
inline Task::ChannelKind poisson( double hz )
{
  Task::ChannelKind k;
  k.kind = "poisson";
  k.value = hz;
  return k;
}
inline Task::ChannelKind rate() { return Task::ChannelKind{ "rate", 0, "", 0.05, 1000.0 }; }

/// One acceptance criterion. `soft` (0..1) is the partial credit of a failed run; `fail` says what is wrong in numbers; `place` and `target`
/// are hint tier 2 (where to look: a channel id or population the lab can highlight); `recipe` is tier 3 (a partial recipe, with numbers).
struct Crit
{
  std::string key;
  bool ok = true;
  double soft = 1.0;
  std::string fail, place, target, recipe;
};

class MicroB : public Task
{
public:
  struct Info
  {
    std::string id, title, brief, chapter, view = "dashboard";
    std::vector< std::string > sensors, motors, concepts_new, concepts_used;
    double duration_ms = 4000;
    int par_neurons = 1;
    std::map< std::string, ChannelKind > kinds;
    std::vector< Band > bands;
    std::vector< Event > timeline;
    Note note;
    std::string default_model = "iaf_psc_alpha";  // catalog card the lab opens the level on (the cell the reference and verdicts assume)
    std::string recipe;                            // concrete numbers, kept out of the brief (shown by mode in the lab)
    std::string concept_hint;           // hint tier 1
    std::vector< Hint > generic_hints;  // shown when no criterion fails (a passed run) or the circuit is not wired
  };

  explicit MicroB( Info i ) : info_( std::move( i ) ) {}

  std::string id() const override { return info_.id; }
  std::string title() const override { return info_.title; }
  std::string brief() const override { return info_.brief; }
  std::vector< std::string > sensor_channels() const override { return info_.sensors; }
  std::vector< std::string > motor_channels() const override { return info_.motors; }
  double duration_ms() const override { return info_.duration_ms; }
  std::string chapter() const override { return info_.chapter; }
  std::string view() const override { return info_.view; }
  int par_neurons() const override { return info_.par_neurons; }
  std::vector< Event > timeline() const override { return info_.timeline; }
  std::vector< Band > bands() const override { return info_.bands; }
  std::map< std::string, ChannelKind > channel_kinds() const override { return info_.kinds; }
  std::string level_kind() const override { return "micro"; }
  std::vector< std::string > concepts_new() const override { return info_.concepts_new; }
  std::vector< std::string > concepts_used() const override { return info_.concepts_used; }
  Note notebook() const override { return info_.note; }
  std::string default_model() const override { return info_.default_model; }
  std::string recipe() const override { return info_.recipe; }
  std::array< double, 4 > world_bounds() const override { return { 0, 0, 1, 1 }; }

  BodyState body() const override
  {
    BodyState b;
    b.points = { { 0, 0 }, { 1, 0 } };
    b.readouts = readouts_;
    b.readouts[ "t_ms" ] = t_;
    return b;
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, info_.sensors, info_.motors );
    if ( !inspected_ )  // the circuit as the run starts (before any slice): mechanism criteria are judged on its structure
    {
      inspected_ = true;
      graph_.capture( e );
      mech_.clear();
      if ( graph_.valid() )
        mechanism( graph_, mech_ );
    }
    drive( e, dt );
    e.step( dt );
    t_ += dt;
    record( e, dt );
    plant( dt );
    update_readouts();
  }

  Score score() const override
  {
    Score s;
    std::vector< Crit > cs;
    std::string pass_msg;
    collect( s.parts, cs, pass_msg );
    bool mech_ok = true;
    for ( const auto& c : mech_ )
      mech_ok = mech_ok && c.ok;
    int met = 0;
    double soft = 0;
    const Crit* bad = nullptr;
    for ( const auto& c : cs )
    {
      if ( c.ok )
        ++met;
      else if ( !bad )
        bad = &c;
      soft += c.ok ? 1.0 : clampd( c.soft, 0, 1 );
    }
    const bool all_ok = bad == nullptr;
    s.passed = done() && wiring_.ok() && all_ok;
    s.value = s.passed ? 100.0 : 85.0 * soft / std::max< size_t >( cs.size(), 1 );
    if ( !wiring_.ok() )
      s.value = std::min( s.value, 30.0 );  // a circuit that leaves a channel unwired has not tried the puzzle yet
    if ( !mech_ok )
      s.value = std::min( s.value, 40.0 );  // behaviour without the mechanism the level teaches is not a solution (docs/REQ-0.4.md rule 1)
    if ( !s.passed && silent() )
      s.value = 0;  // nothing fired anywhere: staying quiet at rest is not partial credit
    if ( wiring_.bare() )   // an empty bench has built nothing: no score and no criterion met
    {
      s.value = 0;
      s.passed = false;
      met = 0;
    }
    s.parts[ "criteria_met" ] = met;
    s.parts[ "criteria_total" ] = static_cast< double >( cs.size() );
    if ( !wiring_.ok() )
      s.verdict = "Not wired: " + wiring_.message() + ".";
    else if ( !done() )
      s.verdict = "Running.";
    else if ( bad )
      s.verdict = bad->fail;
    else
      s.verdict = pass_msg;
    return s;
  }

  std::vector< Hint > hints() const override
  {
    Score tmp;
    std::vector< Crit > cs;
    std::string pm;
    collect( tmp.parts, cs, pm );
    const Crit* bad = nullptr;
    for ( const auto& c : cs )
      if ( !c.ok )
      {
        bad = &c;
        break;
      }
    if ( !bad || !done() || !wiring_.ok() )
      return info_.generic_hints;
    return { { 1, info_.concept_hint, "" }, { 2, bad->place, bad->target }, { 3, bad->recipe, bad->target } };
  }

protected:
  /// Mode scaling: > 1 means wider bands (guided), < 1 tighter (researcher).
  double tol() const { return mode_ == "guided" ? 1.35 : ( mode_ == "researcher" ? 0.8 : 1.0 ); }
  /// Stimulus noise multiplier: 0 guided, 1 standard, 2 researcher.
  double noise() const { return mode_ == "guided" ? 0.0 : ( mode_ == "researcher" ? 2.0 : 1.0 ); }

  void begin( uint64_t seed )
  {
    rng_ = Rng( seed ^ hash_id() );
    t_ = 0;
    inspected_ = false;
    mech_.clear();
    graph_.clear();
    wiring_.invalidate();
    motor_.invalidate();
    cum_.clear();
    pool_.clear();
    lp_.clear();
    readouts_.clear();
    for ( const auto& m : info_.motors )
      cum_[ m ] = std::vector< double >( 1, 0.0 );
  }

  /// Set the inputs for the slice about to run (t_ is its start).
  virtual void drive( Engine& e, double dt ) = 0;
  /// Advance the body model after the slice (t_ is now its end); smoothed motor rates are available through hz_now().
  virtual void plant( double dt ) {}
  /// Dashboard readouts, called after every slice.
  virtual void update_readouts() {}
  /// Mechanism criteria (docs/REQ-0.4.md rule 1), judged on the circuit structure captured when the run starts. Each criterion that fails names the
  /// missing part of the mechanism in words (no numbers); they speak before the behaviour criteria. Default: none.
  virtual void mechanism( const mechb::Graph& g, std::vector< Crit >& out ) const { (void)g; (void)out; }
  /// A mechanism criterion: `ok` when the structure has the part; the rest is the verdict and the hint tiers 2 and 3 (words only).
  static Crit mech( const std::string& key, bool ok, const std::string& fail, const std::string& place, const std::string& target, const std::string& direction )
  {
    Crit c;
    c.key = key;
    c.ok = ok;
    c.soft = 0.0;
    c.fail = fail;
    c.place = place;
    c.target = target;
    c.recipe = direction;
    return c;
  }
  /// Fill the score parts and the criteria from the recorded windows; pass_msg is the verdict of a passed run.
  virtual void measure( std::map< std::string, double >& parts, std::vector< Crit >& crits, std::string& pass_msg ) const = 0;

  /// The criteria in speaking order: the mechanism first, then the behaviour.
  void collect( std::map< std::string, double >& parts, std::vector< Crit >& cs, std::string& pass_msg ) const
  {
    measure( parts, cs, pass_msg );
    cs.insert( cs.begin(), mech_.begin(), mech_.end() );
  }
  bool done() const { return t_ >= info_.duration_ms - 1e-6; }
  /// True while no motor channel has fired a single spike.
  bool silent() const
  {
    for ( const auto& m : info_.motors )
    {
      auto it = cum_.find( m );
      if ( it != cum_.end() && !it->second.empty() && it->second.back() > 0.5 )
        return false;
    }
    return true;
  }

  /// Spikes of motor channel `ch` in (t0, t1] ms of model time.
  double spikes( const std::string& ch, double t0, double t1 ) const
  {
    auto it = cum_.find( ch );
    if ( it == cum_.end() )
      return 0;
    const auto& c = it->second;
    auto at = [ & ]( double t ) {
      long i = static_cast< long >( std::floor( t + 1e-6 ) );
      i = std::max( 0L, std::min( i, static_cast< long >( c.size() ) - 1 ) );
      return c[ static_cast< size_t >( i ) ];
    };
    return at( t1 ) - at( t0 );
  }
  double pool( const std::string& ch ) const
  {
    auto it = pool_.find( ch );
    return it == pool_.end() ? 1.0 : std::max( 1.0, it->second );
  }
  /// Mean Hz per neuron of channel `ch` over (t0, t1].
  double hz( const std::string& ch, double t0, double t1 ) const
  {
    return t1 > t0 ? spikes( ch, t0, t1 ) / pool( ch ) * 1000.0 / ( t1 - t0 ) : 0.0;
  }
  /// Smoothed instantaneous rate (Hz per neuron, tau 40 ms) for the dashboard and the plant.
  double hz_now( const std::string& ch ) const
  {
    auto it = lp_.find( ch );
    return it == lp_.end() ? 0.0 : it->second.y;
  }
  /// Time of the first spike of a sliding window of `win` ms that holds >= `need` spikes of `ch`, searched in [t0, t1] (-1 if never).
  double first_window( const std::string& ch, double t0, double t1, double win, double need ) const
  {
    for ( double t = t0 + win; t <= t1 + 1e-9; t += 1.0 )
      if ( spikes( ch, t - win, t ) >= need - 1e-9 )
        return first_spike( ch, t - win, t );
    return -1;
  }
  /// Time (ms) of the first spike of `ch` in (t0, t1], or t1 if none.
  double first_spike( const std::string& ch, double t0, double t1 ) const
  {
    for ( double t = t0 + 1; t <= t1 + 1e-9; t += 1.0 )
      if ( spikes( ch, t - 1, t ) >= 0.5 )
        return t;
    return t1;
  }

  static std::string fmt( double x, int prec = 0 )
  {
    std::ostringstream o;
    o << std::fixed << std::setprecision( prec ) << x;
    return o.str();
  }
  /// 1 when v >= good, 0 when v <= bad, linear between (soft credit helper).
  static double ramp( double v, double bad, double good )
  {
    return good == bad ? ( v >= good ? 1.0 : 0.0 ) : clampd( ( v - bad ) / ( good - bad ), 0, 1 );
  }

  Info info_;
  Wiring wiring_;
  MotorReader motor_;
  Rng rng_;
  double t_ = 0;
  std::map< std::string, double > readouts_;
  mechb::Graph graph_;
  std::vector< Crit > mech_;
  bool inspected_ = false;

private:
  void record( Engine& e, double dt )
  {
    const long bin = static_cast< long >( std::ceil( t_ - 1e-6 ) );  // cum_[i] holds the spikes up to time i ms; the slice ending at t_ adds to cum_[ceil(t_)]
    for ( const auto& m : info_.motors )
    {
      const double n = motor_.spike_count( e, m, dt );
      pool_[ m ] = motor_.pool_size( e, m );
      auto& c = cum_[ m ];
      while ( static_cast< long >( c.size() ) <= bin )
        c.push_back( c.back() );
      c[ static_cast< size_t >( bin ) ] += n;
      auto& f = lp_[ m ];
      f.tau_ms = 40;
      f.step( n / std::max( 1.0, pool_[ m ] ) * 1000.0 / dt, dt );
    }
  }
  uint64_t hash_id() const
  {
    uint64_t h = 1469598103934665603ULL;
    for ( char c : info_.id )
      h = ( h ^ static_cast< unsigned char >( c ) ) * 1099511628211ULL;
    return h;
  }

  std::map< std::string, std::vector< double > > cum_;
  std::map< std::string, double > pool_;
  std::map< std::string, Lowpass > lp_;
};

} // namespace spike::tasks::microb
