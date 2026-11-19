// Task "pupil": the pupillary light reflex (parasympathetic) and the sympathetic dilator pathway (darkness, fright).
//
// WORLD (spike::body::Eye, docs/body/eye.md; 1 ms slices, game time = real time)
//   Light (cd/m^2):   dim 1 until 4 s, bright 200 from 4 s, dim again from 11 s, medium 30 from 20 s. A threat cue runs 30-34 s (at medium light).
//   Sensor "retina":  Poisson rate = kFibres * retinal_hz of the eye model (the eye's own retinal illuminance = luminance x pupil area, so a
//                     small pupil lowers the signal: the negative feedback the reflex closes). 5-100 Hz per fibre, x4 fibres.
//   Sensor "threat":  Poisson rate, 0 normally and kThreatHz (250 Hz) during the fright cue (amygdala / hypothalamic alarm).
//   Motor "sphincter": ciliary-ganglion pool, mean Hz per neuron / kSphRef (40 Hz)  = sphincter drive 0..1 (parasympathetic).
//   Motor "dilator":   superior-cervical-ganglion pool, mean Hz per neuron / kDilRef (40 Hz) = dilator drive 0..1 (sympathetic).
//   Pupil diameter D = 2 + 6 clamp(0.5 + 0.5 (dilator - sphincter)) mm with first-order muscle kinetics (sphincter 130/500 ms, dilator 800/300 ms).
//
// MECHANISM (Engine::structure(), checked at the start and every 0.5 s): two pathways onto two separate pools, the parasympathetic reflex arc (an
//   excitatory path from 'retina' to the cells 'sphincter' reads) and the sympathetic arm (an excitatory path from 'threat' to the cells 'dilator'
//   reads). One pool read by both outputs is one muscle, not two. Missing a pathway caps the score below the pass bar; the verdict names it.
//
// SCORING
//   dim     = mean D over 3-4 s (adapted to the dark), bright = mean D over 9.5-11 s, dim2 = mean D over 18.5-20 s,
//   medium  = mean D over 24.5-26 s, fright = mean D over 32.5-34 s.
//   constriction = dim - bright, redilation = dim2 - bright, fright dilation = fright - medium.
//   latency (constriction) = time after the 4 s step until D has fallen 20 % of the way from its value just before the step (mean of the preceding 200 ms,
//   not the 3-4 s mean, which pupil noise already crosses) to the bright plateau and stays there for 50 ms; (redilation) likewise after 11 s.
//   A crossing within 50 ms of the step is not a reflex and is ignored; no persistent crossing = no response.
//   pass  : constriction >= 2.5 mm and bright <= 4.5 mm (direction and amplitude), redilation >= 2.5 mm and dim2 >= 6 mm (both directions),
//           constriction latency 150-350 ms (human 200-250 ms), fright dilation >= 0.8 mm.
//   value = 100 * mean of five 0..1 soft scores (constriction amplitude, redilation amplitude, constriction latency, redilation latency (0.1-2 s), fright).
//
// Biology: retinal ganglion cells (incl. melanopsin) -> olivary pretectal nucleus -> Edinger-Westphal nucleus -> ciliary ganglion -> iris sphincter
// (Gamlin 2006; McDougal & Gamlin 2015 Compr Physiol 5:439); darkness and arousal dilate by the sympathetic path hypothalamus -> intermediolateral
// column (C8-T2) -> superior cervical ganglion -> dilator, and by inhibiting the Edinger-Westphal nucleus (Steinhauer & Hakerem 1992).
// Human light-reflex latency 200-250 ms (Ellis 1981; Loewenfeld 1993).
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include "spike/body/eye.h"
#include "mech_util.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{
namespace
{

constexpr double kDuration = 40000;
constexpr double kFibres = 4, kThreatHz = 250, kSphRef = 40, kDilRef = 40;
constexpr double kDim = 1, kBright = 200, kMedium = 30, kLatFrac = 0.2;
constexpr double kBrightOn = 4000, kBrightOff = 11000, kMediumOn = 20000, kThreatOn = 30000, kThreatOff = 34000;

class PupilTask : public Task
{
public:
  PupilTask() { reset( 1 ); }
  std::string id() const override { return "pupil"; }
  std::string title() const override { return "Pupil light reflex"; }
  std::string brief() const override
  {
    return "The pupil is a two-muscle servo. Light on the retina fires ganglion cells that excite the pretectum, which excites the "
           "Edinger-Westphal nucleus; its neurons relay in the ciliary ganglion to the iris sphincter and squeeze the pupil shut "
           "(parasympathetic, fast). In the dark, and when frightened, the sympathetic chain (hypothalamus, spinal intermediolateral "
           "column, superior cervical ganglion) drives the radial dilator and the pupil opens (slower). Light also switches the "
           "sympathetic side off. Wire 'retina' to the reflex arc and 'threat' to the sympathetic arm, and read out 'sphincter' and "
           "'dilator'. Score: a step to bright light must constrict the pupil by 2.5 mm or more within about a quarter of a second, the "
           "return to dark must open it again, and a fright cue must dilate it. ";
  }
  /// The cell type the reference circuit and the verdicts assume; the numbers of the build live in recipe() (shown by mode in the lab).
  std::string default_model() const override { return "iaf_psc_alpha"; }
  std::string recipe() const override
  {
    return "A recipe that works (iaf_psc_alpha groups; every link at the "
           "default weight, excitatory 80 pA, inhibitory -80 pA; ADVANCED bias I_e in pA): retina onto RGC (6 cells, bias 100) > PON (4, 225) "
           "> EW (4, 340, tau syn I 10 ms) > CG (4, 340) > sphincter, all three links with delay 15 ms (the reflex then takes about 200 ms); threat onto HYP (4, "
           "bias 100) > IML (4, bias 410, tau syn E and I 10 ms) > SCG (4, bias 340) > dilator; PON inhibits IML (light switches the sympathetic "
           "side off) and HYP inhibits EW (fright switches the parasympathetic side off). The biases make those cells fire on their own: "
           "the dilator is tonic in the dark and the light turns it off.";
  }
  std::vector< std::string > sensor_channels() const override { return { "retina", "threat" }; }
  std::vector< std::string > motor_channels() const override { return { "sphincter", "dilator" }; }
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    ChannelKind r, t;
    r.kind = t.kind = "poisson";
    r.value = kFibres * 5;
    t.value = kThreatHz;
    return { { "retina", r }, { "threat", t }, { "sphincter", ChannelKind{ "rate" } }, { "dilator", ChannelKind{ "rate" } } };
  }
  double duration_ms() const override { return kDuration; }
  std::string chapter() const override { return "autonomic"; }
  std::string view() const override { return "eye"; }
  int par_neurons() const override { return variant_par( variant_, 30 ); }

