// Micro-puzzle "m_reciprocal" (concept 12: reciprocal inhibition). When a muscle is stretched, its antagonist must relax.
//
// WORLD (3200 ms; both sensors are dc, multiplied by the sensor weight (pA per unit) in the circuit)
//   "tone"      : level 1.4 all episode (a steady background drive: with the lab's 300 pA per unit it is 420 pA, just above the 375 pA rheobase,
//                 so a motoneuron wired to it fires at about 40 Hz: both muscles hold a little tone).
//   "ia_flexor" : the flexor's muscle spindle afferent: level 2.0 while the flexor is stretched, on [800 + J, 1800 + J) (J = U(0,100) ms seeded), else 0.
//   Motor "flexor" and "extensor": the two motor pools of an antagonist pair at a hinge joint. Joint angle readout = 0.6 deg per Hz of (flexor - extensor).
// SCORING (windows in model time; J is the seeded jitter)
//   baseline  [300, 700]            both pools >= 8 Hz           the muscles hold tone (else the antagonist has nothing to relax from)
//   stretch   [1000+J, 1800+J]      flexor >= baseline + 15 Hz   the stretch reflex contracts the stretched muscle
//                                   extensor <= 0.3 * baseline   (guided 0.4, researcher 0.24) the antagonist relaxes
//   recovery  [2200+J, 3200]        extensor >= 0.6 * baseline   relaxation is only for the stretch: tone returns
//   value     100 when passed, else 85 * credit of the five criteria.
// Why it works out: the Ia afferent has two branches, one excitatory onto its own motoneurons and one onto an inhibitory interneuron
// (Ia inhibitory interneuron) that inhibits the antagonist's motoneurons. The extensor has only 45 pA of drive above rheobase, so the
// inhibition needs to cancel just that.
//
// Biology: Sherrington CS (1893, 1906) "reciprocal innervation": a muscle's antagonist relaxes when it contracts; the pathway is the Ia
// inhibitory interneuron (Lloyd DPC (1946) J Neurophysiol 9:439-444).
#include "micro_b.h"

