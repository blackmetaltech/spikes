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
           "return to dark must open it again, and a fright cue must dilate it.";
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
  int par_neurons() const override { return 30; }
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
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, sensor_channels(), motor_channels() );
    lum_ = luminance( t_ );
    bool threat = t_ >= kThreatOn && t_ < kThreatOff;
    e.set_input( "retina", kFibres * eye_.retinal_hz );
    e.set_input( "threat", threat ? kThreatHz : 0.0 );
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
    s.passed = done && con >= 2.5 && bright <= 4.5 && red >= 2.5 && dim2 >= 6.0 && lat_c >= 150 && lat_c <= 350 && fright >= 0.8;
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( spikes_s_ < 1 && spikes_d_ < 1 )
      o << "Both motor pools are silent" << yet << " Check the reflex arc from 'retina' and the sympathetic arm.";
    else if ( t_ < kBrightOff )
      o << "Pupil " << eye_.diameter_mm << " mm; the light tests are still running.";
    else if ( spikes_d_ < 1 )
      o << "The dilator never fires: no sympathetic drive, so the pupil cannot open in the dark or when frightened.";
    else if ( con < 2.5 )
      o << "The pupil only constricted " << con << " mm in bright light (need 2.5): " << ( spikes_s_ < 1 ? "the sphincter never fires." : "the parasympathetic arm is too weak or the dilator is not switched off." );
    else if ( bright > 4.5 )
      o << "Bright-light pupil is " << bright << " mm (needs 4.5 or less).";
    else if ( lat_c < 0 )
      o << "No clean constriction onset after the light step (the diameter never settled past 20 % of the way to its bright-light value).";
    else if ( lat_c < 150 || lat_c > 350 )
      o << "Constriction latency " << lat_c << " ms is outside 150-350 ms (human: 200-250 ms).";
    else if ( red < 2.5 || dim2 < 6.0 )
      o << "Back in the dark the pupil only reopened to " << dim2 << " mm: the sympathetic dilator is too weak.";
    else if ( t_ < kThreatOff )
      o << "Light reflex works (" << con << " mm in " << lat_c << " ms, reopens to " << dim2 << " mm); the fright cue is still to come.";
    else if ( fright < 0.8 )
      o << "The fright cue dilated the pupil only " << fright << " mm (need 0.8): the sympathetic arm from 'threat' is not wired or too weak.";
    else
      o << "Constricts " << con << " mm in " << lat_c << " ms, reopens " << red << " mm, fright adds " << fright << " mm.";
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { 0, 0, 1, 1 }; }

private:
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
};

} // namespace

std::unique_ptr< Task > make_pupil() { return std::make_unique< PupilTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( pupil, 400, "pupil.json", make_pupil ) }