  // ---- v0.3 ----
  std::string level_kind() const override { return "task"; }
  std::vector< std::string > concepts_new() const override { return {}; }
  std::vector< std::string > concepts_used() const override { return { "excitation", "inhibition", "reciprocal_inhibition" }; }
  bool on_spine() const override { return true; }
  /// Pass bars (constriction and redilation amplitude / bright and dark diameters / latency window / fright dilation):
  /// guided 2.0 mm, 5.0 / 5.5 mm, 120-400 ms, 0.64 mm; standard 2.5, 4.5 / 6.0, 150-350, 0.8; researcher 2.94, 4.2 / 6.3, 160-330, 0.94.
  /// Fright cue x1.2 guided, x0.9 researcher.
  std::map< std::string, std::string > variants() const override
  {
    return { { "half_neurons", "half the neurons: 15 instead of 30" },
             { "weak_retina", "three retinal fibres instead of four: a weaker light signal" } };
  }
  std::vector< Hint > hints() const override
  {
    Score s = score();
    std::vector< Hint > h;
    if ( !mech_.ok() )
    {
      h.push_back( make_hint( 1, "The pupil is a two-muscle servo: a parasympathetic pathway closes it (retina to sphincter) and a sympathetic pathway opens it (threat to dilator). Each needs its own chain of cells.", "" ) );
      h.push_back( make_hint( 2, "One of the two pathways is missing, or both end on the same cells.", "dilator" ) );
      h.push_back( make_hint( 3, "Build a relay chain from retina to the sphincter output, and a separate chain from threat to the dilator output.", "" ) );
    }
    else if ( s.parts[ "sphincter_spikes" ] < 1 || s.parts[ "constriction_mm" ] < amp_min() )
    {
      h.push_back( make_hint( 1, "The light reflex is excitation along a chain: retina, pretectum, Edinger-Westphal, ciliary ganglion, sphincter. Light must also switch the dilator off.", "" ) );
      h.push_back( make_hint( 2, "The sphincter pool is not squeezing the pupil enough.", "sphincter" ) );
      h.push_back( make_hint( 3, "Give the relay cells a standing drive so they fire, strengthen the chain, and let the light signal inhibit the sympathetic side.", "" ) );
    }
    else if ( s.parts[ "fright_dilation_mm" ] < fright_min() )
    {
      h.push_back( make_hint( 1, "Fright dilates the pupil by the sympathetic arm and by inhibiting the parasympathetic Edinger-Westphal cells.", "" ) );
      h.push_back( make_hint( 2, "The threat input does not reach the sympathetic arm.", "threat" ) );
      h.push_back( make_hint( 3, "Make the threat chain strong enough to raise the dilator output, and let it also inhibit the parasympathetic relay.", "" ) );
    }
    else
    {
      h.push_back( make_hint( 1, "A reflex arc has a latency made of synaptic and conduction delays; a human pupil answers within a fraction of a second.", "" ) );
      h.push_back( make_hint( 2, "The constriction latency is outside the window: look at the delays along the parasympathetic chain.", "sphincter" ) );
      h.push_back( make_hint( 3, "Lengthen the synaptic delay along the reflex arm if the pupil answers too fast, shorten it if too slow.", "" ) );
    }
    return h;
  }
  std::map< std::string, Metric > metrics() const override { return { { "constriction_latency_ms", make_metric( score().parts[ "constriction_latency_ms" ], "ms", true ) } }; }
  Note notebook() const override
  {
    return { "Two muscles, one pupil",
             "Light shuts the pupil through the pretectum, the Edinger-Westphal nucleus and the ciliary ganglion to the iris sphincter. "
             "Darkness and fright open it through the sympathetic chain to the radial dilator. The human light reflex takes 200-250 ms.",
             "McDougal DH, Gamlin PD (2015) Compr Physiol 5:439; Loewenfeld IE (1993) The Pupil" };
  }
  std::vector< Event > timeline() const override
  {
    return { { kBrightOn, kBrightOff - kBrightOn, "light flash (200 cd/m2)" },
             { kBrightOff, kMediumOn - kBrightOff, "darkness" },
             { kThreatOn, kThreatOff - kThreatOn, "fright cue" } };
  }
  std::vector< Band > bands() const override { return { { "pupil_mm", "mm", 2.0, 8.0 } }; }