namespace spike::tasks
{
namespace
{
using namespace microb;

constexpr double kDur = 3200, kTone = 1.4, kIa = 2.0, kOn = 800, kOff = 1800;

class MReciprocal : public MicroB
{
public:
  MReciprocal() : MicroB( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_reciprocal";
    i.title = "Reciprocal inhibition";
    i.brief = "Muscles come in antagonist pairs: when the flexor contracts, the extensor has to let go, or the joint just stiffens. The "
              "spindle afferent of the stretched muscle (Ia) excites its own motoneurons and, through an inhibitory interneuron, silences "
              "its antagonist's. Here two motor pools hold a little tone (the 'tone' sensor, about 40 spikes/s in each). At about 1 s the "
              "flexor is stretched (the 'ia_flexor' afferent switches on for 1 s): the flexor pool must speed up by at least 15 spikes/s "
              "while the extensor pool drops to a third of its resting rate or less, and the extensor must come back after the stretch. "
              "Wire tone to both pools, ia_flexor to the flexor pool, and give ia_flexor a second route to the extensor that inverts its "
              "sign: through an interneuron set to inhibitory. The extensor only has a little drive above its threshold, so a little "
              "inhibition is enough.";
    i.chapter = "spinal";
    i.sensors = { "tone", "ia_flexor" };
    i.motors = { "flexor", "extensor" };
    i.duration_ms = kDur;
    i.concepts_new = { "reciprocal_inhibition" };
    i.concepts_used = { "excitation", "inhibition" };
    i.par_neurons = 3;
    i.default_model = "iaf_psc_alpha";
    i.recipe = "tone onto a flexor group and an extensor group (2 cells each; the first connection sets 300 pA per unit, which "
               "is right). ia_flexor onto the flexor group (the stretch adds 600 pA) and onto an interneuron group of 2 cells "
               "set to inhibitory, which projects to the extensor group at the default weight (-80 pA); raise it to about -160 "
               "pA if the extensor does not fall silent.";
    i.kinds = { { "tone", dc( kTone ) }, { "ia_flexor", dc( kIa ) }, { "flexor", rate() }, { "extensor", rate() } };
    i.bands = { { "extensor_vs_rest", "", 0.0, 0.3 } };
    i.timeline = { { kOn, kOff - kOn, "flexor stretched" } };
    i.concept_hint = "Reciprocal inhibition: the afferent of a stretched muscle excites that muscle and, through an inhibitory interneuron, "
                     "inhibits its antagonist, so the pair works as one. An inhibitory neuron turns a sensory excitation into a relaxation.";
    i.generic_hints = { { 1, i.concept_hint, "" },
      { 2, "The extensor needs a route from 'ia_flexor' that changes sign: a cell set to inhibitory between the sensor and the extensor pool.", "extensor" },
      { 3, "Tone goes onto a flexor group and an extensor group. ia_flexor goes onto the flexor group and onto a small interneuron group set to inhibitory, "
           "which projects onto the extensor group; make that inhibitory link stronger if the extensor does not fall silent.",
        "ia_flexor" } };
    i.note = { "Reciprocal innervation",
      "Charles Sherrington noticed in the 1890s that when a limb's flexors are excited their antagonists relax, as if the two were wired "
      "as one. The wiring is a spinal one: the Ia afferent of the stretched muscle branches onto an inhibitory interneuron that "
      "silences the antagonist's motoneurons. Without it every movement would have to fight the opposing muscle; its loss is part of spasticity.",
      "Sherrington CS (1906) The Integrative Action of the Nervous System. Yale University Press; Lloyd DPC (1946) J Neurophysiol 9:439-444" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    j_ = rng_.uniform( 0, 100 );
    base_e_ = 40;
  }

  std::map< std::string, Metric > metrics() const override
  {
    std::map< std::string, double > p;
    std::vector< Crit > c;
    std::string m;
    measure( p, c, m );
    return { { "extensor_vs_rest", { p.count( "extensor_ratio" ) ? p[ "extensor_ratio" ] : 1.0, "", true } } };
  }

protected:
  void mechanism( const mechb::Graph& g, std::vector< Crit >& out ) const override
  {
    const auto flex = g.out_pops( "flexor" ), ext = g.out_pops( "extensor" ), ia = g.in_pops( "ia_flexor" );
    mechb::PopSet motors = flex;
    motors.insert( ext.begin(), ext.end() );
    // the inhibition must go through an interneuron (a cell that is not one of the two motor pools): afferent -> inhibitory interneuron -> antagonist motoneurons
    const bool inh = g.hops( ia, ext, -1, 4, motors ) >= 1;
    bool any_ex = false;  // an inhibitory interneuron that the afferent does not drive, or a drive that is not inhibitory, to word the verdict
    for ( int x = 0; x < g.n_pops(); ++x )
      if ( !motors.count( x ) && g.edge( mechb::PopSet{ x }, ext, -1 ) )
        any_ex = true;
    std::string fail = "No inhibitory route to the antagonist: the afferent of the stretched muscle does not reach the extensor motoneurons through an inhibitory interneuron.";
    if ( any_ex )
      fail = "An inhibitory interneuron projects onto the extensor, but 'ia_flexor' does not drive it: the stretched muscle's afferent must excite that interneuron.";
    out.push_back( mech( "ia_inhibitory_interneuron", inh, fail, "Between 'ia_flexor' and the extensor pool there must be an interneuron set to inhibitory.", "extensor",
                         "Put a small interneuron group on the path from 'ia_flexor' to the extensor: ia_flexor excites it, and it is set to inhibitory onto the extensor group." ) );
    out.push_back( mech( "ia_own_motoneurons", g.exc( ia, flex, 3 ),
                         "The afferent of the stretched muscle does not excite its own motoneurons: reciprocal inhibition is the second branch of the reflex, not the only one.",
                         "The stretch reflex itself: 'ia_flexor' should excite the flexor pool.", "ia_flexor",
                         "Drag 'ia_flexor' onto the flexor group as well." ) );
  }

  bool stretching( double t ) const { return t >= kOn + j_ && t < kOff + j_; }

  void drive( Engine& e, double ) override
  {
    e.set_input( "tone", kTone );
    e.set_input( "ia_flexor", stretching( t_ ) ? kIa * ( 1.0 + 0.03 * noise() * rng_.normal() ) : 0.0 );
  }

  void update_readouts() override
  {
    const double f = hz_now( "flexor" ), x = hz_now( "extensor" );
    if ( t_ >= 700 && t_ < 710 )
      base_e_ = std::max( 5.0, hz( "extensor", 300, 700 ) );
    readouts_[ "flexor_hz" ] = f;
    readouts_[ "extensor_hz" ] = x;
    readouts_[ "angle_deg" ] = 0.6 * ( f - x );
    readouts_[ "extensor_vs_rest" ] = stretching( t_ ) && t_ > kOn + j_ + 200 ? x / base_e_ : 0.0;
  }

  void measure( std::map< std::string, double >& parts, std::vector< Crit >& crits, std::string& pass_msg ) const override
  {
    const double tl = tol();
    const double bf = hz( "flexor", 300, 700 ), be = hz( "extensor", 300, 700 );
    const double sf = hz( "flexor", 1000 + j_, 1800 + j_ ), se = hz( "extensor", 1000 + j_, 1800 + j_ );
    const double re = hz( "extensor", 2200 + j_, kDur );
    const double ratio = se / std::max( be, 1.0 );
    parts[ "flexor_rest_hz" ] = bf;
    parts[ "extensor_rest_hz" ] = be;
    parts[ "flexor_stretch_hz" ] = sf;
    parts[ "extensor_stretch_hz" ] = se;
    parts[ "extensor_recovery_hz" ] = re;
    parts[ "extensor_ratio" ] = ratio;

    Crit tone;
    tone.key = "tone";
    tone.ok = bf >= 8 && be >= 8;
    tone.soft = ramp( std::min( bf, be ), 0, 8 );
    tone.fail = std::string( "At rest the " ) + ( bf < 8 ? "flexor" : "extensor" ) + " is silent: a muscle that is silent has nothing to relax from. "
                "Wire 'tone' to both pools.";
    tone.place = "One of the pools gets no tone. 'tone' has to reach both the flexor and the extensor group.";
    tone.target = bf < 8 ? "flexor" : "extensor";
    tone.recipe = "Drag 'tone' onto a flexor group and onto an extensor group; the first connection's weight is already about right.";
    crits.push_back( tone );

    Crit up;
    up.key = "flexor_up";
    up.ok = sf >= bf + 15 / tl;
    up.soft = ramp( sf - bf, 0, 15 );
    up.fail = "The stretched flexor did not speed up: the afferent has to excite its own motoneurons. "
              "Wire 'ia_flexor' to the flexor group.";
    up.place = "The stretch reflex itself: 'ia_flexor' should excite the flexor pool.";
    up.target = "ia_flexor";
    up.recipe = "Drag 'ia_flexor' onto the flexor group with its first-connection weight.";
    crits.push_back( up );

    Crit rel;
    rel.key = "relax";
    rel.ok = ratio <= 0.3 * tl;
    rel.soft = 1.0 - ramp( ratio, 0.3, 1.0 );
    rel.fail = std::string( "During the stretch the extensor " ) + ( ratio >= 0.8 ? "does not relax at all" : "relaxes only partly" ) +
               ": the stretch must reach it through an inhibitory cell, strong enough to cancel its drive.";
    rel.place = "The antagonist: the path from 'ia_flexor' to the extensor pool must change sign, so put an inhibitory interneuron on it.";
    rel.target = "extensor";
    rel.recipe = "ia_flexor onto a small interneuron group set to inhibitory, projecting onto the extensor group; the default inhibitory weight may be too weak, so strengthen it.";
    crits.push_back( rel );

    Crit rec;
    rec.key = "recovery";
    rec.ok = re >= 0.6 * be;
    rec.soft = ramp( re, 0, 0.6 * be );
    rec.fail = "After the stretch the extensor does not come back to its resting rate: relaxation should last only as long as the stretch.";
    rec.place = "The inhibition is too persistent or the extensor lost its tone: it should follow the stretch.";
    rec.target = "extensor";
    rec.recipe = "Keep the inhibition driven only by 'ia_flexor' (not by tone), so that it ends with the stretch.";
    crits.push_back( rec );

    pass_msg = "The flexor sped up while the extensor fell quiet and then came back: reciprocal inhibition.";
  }

private:
  double j_ = 0, base_e_ = 40;
};

} // namespace

std::unique_ptr< Task > make_m_reciprocal() { return std::make_unique< MReciprocal >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_reciprocal, 99, "m_reciprocal.json", make_m_reciprocal ) }
