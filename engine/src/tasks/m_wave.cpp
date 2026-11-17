// Micro-puzzle "m_wave" (concept 18: travelling waves and polarity). A bolus in the gut must make the gut above it squeeze and the gut below it relax.
//
// WORLD (3200 ms; all three sensors are dc, multiplied by the sensor weight (pA per unit) in the circuit)
//   A short tube of three segments, 0 (oral), 1 and 2 (aboral). "tone": level 1.4 all episode (420 pA at 300 pA per unit: a cell wired to it
//   fires at about 40 spikes/s, so each segment holds some tone). A bolus presses on segment 1 on [700 + J, 1500 + J) (sensor "dist1", level 2.0), then
//   on segment 2 on [1700 + J, 2500 + J) (sensor "dist2", level 2.0), J = U(0,100) ms seeded; it is pushed along the tube by what the gut does.
//   Motor "seg0", "seg1", "seg2": the three segments' motor pools (more spikes = more squeeze).
// SCORING (windows in model time)
//   tone       [300, 650]: all three segments >= 8 spikes/s
//   ascending  bolus at 1, window [900+J, 1500+J]: seg0 >= its tone + 15 spikes/s (the segment above contracts)
//   descending bolus at 1, same window: seg2 <= 0.3 * its tone (the segment below relaxes)
//   travel     bolus at 2, window [1900+J, 2500+J]: seg1 >= its tone + 15 (the wave moved down one segment) and seg0 <= its tone + 8 (not still contracting)
//   recovery   [2700+J, 3200]: every segment back within 0.6 .. 1.35 of its tone
//   value 100 when passed, else 85 * credit of the five.
// Why it works out: the same sensor must drive the segment above (excitation, ascending) and, through an inhibitory interneuron, the segment
// below (inhibition, descending): contraction behind the bolus, relaxation ahead of it, so the bolus moves aboral.
//
// Biology: Bayliss WM, Starling EH (1899) J Physiol 24:99-143 "the law of the intestine": distension excites a contraction above the stimulated point and
// an inhibition below it; the enteric nervous system uses ascending excitatory (cholinergic) and descending inhibitory (nitrergic) interneurons.
#include "micro_b.h"