  void reset( uint64_t ) override
  {
    t_ = 0;
    eye_.reset( 0.0, 0.5 );
    lum_ = kDim;
    d_.clear();
    sph_ = dil_ = 0;
    ss_.reset( 0 );
    sd_.reset( 0 );
    spikes_s_ = spikes_d_ = 0;
    motor_.invalidate();
    wiring_.invalidate();
    mech_.invalidate();
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, sensor_channels(), motor_channels() );
    mech_.refresh( e, t_, []( const Structure& st ) { return pathways_missing( st ); } );
    lum_ = luminance( t_ );
    bool threat = t_ >= kThreatOn && t_ < kThreatOff;
    e.set_input( "retina", fibres() * eye_.retinal_hz );
    e.set_input( "threat", threat ? kThreatHz * mode_factor( mode_, 1.2, 0.9 ) : 0.0 );
    e.step( dt );
    double hs = motor_.hz_per_neuron( e, "sphincter" ), hd = motor_.hz_per_neuron( e, "dilator" );
    spikes_s_ += motor_.spike_count( e, "sphincter", dt );
    spikes_d_ += motor_.spike_count( e, "dilator", dt );
    // the motor rates of one slice are spike counts (0 or 1000/N Hz): smooth them (20 ms) BEFORE the clamp, then the eye's own kinetics
    ss_.tau_ms = sd_.tau_ms = 20;
    sph_ = ss_.step( hs / kSphRef, dt );
    dil_ = sd_.step( hd / kDilRef, dt );
    body::EyeInput in;
    in.sphincter = clampd( sph_, 0, 1 );
    in.dilator = clampd( dil_, 0, 1 );
    in.luminance = lum_;
    eye_.step( dt, in );
    t_ += dt;
    int n = static_cast< int >( t_ / 1.0 + 1e-9 );
    while ( static_cast< int >( d_.size() ) < n )
      d_.push_back( eye_.diameter_mm );
  }

  BodyState body() const override
  {
    BodyState b;
    auto v = eye_.view();
    b.readouts[ "pupil_mm" ] = eye_.diameter_mm;
    // light 0..1: log10 luminance mapped from 0.1 to 1000 cd/m2
    b.readouts[ "light" ] = clampd( ( std::log10( std::max( lum_, 1e-3 ) ) + 1.0 ) / 4.0, 0, 1 );
    b.readouts[ "luminance_cdm2" ] = lum_;
    b.readouts[ "retinal_hz" ] = v.retinal_hz;
    b.readouts[ "retinal_td" ] = eye_.illuminance_td;
    b.readouts[ "sphincter" ] = eye_.sphincter_act;
    b.readouts[ "dilator" ] = eye_.dilator_act;
    b.readouts[ "sphincter_hz" ] = std::max( 0.0, sph_ ) * kSphRef;
    b.readouts[ "dilator_hz" ] = std::max( 0.0, dil_ ) * kDilRef;
    b.readouts[ "threat" ] = ( t_ >= kThreatOn && t_ < kThreatOff ) ? 1.0 : 0.0;
    b.readouts[ "t_ms" ] = t_;
    return b;
  }

  Score score() const override
  {
    Score s;
    double dim = avg( 3000, 4000 ), bright = avg( 9500, 11000 ), dim2 = avg( 18500, 20000 ), med = avg( 24500, 26000 ), fr = avg( 32500, 34000 );
    double con = dim - bright, red = dim2 - bright, fright = fr - med;
    double lat_c = latency( kBrightOn, dim, bright ), lat_r = latency( kBrightOff, bright, dim2 );
    s.parts[ "dim_mm" ] = dim;
    s.parts[ "bright_mm" ] = bright;
    s.parts[ "dim2_mm" ] = dim2;
    s.parts[ "constriction_mm" ] = con;
    s.parts[ "redilation_mm" ] = red;
    s.parts[ "constriction_latency_ms" ] = lat_c;
    s.parts[ "redilation_latency_ms" ] = lat_r;
    s.parts[ "fright_dilation_mm" ] = fright;
    s.parts[ "sphincter_spikes" ] = spikes_s_;
    s.parts[ "dilator_spikes" ] = spikes_d_;
    double c_amp = clampd( con / 2.5, 0, 1 ) * ( bright <= 4.5 ? 1.0 : 0.5 );
    double r_amp = clampd( red / 2.5, 0, 1 ) * ( dim2 >= 6.0 ? 1.0 : 0.5 );
    double c_lat = ( lat_c > 0 && con > 0.5 ) ? soft( lat_c, 150, 350, 250 ) : 0.0;
    double r_lat = ( lat_r > 0 && red > 0.5 ) ? soft( lat_r, 100, 2000, 1500 ) : 0.0;
    double fr_s = clampd( fright / 0.8, 0, 1 );
    s.value = 100.0 * ( c_amp + r_amp + c_lat + r_lat + fr_s ) / 5.0;
    bool done = t_ >= kDuration;
    s.passed = mech_.ok() && done && con >= amp_min() && bright <= bright_max() && red >= amp_min() && dim2 >= dark_min() && lat_c >= lat_lo() && lat_c <= lat_hi() && fright >= fright_min();
    if ( !mech_.ok() )
      s.value = std::min( s.value, mech::kNoMechanismCap );
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !mech_.ok() )
      o << mech_.missing();
    else if ( spikes_s_ < 1 && spikes_d_ < 1 )
      o << "Both motor pools are silent" << yet << " Check the reflex arc from 'retina' and the sympathetic arm.";
    else if ( t_ < kBrightOff )
      o << "The light tests are still running.";
    else if ( spikes_d_ < 1 )
      o << "The dilator never fires: no sympathetic drive, so the pupil cannot open in the dark or when frightened.";
    else if ( con < amp_min() )
      o << "The pupil does not constrict enough in bright light: " << ( spikes_s_ < 1 ? "the sphincter never fires." : "the parasympathetic arm is too weak or the dilator is not switched off." );
    else if ( bright > bright_max() )
      o << "The pupil stays too wide in bright light.";
    else if ( lat_c < 0 )
      o << "No clean constriction onset after the light step (the diameter never settled most of the way to its bright-light value).";
    else if ( lat_c < lat_lo() || lat_c > lat_hi() )
      o << ( lat_c < lat_lo() ? "The pupil constricts too early after the light step: a reflex arc has longer delays than that." : "The pupil constricts too late after the light step: shorten the delays of the arc." );
    else if ( red < amp_min() || dim2 < dark_min() )
      o << "Back in the dark the pupil does not reopen far enough: the sympathetic dilator is too weak.";
    else if ( t_ < kThreatOff )
      o << "The light reflex works; the fright cue is still to come.";
    else if ( fright < fright_min() )
      o << "The fright cue dilates the pupil too little: the sympathetic arm from 'threat' is not wired or too weak.";
    else
      o << "The pupil constricts to light with a reflex latency, reopens in the dark and dilates to fright.";
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { 0, 0, 1, 1 }; }