namespace spike::tasks
{
namespace
{
using namespace microb;

constexpr double kDur = 3200, kTone = 1.4, kPress = 2.0;
const char* const kSeg[ 3 ] = { "seg0", "seg1", "seg2" };

class MWave : public MicroB
{
public:
  MWave() : MicroB( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_wave";
    i.title = "Travelling wave";
    i.brief = "Food moves along the gut because the gut squeezes behind a bolus and relaxes in front of it: excitation runs up the tube (oral) and "
              "inhibition runs down (aboral), so the push always points the same way. Here a short tube has three segments, seg0 (oral) to seg2 (aboral). "
              "Each holds a little tone (the 'tone' sensor, about 40 spikes/s). A bolus presses on segment 1 (sensor 'dist1') and later on segment 2 "
              "('dist2'). While it presses on segment 1, seg0 must contract (at least 15 spikes/s above its tone) and seg2 must relax (to a third of its tone "
              "or less); while it presses on segment 2 the wave has moved on: seg1 contracts and seg0 is no longer contracting. At the end every segment "
              "must be back to its tone. Wire tone to all three segments, dist1 to seg0, dist2 to seg1, and give dist1 a second route to seg2 that "
              "changes sign: through a cell set to inhibitory. Get the polarity right: contracting the segment below the bolus would squeeze it back.";
    i.chapter = "autonomic";
    i.sensors = { "tone", "dist1", "dist2" };
    i.motors = { "seg0", "seg1", "seg2" };
    i.duration_ms = kDur;
    i.concepts_new = { "travelling_wave" };
    i.concepts_used = { "excitation", "inhibition" };
    i.par_neurons = 4;
    i.kinds = { { "tone", dc( kTone ) }, { "dist1", dc( kPress ) }, { "dist2", dc( kPress ) }, { "seg0", rate() }, { "seg1", rate() }, { "seg2", rate() } };
    i.bands = { { "seg2_vs_tone", "", 0.0, 0.3 } };
    i.timeline = { { 700, 800, "bolus at segment 1" }, { 1700, 800, "bolus at segment 2" } };
    i.concept_hint = "Polarity: a travelling wave has a direction because excitation spreads one way (above the bolus) and inhibition the other (below it). The same "
                     "stimulus is wired with opposite signs to its two neighbours.";
    i.generic_hints = { { 1, i.concept_hint, "" },
      { 2, "The segment below the bolus must be relaxed by the stimulus: look at seg2 (it needs an inhibitory route from dist1).", "seg2" },
      { 3, "tone onto 3 groups (seg0, seg1, seg2). dist1 onto seg0 and onto an inhibitory group (2 cells) that projects onto seg2 at about -160 pA; "
           "dist2 onto seg1.", "dist1" } };
    i.note = { "The law of the intestine",
      "In 1899 Bayliss and Starling pushed a bolus into a dog's gut and saw that the gut contracted above it and relaxed below it. "
      "They called it the law of the intestine; today we know the circuit: ascending cholinergic excitatory and descending nitrergic inhibitory "
      "interneurons of the myenteric plexus, driven by stretch-sensitive afferents. Lose the descending inhibitor and the bolus meets a "
      "contracted segment ahead of it, as in achalasia and Hirschsprung disease.",
      "Bayliss WM, Starling EH (1899) The movements and innervation of the small intestine. J Physiol 24:99-143" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    j_ = rng_.uniform( 0, 100 );
    base_[ 0 ] = base_[ 1 ] = base_[ 2 ] = 40;
  }

  std::map< std::string, Metric > metrics() const override
  {
    std::map< std::string, double > p;
    std::vector< Crit > c;
    std::string m;
    measure( p, c, m );
    return { { "seg2_vs_tone", { p.count( "seg2_ratio" ) ? p[ "seg2_ratio" ] : 1.0, "", true } } };
  }

protected:
  bool at1( double t ) const { return t >= 700 + j_ && t < 1500 + j_; }
  bool at2( double t ) const { return t >= 1700 + j_ && t < 2500 + j_; }

  void drive( Engine& e, double ) override
  {
    e.set_input( "tone", kTone );
    e.set_input( "dist1", at1( t_ ) ? kPress * ( 1.0 + 0.03 * noise() * rng_.normal() ) : 0.0 );
    e.set_input( "dist2", at2( t_ ) ? kPress * ( 1.0 + 0.03 * noise() * rng_.normal() ) : 0.0 );
  }

  void update_readouts() override
  {
    if ( t_ >= 650 && t_ < 660 )
      for ( int i = 0; i < 3; ++i )
        base_[ i ] = std::max( 5.0, hz( kSeg[ i ], 300, 650 ) );
    for ( int i = 0; i < 3; ++i )
      readouts_[ std::string( kSeg[ i ] ) + "_hz" ] = hz_now( kSeg[ i ] );
    readouts_[ "bolus_pos" ] = at1( t_ ) ? 1.0 : ( at2( t_ ) ? 2.0 : 0.0 );
    readouts_[ "seg2_vs_tone" ] = at1( t_ ) && t_ > 700 + j_ + 200 ? hz_now( "seg2" ) / base_[ 2 ] : 0.0;
  }

  void measure( std::map< std::string, double >& parts, std::vector< Crit >& crits, std::string& pass_msg ) const override
  {
    const double tl = tol();
    double b[ 3 ], w1[ 3 ], w2[ 3 ], rc[ 3 ];
    for ( int i = 0; i < 3; ++i )
    {
      b[ i ] = hz( kSeg[ i ], 300, 650 );
      w1[ i ] = hz( kSeg[ i ], 900 + j_, 1500 + j_ );
      w2[ i ] = hz( kSeg[ i ], 1900 + j_, 2500 + j_ );
      rc[ i ] = hz( kSeg[ i ], 2700 + j_, kDur );
      parts[ std::string( kSeg[ i ] ) + "_tone_hz" ] = b[ i ];
      parts[ std::string( kSeg[ i ] ) + "_bolus1_hz" ] = w1[ i ];
      parts[ std::string( kSeg[ i ] ) + "_bolus2_hz" ] = w2[ i ];
    }
    parts[ "seg2_ratio" ] = w1[ 2 ] / std::max( b[ 2 ], 1.0 );

    Crit tone;
    tone.key = "tone";
    tone.ok = b[ 0 ] >= 8 && b[ 1 ] >= 8 && b[ 2 ] >= 8;
    tone.soft = ramp( std::min( b[ 0 ], std::min( b[ 1 ], b[ 2 ] ) ), 0, 8 );
    int silent = b[ 0 ] < 8 ? 0 : ( b[ 1 ] < 8 ? 1 : 2 );
    tone.fail = "Segment " + std::to_string( silent ) + " is silent at rest (" + fmt( b[ silent ] ) + " spikes/s): a segment with no tone has nothing to relax from. Wire 'tone' to all three segments.";
    tone.place = "One of the segments gets no tone.";
    tone.target = kSeg[ silent ];
    tone.recipe = "Drag 'tone' onto a group for each segment (the first connection sets the weight to 300 pA per unit, which is right).";
    crits.push_back( tone );

    Crit asc;
    asc.key = "ascending";
    asc.ok = w1[ 0 ] >= b[ 0 ] + 15 / tl;
    asc.soft = ramp( w1[ 0 ] - b[ 0 ], 0, 15 );
    asc.fail = "With the bolus on segment 1, seg0 (above it) does not contract: " + fmt( b[ 0 ] ) + " to " + fmt( w1[ 0 ] ) + " spikes/s. Excitation must run up the tube: wire dist1 to seg0.";
    asc.place = "The segment above the bolus: seg0 has to be excited by dist1.";
    asc.target = "seg0";
    asc.recipe = "Drag 'dist1' onto the seg0 group (weight 300 pA per unit adds 600 pA).";
    crits.push_back( asc );

    Crit desc;
    desc.key = "descending";
    desc.ok = w1[ 2 ] <= 0.3 * tl * std::max( b[ 2 ], 1.0 );
    desc.soft = 1.0 - ramp( w1[ 2 ] / std::max( b[ 2 ], 1.0 ), 0.3, 1.0 );
    desc.fail = "With the bolus on segment 1, seg2 (below it) fires " + fmt( w1[ 2 ] ) + " spikes/s against " + fmt( b[ 2 ] ) + " at rest: it must relax. Inhibition must run down the "
                "tube: dist1 onto an inhibitory cell that projects onto seg2.";
    desc.place = "The segment below the bolus: seg2 needs an inhibitory route from dist1.";
    desc.target = "seg2";
    desc.recipe = "dist1 onto an inhibitory group of 2 cells, projecting onto seg2 at about -160 pA.";
    crits.push_back( desc );

    Crit trv;
    trv.key = "travel";
    const bool seg1_up = w2[ 1 ] >= b[ 1 ] + 15 / tl, seg0_off = w2[ 0 ] <= b[ 0 ] + 8 * tl;
    trv.ok = seg1_up && seg0_off;
    trv.soft = ( ramp( w2[ 1 ] - b[ 1 ], 0, 15 ) + ( seg0_off ? 1.0 : 0.0 ) ) / 2.0;
    trv.fail = !seg1_up ? "With the bolus on segment 2, seg1 (above it) does not contract (" + fmt( b[ 1 ] ) + " to " + fmt( w2[ 1 ] ) + " spikes/s): the wave must move on. Wire dist2 to seg1."
                        : "With the bolus on segment 2, seg0 still contracts (" + fmt( w2[ 0 ] ) + " spikes/s): the wave has moved on, only the segment above the bolus should squeeze.";
    trv.place = "The next step of the wave: dist2 has to excite the segment above it (seg1), not the earlier one.";
    trv.target = !seg1_up ? "seg1" : "seg0";
    trv.recipe = "dist2 onto the seg1 group only.";
    crits.push_back( trv );

    Crit rec;
    rec.key = "recovery";
    bool back = true;
    for ( int i = 0; i < 3; ++i )
      back = back && rc[ i ] >= 0.6 * b[ i ] && rc[ i ] <= 1.35 * b[ i ];
    rec.ok = back;
    rec.soft = back ? 1.0 : 0.5;
    rec.fail = "After the bolus has passed the segments are at " + fmt( rc[ 0 ] ) + " / " + fmt( rc[ 1 ] ) + " / " + fmt( rc[ 2 ] ) + " spikes/s, their tone was " + fmt( b[ 0 ] ) + " / " + fmt( b[ 1 ] ) + " / " + fmt( b[ 2 ] ) +
               ": the response must end with the stimulus.";
    rec.place = "A segment that does not return to rest: its inhibition or excitation outlasts the bolus.";
    rec.target = "seg2";
    rec.recipe = "Drive the contraction and the inhibition only from dist1 and dist2, not from tone.";
    crits.push_back( rec );

    pass_msg = "Bolus at segment 1: seg0 " + fmt( b[ 0 ] ) + " -> " + fmt( w1[ 0 ] ) + " spikes/s (contracts), seg2 " + fmt( b[ 2 ] ) + " -> " + fmt( w1[ 2 ] ) +
               " (relaxes); bolus at segment 2: seg1 " + fmt( b[ 1 ] ) + " -> " + fmt( w2[ 1 ] ) + ". Excitation up, inhibition down: the wave has a direction.";
  }

private:
  double j_ = 0, base_[ 3 ] = { 40, 40, 40 };
};

} // namespace

std::unique_ptr< Task > make_m_wave() { return std::make_unique< MWave >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_wave, 409, "m_wave.json", make_m_wave ) }