private:
  // the mechanism: retina -> ... -> sphincter pool, threat -> ... -> dilator pool, on separate pools
  static std::string pathways_missing( const Structure& st )
  {
    const auto sph = mech::out_pops( st, "sphincter" ), dil = mech::out_pops( st, "dilator" ), ret = mech::in_pops( st, "retina" ),
               thr = mech::in_pops( st, "threat" );
    if ( sph.empty() || dil.empty() )
      return "The pupil needs two muscles' worth of output: cells read by 'sphincter' and, separately, cells read by 'dilator'.";
    if ( mech::overlap( sph, dil ) )
      return "The sphincter and the dilator outputs read the same cells: one pool cannot both squeeze and open the pupil. Build the two pathways separately.";
    const bool para = mech::path( st, ret, sph, +1 ), sym = mech::path( st, thr, dil, +1 );
    if ( !para && !sym )
      return "Neither pathway is wired: light must reach the sphincter through the parasympathetic chain, and the threat cue must reach the dilator through the sympathetic chain.";
    if ( !para )
      return "The parasympathetic pathway is missing: nothing carries the retinal signal, by excitation, to the cells behind the sphincter output, so light cannot close the pupil.";
    if ( !sym )
      return "The sympathetic pathway is missing: nothing carries the threat cue, by excitation, to the cells behind the dilator output, so fright cannot open the pupil.";
    return "";
  }

  double amp_min() const { return mode_ == "guided" ? 2.0 : ( mode_ == "researcher" ? 2.94 : 2.5 ); }
  double bright_max() const { return mode_ == "guided" ? 5.0 : ( mode_ == "researcher" ? 4.2 : 4.5 ); }
  double dark_min() const { return mode_ == "guided" ? 5.5 : ( mode_ == "researcher" ? 6.3 : 6.0 ); }
  double lat_lo() const { return mode_ == "guided" ? 120 : ( mode_ == "researcher" ? 160 : 150 ); }
  double lat_hi() const { return mode_ == "guided" ? 400 : ( mode_ == "researcher" ? 330 : 350 ); }
  double fright_min() const { return mode_ == "guided" ? 0.64 : ( mode_ == "researcher" ? 0.94 : 0.8 ); }
  double fibres() const { return variant_ == "weak_retina" ? 3.0 : kFibres; }
  static double luminance( double t )
  {
    if ( t < kBrightOn ) return kDim;
    if ( t < kBrightOff ) return kBright;
    if ( t < kMediumOn ) return kDim;
    return kMedium;
  }
  static double soft( double v, double lo, double hi, double w )
  {
    double d = v < lo ? lo - v : ( v > hi ? v - hi : 0.0 );
    return clampd( 1.0 - d / w, 0, 1 );
  }
  double avg( double a, double b ) const
  {
    double s = 0;
    int n = 0;
    for ( size_t i = 0; i < d_.size(); ++i )
      if ( i + 1 > a && i + 1 <= b )
        s += d_[ i ], ++n;
    return n ? s / n : eye_.diameter_mm;
  }
  // Latency in ms after t0: time until D has moved 20 % of the way from its value just BEFORE t0 (mean of the preceding 200 ms) to the
  // plateau `to`, and stays past that threshold for 50 ms. A crossing under 50 ms cannot be a reflex (it is noise or drift) and is skipped.
  // -1 if there is none, or the total change is under 0.5 mm.
  double latency( double t0, double /*from*/, double to ) const
  {
    size_t i0 = static_cast< size_t >( t0 ), lo = i0 > 200 ? i0 - 200 : 0;
    if ( d_.size() <= i0 || i0 == lo )
      return -1;
    double base = 0;
    for ( size_t i = lo; i < i0; ++i )
      base += d_[ i ];
    base /= static_cast< double >( i0 - lo );
    if ( std::fabs( to - base ) < 0.5 )
      return -1;
    double thr = base + kLatFrac * ( to - base );
    auto past = [ & ]( size_t i ) { return ( to > base ) ? d_[ i ] >= thr : d_[ i ] <= thr; };
    for ( size_t i = i0 + 50; i + 50 <= d_.size(); ++i )
    {
      if ( !past( i ) )
        continue;
      // first sample of the run that starts at or before i
      size_t j = i;
      while ( j > i0 && past( j - 1 ) )
        --j;
      bool stays = true;
      for ( size_t k = j; k < j + 50 && stays; ++k )
        stays = past( k );
      if ( stays && j - i0 + 1 >= 50 )
        return static_cast< double >( j - i0 + 1 );
    }
    return -1;
  }

  double t_ = 0, lum_ = kDim, sph_ = 0, dil_ = 0, spikes_s_ = 0, spikes_d_ = 0;
  body::Eye eye_;
  Lowpass ss_, sd_;
  std::vector< double > d_;
  MotorReader motor_;
  Wiring wiring_;
  mech::Gate mech_;
};

} // namespace

std::unique_ptr< Task > make_pupil() { return std::make_unique< PupilTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( pupil, 400, "pupil.json", make_pupil ) }
