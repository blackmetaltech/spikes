// The parts bin: static card data. Defaults of the listed parameters are checked against NEST in engine/tests/test_catalog.cpp.
#include "spike/catalog.h"

#include "nlohmann/json.hpp"

namespace spike
{
namespace
{
using P = CatalogParam;
using nlohmann::json;

const char* kIzh2003 = "Izhikevich EM (2003) Simple model of spiking neurons. IEEE Trans Neural Netw 14:1569-1572 (Fig. 1 parameters); "
                       "Izhikevich EM (2004) Which model to use for cortical spiking neurons? IEEE Trans Neural Netw 15:1063-1070";

CatalogModel izh( const char* tag, const char* title, const char* summary, std::vector< std::string > examples, double a, double b, double c,
  double d, const char* ei, const char* ei_note )
{
  CatalogModel m;
  m.id = std::string( "izhikevich_" ) + tag;
  m.model = "izhikevich";
  m.title = std::string( "Izhikevich " ) + tag + ": " + title;
  m.family = "Izhikevich (phenomenological)";
  m.summary = summary;
  m.examples = std::move( examples );
  m.source = kIzh2003;
  m.preset_json = json{ { "a", a }, { "b", b }, { "c", c }, { "d", d }, { "V_m", -65.0 }, { "U_m", b * -65.0 } }.dump();
  m.params = { P{ "a", "1/ms", "Recovery speed: how fast the slow variable follows the voltage (small = slow).", a, 0.01, 0.1 },
    P{ "b", "", "Sensitivity of the recovery variable to sub-threshold voltage (resonance).", b, 0.15, 0.3 },
    P{ "c", "mV", "Voltage the cell is reset to after a spike.", c, -70, -45 },
    P{ "d", "", "Extra recovery kick added after each spike (large = strong adaptation or bursting).", d, 0.0, 10 },
    P{ "I_e", "pA", "Constant input current (the model's own units, about 1 per mV/ms).", 0, -10, 40 } };
  m.input_weight = 2.0;  // measured: one spike steps V_m by exactly the weight (NEST izhikevich), so 2 = a 2 mV EPSP
  m.weight_unit = "mV";
  m.ei = ei;
  m.ei_note = ei_note;
  m.drive = { "dc", "pA", { 0, 2, 4, 6, 10, 15 } };
  return m;
}

std::vector< CatalogModel > build()
{
  std::vector< CatalogModel > v;
  auto add = [ & ]( CatalogModel m ) { v.push_back( std::move( m ) ); };

  {
    CatalogModel m;
    m.id = m.model = "iaf_psc_alpha";
    m.title = "Leaky integrate-and-fire (current synapses)";
    m.family = "Integrate-and-fire";
    m.summary = "The simplest spiking cell: the membrane charges like a leaky capacitor, and when it crosses threshold it fires and resets. "
                "Synaptic inputs are currents with an alpha-function shape. Fires regularly with no adaptation; its rate grows smoothly "
                "with input after a threshold current (class 1).";
    m.examples = { "Generic cortical neuron in large network models (Brunel 2000 balanced network)", "Quick stand-in for any neuron whose details do not matter" };
    m.source = "Rotter & Diesmann 1999 Biol Cybern 81:381-402 (exact integration); Brunel 2000 J Comput Neurosci 8:183-208";
    m.params = { P{ "tau_m", "ms", "Membrane time constant: how slowly the cell forgets its input.", 10, 2, 100 },
      P{ "C_m", "pF", "Membrane capacitance (sets how much current moves the voltage).", 250, 50, 1000 },
      P{ "V_th", "mV", "Spike threshold.", -55, -60, -40 }, P{ "V_reset", "mV", "Voltage right after a spike.", -70, -80, -55 },
      P{ "t_ref", "ms", "Refractory period (dead time after a spike).", 2, 0, 10 },
      P{ "tau_syn_ex", "ms", "Excitatory synaptic current time constant.", 2, 0.1, 20 },
      P{ "tau_syn_in", "ms", "Inhibitory synaptic current time constant.", 2, 0.1, 20 },
      P{ "I_e", "pA", "Constant input current (rheobase at defaults is 375 pA).", 0, -500, 1000 } };
    m.input_weight = 80;
    m.weight_unit = "pA";
    m.ei = "either";
    m.ei_note = "Generic: use positive weights for an excitatory cell and negative for an inhibitory one.";
    m.drive = { "dc", "pA", { 0, 380, 450, 600, 800, 1000 } };
    add( m );
  }
  {
    CatalogModel m;
    m.id = m.model = "iaf_cond_exp";
    m.title = "Integrate-and-fire with conductance synapses";
    m.family = "Integrate-and-fire";
    m.summary = "Like the leaky integrate-and-fire cell, but synapses open ion channels (conductances) with exponential decay, so an input's effect "
                "depends on the membrane voltage and shunts the cell: inhibition near rest mostly clamps the voltage rather than pushing it down.";
    m.examples = { "Cortical cells in conductance-based benchmark networks (Brette et al. 2007)", "Neurons in a high-conductance in-vivo state" };
    m.source = "Meffin et al. 2004 J Neurophysiol 92:2064-2086; Brette et al. 2007 J Comput Neurosci 23:349-398";
    m.params = { P{ "g_L", "nS", "Leak conductance (membrane time constant = C_m / g_L).", 16.6667, 5, 100 },
      P{ "C_m", "pF", "Membrane capacitance.", 250, 50, 1000 }, P{ "V_th", "mV", "Spike threshold.", -55, -60, -40 },
      P{ "V_reset", "mV", "Voltage right after a spike.", -60, -80, -55 }, P{ "t_ref", "ms", "Refractory period.", 2, 0, 10 },
      P{ "E_ex", "mV", "Excitatory reversal potential.", 0, -20, 20 }, P{ "E_in", "mV", "Inhibitory reversal potential.", -85, -100, -65 },
      P{ "tau_syn_ex", "ms", "Excitatory conductance decay.", 0.2, 0.1, 20 }, P{ "tau_syn_in", "ms", "Inhibitory conductance decay.", 2, 0.1, 20 },
      P{ "I_e", "pA", "Constant input current.", 0, -500, 1000 } };
    m.input_weight = 20;
    m.weight_unit = "nS";
    m.ei = "either";
    m.ei_note = "Positive weight opens the excitatory channel, negative the inhibitory one.";
    m.drive = { "dc", "pA", { 0, 260, 300, 400, 600, 800 } };
    add( m );
  }
  {
    CatalogModel m;
    m.id = m.model = "iaf_cond_exp_sfa_rr";
    m.title = "Adapting integrate-and-fire (SFA and relative refractoriness)";
    m.family = "Integrate-and-fire";
    m.summary = "A conductance-based integrate-and-fire cell with two spike-triggered potassium conductances: a slow one that makes firing slow down "
                "during a sustained input (spike-frequency adaptation) and a fast one that makes it harder to fire right after a spike.";
    m.examples = { "Regular-spiking neocortical pyramidal cells (adaptation over 100 ms)", "Visual cortex cells with spike-frequency adaptation" };
    m.source = "Meffin et al. 2004 J Neurophysiol 92:2064-2086; Dayan & Abbott 2001 Theoretical Neuroscience, ch. 5";
    m.params = { P{ "q_sfa", "nS", "Adaptation conductance added at each spike (0 = no adaptation).", 14.48, 0, 100 },
      P{ "tau_sfa", "ms", "Adaptation decay time constant.", 110, 20, 1000 },
      P{ "q_rr", "nS", "Fast refractory conductance added at each spike.", 3214, 0, 10000 },
      P{ "tau_rr", "ms", "Refractory conductance decay time constant.", 1.97, 0.5, 20 }, P{ "g_L", "nS", "Leak conductance.", 28.95, 5, 100 },
      P{ "C_m", "pF", "Membrane capacitance.", 289.5, 50, 1000 }, P{ "V_th", "mV", "Spike threshold.", -57, -60, -40 },
      P{ "I_e", "pA", "Constant input current.", 0, -500, 3000 } };
    m.input_weight = 4;
    m.weight_unit = "nS";
    m.ei = "excitatory";
    m.ei_note = "Adapting cells are typical pyramidal (excitatory) neurons.";
    m.drive = { "dc", "pA", { 0, 400, 600, 1000, 1500, 2000 } };
    add( m );
  }
  for ( const char* nm : { "aeif_cond_alpha", "aeif_cond_exp" } )
  {
    const bool alpha = std::string( nm ) == "aeif_cond_alpha";
    CatalogModel m;
    m.id = m.model = nm;
    m.title = alpha ? "Adaptive exponential I&F (alpha conductances)" : "Adaptive exponential I&F (exponential conductances)";
    m.family = "Adaptive exponential";
    m.summary = "Integrate-and-fire with a realistic exponential spike upswing and a slow adaptation current w. Depending on a, b and the reset it "
                "can fire regularly, adapt, burst or fire irregularly, so one equation covers many cortical firing patterns. ";
    m.summary += alpha ? "Synaptic conductances have an alpha-function time course." : "Synaptic conductances decay exponentially.";
    m.examples = { "Regular-spiking and bursting neocortical pyramidal neurons (Naud et al. 2008 fits)", "Adapting cells in network models with hundreds of cells" };
    m.source = "Brette & Gerstner 2005 J Neurophysiol 94:3637-3642; Naud et al. 2008 Biol Cybern 99:335-347";
    m.params = { P{ "a", "nS", "Sub-threshold adaptation: how strongly w follows the voltage.", 4, 0, 20 },
      P{ "b", "pA", "Spike-triggered adaptation jump of w.", 80.5, 0, 200 }, P{ "tau_w", "ms", "Adaptation time constant.", 144, 10, 1000 },
      P{ "Delta_T", "mV", "Sharpness of the spike upswing (small = sharp threshold).", 2, 0.5, 10 }, P{ "V_th", "mV", "Exponential threshold.", -50.4, -60, -40 },
      P{ "V_reset", "mV", "Reset voltage after a spike.", -60, -80, -45 }, P{ "g_L", "nS", "Leak conductance.", 30, 5, 100 },
      P{ "C_m", "pF", "Membrane capacitance.", 281, 50, 1000 }, P{ "I_e", "pA", "Constant input current.", 0, -500, 3000 } };
    m.input_weight = alpha ? 8 : 10;
    m.weight_unit = "nS";
    m.ei = "either";
    m.ei_note = "Adapting by default (excitatory-like); set a = 0, b = 0 for a fast-spiking inhibitory-like cell.";
    m.drive = { "dc", "pA", { 0, 700, 800, 1000, 1500, 2000 } };
    add( m );
  }

  add( izh( "RS", "regular spiking", "Fires a train that slows down during a steady input (adaptation). The classic neocortical excitatory cell.",
    { "Layer 2/3 and 5 neocortical pyramidal neurons" }, 0.02, 0.2, -65, 8, "excitatory", "The standard excitatory cortical cell." ) );
  add( izh( "IB", "intrinsically bursting", "Starts with a burst of spikes, then settles to regular spiking.",
    { "Layer 5 neocortical pyramidal neurons that burst", "Subiculum bursting cells" }, 0.02, 0.2, -55, 4, "excitatory", "Excitatory projection cells." ) );
  add( izh( "CH", "chattering", "Fires stereotyped high-frequency bursts of 3 to 4 spikes with short pauses between them.",
    { "Layer 2/3 and 4 'chattering' neocortical cells (gamma-band bursting)" }, 0.02, 0.2, -50, 2, "excitatory", "Excitatory cells that drive fast rhythms." ) );
  add( izh( "FS", "fast spiking", "Fires fast with no adaptation, and recovers quickly after every spike.",
    { "Parvalbumin basket cells in neocortex and hippocampus", "Fast-spiking inhibitory interneurons" }, 0.1, 0.2, -65, 2, "inhibitory", "Use with negative weights: these are inhibitory interneurons." ) );
  add( izh( "LTS", "low-threshold spiking", "Fires at low input levels and adapts moderately; has a rebound tendency.",
    { "Somatostatin (Martinotti) interneurons", "Low-threshold spiking cortical interneurons" }, 0.02, 0.25, -65, 2, "inhibitory", "Inhibitory interneuron." ) );
  add( izh( "TC", "thalamo-cortical", "Tonic spiking at rest like regular spiking. The full thalamo-cortical burst mode when hyperpolarised needs a negative offset "
    "current first (about -5) and then a step.", { "Thalamic relay neurons in wake (tonic) and sleep (burst) states" }, 0.02, 0.25, -65, 0.05, "excitatory",
    "Excitatory relay cells projecting to cortex." ) );
  add( izh( "RZ", "resonator", "Responds best to inputs at its own preferred frequency (sub-threshold oscillations); fires fast with a small threshold current.",
    { "Cortical neurons with sub-threshold resonance", "Stellate cells of entorhinal cortex (theta resonance)" }, 0.1, 0.26, -65, 2, "either",
    "Frequency-tuned cell; either sign works." ) );

  {
    CatalogModel m;
    m.id = m.model = "hh_psc_alpha";
    m.title = "Hodgkin-Huxley neuron (squid axon)";
    m.family = "Hodgkin-Huxley";
    m.summary = "The original conductance model of the action potential: voltage-gated sodium and potassium channels plus a leak, with real channel "
                "kinetics. Silent below a sharp threshold, then jumps straight to fast repetitive firing (class 2), and it can fire a rebound spike "
                "after a hyperpolarising pulse.";
    m.examples = { "Squid giant axon (Hodgkin & Huxley 1952)", "Teaching model for excitability, the action potential and channel blockers" };
    m.source = "Hodgkin & Huxley 1952 J Physiol 117:500-544; NEST hh_psc_alpha (current-based synapses)";
    m.params = { P{ "g_Na", "nS", "Maximum sodium conductance (spike upstroke).", 12000, 0, 20000 }, P{ "g_K", "nS", "Maximum potassium conductance (repolarisation).", 3600, 0, 8000 },
      P{ "g_L", "nS", "Leak conductance.", 30, 5, 100 }, P{ "E_Na", "mV", "Sodium reversal potential.", 50, 30, 70 }, P{ "E_K", "mV", "Potassium reversal potential.", -77, -100, -60 },
      P{ "C_m", "pF", "Membrane capacitance.", 100, 20, 500 }, P{ "t_ref", "ms", "Refractory period used to count spikes.", 2, 0, 10 },
      P{ "I_e", "pA", "Constant input current.", 0, -500, 3000 } };
    m.input_weight = 250;
    m.weight_unit = "pA";
    m.ei = "either";
    m.ei_note = "Biophysical excitable cell; sign of the weight sets E or I.";
    m.drive = { "dc", "pA", { 0, 700, 800, 1000, 1500, 2000 } };
    add( m );
  }
  {
    CatalogModel m;
    m.id = m.model = "hh_cond_exp_traub";
    m.title = "Hodgkin-Huxley neuron (Traub, conductance synapses)";
    m.family = "Hodgkin-Huxley";
    m.summary = "A Traub-Miles Hodgkin-Huxley cell with fast sodium, delayed-rectifier potassium and exponential conductance synapses. "
                "The card sets V_T = -50 mV so it is quiet at rest (NEST's default of -63 mV fires spontaneously at about 14 Hz).";
    m.examples = { "Hippocampal CA3 pyramidal cells (Traub & Miles 1991)", "Synchrony and oscillation models (Brette et al. 2007 benchmark 3)" };
    m.source = "Traub & Miles 1991 Neuronal Networks of the Hippocampus, Cambridge UP; Brette et al. 2007 J Comput Neurosci 23:349-398";
    m.preset_json = json{ { "V_T", -50.0 } }.dump();
    m.params = { P{ "V_T", "mV", "Voltage offset of the channel kinetics (lower = more excitable).", -50, -65, -45 },
      P{ "g_Na", "nS", "Maximum sodium conductance.", 20000, 5000, 30000 }, P{ "g_K", "nS", "Maximum potassium conductance.", 6000, 1000, 10000 },
      P{ "g_L", "nS", "Leak conductance.", 10, 2, 50 }, P{ "C_m", "pF", "Membrane capacitance.", 200, 50, 500 },
      P{ "tau_syn_ex", "ms", "Excitatory conductance decay.", 5, 0.2, 20 }, P{ "tau_syn_in", "ms", "Inhibitory conductance decay.", 10, 0.2, 50 },
      P{ "I_e", "pA", "Constant input current.", 0, -500, 2000 } };
    m.input_weight = 2;  // measured: one spike gives a 1.9 mV EPSP at rest
    m.weight_unit = "nS";
    m.ei = "either";
    m.ei_note = "Excitatory by default (pyramidal cell); negative weights give an inhibitory conductance neuron.";
    m.drive = { "dc", "pA", { 0, 100, 150, 250, 500, 1000 } };
    add( m );
  }
  {
    CatalogModel m;
    m.id = m.model = "ht_neuron";
    m.title = "Hill-Tononi thalamo-cortical neuron";
    m.family = "Multi-channel biophysical";
    m.summary = "A point neuron with four intrinsic currents (persistent sodium, depolarisation-activated potassium, low-threshold calcium T and the "
                "h-current) and AMPA, NMDA, GABA-A and GABA-B synapses on separate receptor ports. It switches between tonic and burst "
                "firing depending on state, which gives sleep-like slow oscillations in networks.";
    m.examples = { "Thalamic relay and reticular cells", "Cortical pyramidal cells in sleep and wake models (Hill & Tononi 2005)" };
    m.source = "Hill & Tononi 2005 J Neurophysiol 93:1671-1698; Hill & Tononi (NEST ht_neuron)";
    m.params = { P{ "g_peak_T", "nS", "Low-threshold calcium current strength (burst mode).", 1, 0, 5 },
      P{ "g_peak_h", "nS", "h-current strength (rhythmicity and rebound).", 1, 0, 5 }, P{ "g_peak_NaP", "nS", "Persistent sodium strength.", 1, 0, 5 },
      P{ "g_peak_KNa", "nS", "Sodium-dependent potassium (slow adaptation).", 1, 0, 5 }, P{ "g_KL", "nS", "Potassium leak (sets resting level).", 1, 0.5, 2 },
      P{ "tau_m", "ms", "Membrane time constant.", 16, 5, 50 }, P{ "theta_eq", "mV", "Resting spike threshold.", -51, -60, -40 } };
    m.input_weight = 1;  // measured: weight 1 on the AMPA port (receptor_type 1) gives a 1 mV EPSP; the weight multiplies the peak conductance
    m.weight_unit = "x g_peak";
    m.ei = "either";
    m.ei_note = "Synapses are on receptor ports (AMPA = 1); weight scales the peak conductance. Use receptor_type for inhibition.";
    m.drive = { "dc", "pA", { 0, 10, 20, 30, 50, 100 } };
    add( m );
  }
  {
    CatalogModel m;
    m.id = m.model = "gif_cond_exp";
    m.title = "Generalised integrate-and-fire (GIF)";
    m.family = "Stochastic / fitted";
    m.summary = "An integrate-and-fire cell with a fuzzy threshold: the chance of firing rises exponentially as the voltage exceeds the threshold, "
                "so spike times are irregular. Spike-triggered currents and a moving threshold can be fitted to reproduce recorded cells.";
    m.examples = { "Cortical pyramidal cells fitted from patch-clamp data (Mensi et al. 2012)", "Layer 5 neurons in network models with realistic variability" };
    m.source = "Mensi et al. 2012 J Neurophysiol 107:1756-1775; Pozzorini et al. 2015 PLoS Comput Biol 11:e1004275";
    m.params = { P{ "lambda_0", "Hz", "Firing probability density at threshold.", 1, 0.1, 100 }, P{ "Delta_V", "mV", "Threshold softness (large = noisier).", 0.5, 0.1, 5 },
      P{ "V_T_star", "mV", "Threshold voltage.", -35, -60, -30 }, P{ "g_L", "nS", "Leak conductance.", 4, 1, 50 }, P{ "C_m", "pF", "Membrane capacitance.", 80, 20, 500 },
      P{ "V_reset", "mV", "Reset voltage after a spike.", -55, -80, -45 }, P{ "t_ref", "ms", "Absolute refractory period.", 4, 0, 20 }, P{ "I_e", "pA", "Constant input current.", 0, -500, 1000 } };
    m.input_weight = 3;
    m.weight_unit = "nS";
    m.ei = "either";
    m.ei_note = "Fitted to excitatory cells in the literature; either sign works.";
    m.drive = { "dc", "pA", { 0, 100, 150, 200, 400, 800 } };
    add( m );
  }
  {
    CatalogModel m;
    m.id = m.model = "mat2_psc_exp";
    m.title = "Multi-timescale adaptive threshold (MAT)";
    m.family = "Integrate-and-fire";
    m.summary = "A leaky integrate-and-fire cell whose threshold jumps at every spike and relaxes back with two time constants (10 and 200 ms). "
                "This gives adaptation, bursts and spike-time prediction that matches recordings, with very cheap computation.";
    m.examples = { "Cortical pyramidal cells in vivo (Rauch et al. 2003)", "INCF spike time prediction challenge winners" };
    m.source = "Kobayashi, Tsubo & Shinomoto 2009 Front Comput Neurosci 3:9";
    m.params = { P{ "omega", "mV", "Resting threshold.", -51, -60, -40 }, P{ "alpha_1", "mV", "Fast threshold jump per spike.", 37, 0, 100 },
      P{ "alpha_2", "mV", "Slow threshold jump per spike.", 2, 0, 20 }, P{ "tau_1", "ms", "Fast threshold decay.", 10, 2, 50 }, P{ "tau_2", "ms", "Slow threshold decay.", 200, 50, 1000 },
      P{ "tau_m", "ms", "Membrane time constant.", 5, 1, 50 }, P{ "C_m", "pF", "Membrane capacitance.", 100, 20, 500 }, P{ "I_e", "pA", "Constant input current.", 0, -500, 3000 } };
    m.input_weight = 150;
    m.weight_unit = "pA";
    m.ei = "either";
    m.ei_note = "Generic; the sign of the weight sets E or I.";
    m.drive = { "dc", "pA", { 0, 400, 600, 800, 1000, 1500 } };
    add( m );
  }
  {
    CatalogModel m;
    m.id = m.model = "pp_psc_delta";
    m.title = "Stochastic point-process neuron";
    m.family = "Stochastic / fitted";
    m.summary = "Fires randomly with a rate that grows exponentially with the membrane potential (escape noise), followed by a dead time. "
                "Spike trains look Poisson-like and irregular, like cells in vivo, and the synaptic input is an instantaneous voltage jump.";
    m.examples = { "Irregularly firing cortical neurons in vivo", "Noisy sensory afferents", "Stochastic source of spikes for rate-code experiments" };
    m.source = "Jolivet, Rauch, Luscher & Gerstner 2006 J Comput Neurosci 21:35-49; Gerstner & Kistler 2002 Spiking Neuron Models, ch. 5";
    m.params = { P{ "c_1", "1/(s mV)", "Linear coefficient of the rate-voltage relation.", 0, 0, 100 }, P{ "c_2", "1/s", "Base rate coefficient.", 1.238, 0.1, 50 },
      P{ "c_3", "1/mV", "Exponential steepness (large = sharper).", 0.25, 0.05, 1 }, P{ "dead_time", "ms", "Silent period after each spike.", 1, 0, 20 },
      P{ "tau_m", "ms", "Membrane time constant.", 10, 2, 100 }, P{ "C_m", "pF", "Membrane capacitance.", 250, 50, 1000 }, P{ "I_e", "pA", "Constant input current.", 0, -500, 3000 } };
    m.input_weight = 1;  // instantaneous voltage jump: the weight is the EPSP in mV
    m.weight_unit = "mV";
    m.ei = "either";
    m.ei_note = "Input weight is a voltage jump in mV; sign sets E or I.";
    m.drive = { "dc", "pA", { 0, 200, 400, 600, 800, 1000 } };
    add( m );
  }
  {
    CatalogModel m;
    m.id = m.model = "parrot_neuron";
    m.title = "Parrot (relay) neuron";
    m.family = "Relay";
    m.summary = "Repeats every spike it receives, one for one, with no membrane dynamics. Use it as a relay, to fan one input out to many targets, "
                "or to turn a stimulation device into a real neuron that can take plastic synapses.";
    m.examples = { "A sensory axon or relay station that does not transform the signal", "Primary afferents that copy receptor spikes" };
    m.source = "NEST parrot_neuron";
    m.params = {};
    m.input_weight = 1;  // a relay: it repeats every spike whatever the weight (> 0), so the weight only matters downstream
    m.weight_unit = "";
    m.ei = "either";
    m.ei_note = "Pure relay: it passes spikes on whatever the weight; the sign of the downstream synapse sets E or I.";
    m.drive = { "poisson", "Hz", { 0, 10, 20, 50, 100, 200 } };
    add( m );
  }

  {
    CatalogModel m;
    m.id = m.model = "hh_psc_alpha_gap";
    m.title = "Hodgkin-Huxley neuron with gap junctions (inferior olive)";
    m.family = "Hodgkin-Huxley";
    m.summary = "A Hodgkin-Huxley cell fitted to inferior-olive neurons, with extra Kv1 and persistent sodium channels, and the only current-synapse "
                "cell that accepts gap junctions: electrical coupling that synchronises neighbours.";
    m.examples = { "Inferior olive neurons (subthreshold oscillations, electrical coupling)" };
    m.source = "Hahne et al. 2015 Front Neuroinform 9:22 (gap junctions in NEST); Hodgkin & Huxley 1952 J Physiol 117:500-544";
    m.params = { P{ "g_Na", "nS", "Sodium conductance.", 4500, 1000, 10000 }, P{ "g_Kv3", "nS", "Kv3 potassium conductance.", 9000, 1000, 20000 },
      P{ "g_Kv1", "nS", "Kv1 potassium conductance.", 9, 0, 50 }, P{ "g_L", "nS", "Leak conductance.", 10, 2, 50 }, P{ "C_m", "pF", "Membrane capacitance.", 40, 10, 200 },
      P{ "I_e", "pA", "Constant input current.", 0, -300, 1000 } };
    m.input_weight = 90;
    m.weight_unit = "pA";
    m.ei = "either";
    m.ei_note = "Gap junctions are excitatory-like and symmetric; chemical synapses can be either.";
    m.drive = { "dc", "pA", { 0, 80, 100, 200, 400, 800 } };
    m.gap_capable = true;
    add( m );
  }
  {
    CatalogModel m;
    m.id = m.model = "hh_cond_beta_gap_traub";
    m.title = "Traub Hodgkin-Huxley neuron with gap junctions";
    m.family = "Hodgkin-Huxley";
    m.summary = "A Traub-Miles cell with beta-function conductance synapses that also accepts gap junctions, so electrically coupled clusters "
                "can be built from conductance-based neurons.";
    m.examples = { "Electrically coupled interneuron networks (gamma synchrony)", "Hippocampal pyramidal cells in network models" };
    m.source = "Hahne et al. 2015 Front Neuroinform 9:22; Traub & Miles 1991";
    m.params = { P{ "V_T", "mV", "Voltage offset of the channel kinetics.", -50, -65, -45 }, P{ "g_Na", "nS", "Sodium conductance.", 20000, 5000, 30000 },
      P{ "g_K", "nS", "Potassium conductance.", 6000, 1000, 10000 }, P{ "g_L", "nS", "Leak conductance.", 10, 2, 50 }, P{ "C_m", "pF", "Membrane capacitance.", 200, 50, 500 },
      P{ "I_e", "pA", "Constant input current.", 0, -500, 2000 } };
    m.input_weight = 1;  // measured: one spike gives a 1.2 mV EPSP at rest
    m.weight_unit = "nS";
    m.ei = "either";
    m.ei_note = "Either sign; gap junctions are symmetric.";
    m.drive = { "dc", "pA", { 0, 100, 150, 250, 500, 1000 } };
    m.gap_capable = true;
    add( m );
  }

  // NESTML custom cells (models/nestml, registered by the NESTML module; available only when that module is compiled into the build)
  {
    CatalogModel m;
    m.id = m.model = "sa_node_pacemaker";
    m.title = "Sinoatrial node pacemaker cell";
    m.family = "Custom (NESTML)";
    m.summary = "A reduced model of the heart's pacemaker cell: it fires on its own at about 70 beats per minute through the funny current I_f (HCN channels), "
                "T- and L-type calcium and a delayed-rectifier potassium. Acetylcholine (vagus) slows it; noradrenaline (sympathetic) speeds it up. "
                "It needs no input to fire: a large depolarising current stops it (depolarisation block).";
    m.examples = { "Sinoatrial node cell setting the heart rate", "Autonomic control of heart rate (vagal and sympathetic)" };
    m.source = "Reduced Demir et al. 1994 / Maltsev & Lakatta 2009 style model (source documented with the model in docs/models)";
    m.params = { P{ "ne", "a.u.", "Noradrenaline level: raises I_f and I_CaL (speeds up).", 0, 0, 1 },
      P{ "ach", "a.u.", "Acetylcholine level: opens I_KACh and shifts I_f (slows down).", 0, 0, 1 }, P{ "g_f", "nS", "Funny-current (HCN) conductance.", 0.34, 0.1, 1 },
      P{ "g_CaL", "nS", "L-type calcium conductance.", 3.53, 1, 6 }, P{ "g_KACh", "nS", "Muscarinic potassium conductance at full ACh.", 0.03, 0, 0.2 },
      P{ "C_m", "pF", "Membrane capacitance.", 32, 10, 100 }, P{ "I_e", "pA", "Constant input current.", 0, -10, 10 } };
    m.input_weight = 0.2;  // measured: 0.2 nS gives a ~2 mV EPSP in the 32 pF cell (1 nS = 10 mV); the reference coupling is 2 nS
    m.weight_unit = "nS";
    m.ei = "either";
    m.ei_note = "A pacemaker: usually the source of a rhythm; modulate it with NE and ACh through the modulation table.";
    m.drive = { "dc", "pA", { -5, -2, 0, 1, 2, 4 } };
    m.nestml = true;
    m.vm_lo = -74;  // = vm_init_range("sa_node_pacemaker") in engine/src/circuit.cpp (checked in test_catalog)
    m.vm_hi = -45;
    m.run_ms = 12000;  // >= 10 beats even at the slowest drive level (0.4 Hz): extended up to max_ms
    m.skip_ms = 2000;
    m.max_ms = 60000;
    m.trace_ms = 1500;
    m.trace_dt_ms = 1.0;
    add( m );
  }
  {
    CatalogModel m;
    m.id = m.model = "butera_burster";
    m.title = "Bursting pacemaker (preBotzinger complex)";
    m.family = "Custom (NESTML)";
    m.summary = "A persistent-sodium burster: slow inactivation of I_NaP makes the cell burst rhythmically even without synaptic input. The kernel of the "
                "respiratory rhythm generator in the brainstem. More tonic drive makes the bursts faster and finally tonic.";
    m.examples = { "preBotzinger complex inspiratory neurons driving breathing" };
    m.source = "Butera, Rinzel & Smith 1999 J Neurophysiol 82:382-397 (model 1)";
    m.params = { P{ "g_NaP", "nS", "Persistent sodium conductance (burst drive).", 2.8, 0, 6 }, P{ "g_L", "nS", "Leak conductance (sets the burst regime).", 2.8, 1, 5 },
      P{ "g_tonic", "nS", "Tonic excitatory conductance (paper g_tonic-e): speeds the bursts up.", 0.3, 0, 1 },
      P{ "C_m", "pF", "Membrane capacitance.", 21, 5, 50 }, P{ "I_e", "pA", "Constant input current.", 0, -10, 50 },
      P{ "ne", "a.u.", "Noradrenaline level (adds excitatory conductance).", 0, 0, 1 }, P{ "ach", "a.u.", "Acetylcholine level (adds excitatory conductance).", 0, 0, 1 } };
    m.input_weight = 0.05;  // measured: 0.1 nS gives a 1 mV EPSP; 0.3 nS pushes a quiet cell into spiking; the reference uses 0.03-0.09 nS
    m.weight_unit = "nS";
    m.ei = "excitatory";
    m.ei_note = "Respiratory pacemaker neurons are excitatory.";
    m.drive = { "dc", "pA", { 0, 2, 5, 10, 20, 50 } };
    m.nestml = true;
    m.run_ms = 60000;  // >= 10 bursts (period 2.5-10 s at the low drive levels)
    m.skip_ms = 2000;
    m.trace_ms = 5000;
    m.trace_dt_ms = 5.0;
    add( m );
  }
  {
    CatalogModel m;
    m.id = m.model = "icc_slow_wave";
    m.title = "Interstitial cell of Cajal slow-wave oscillator";
    m.family = "Custom (NESTML)";
    m.summary = "A simple two-variable (FitzHugh-Nagumo type) oscillator for the gut's pacemaker cells: slow plateau-like waves about three times per minute "
                "that set the rhythm of peristalsis in the smooth muscle. It emits one spike at each wave onset. ACh speeds it up, NE slows it.";
    m.examples = { "Gut interstitial cells of Cajal (stomach and small intestine slow waves)" };
    m.source = "FitzHugh 1961 Biophys J 1:445-466; Nagumo et al. 1962, rescaled to ICC slow waves (documented with the model in docs/models)";
    m.params = { P{ "drive", "", "Baseline excitation (0.5 gives about 3 waves per minute).", 0.5, 0, 1 }, P{ "tau_w", "ms", "Recovery time constant (sets the wave period).", 6000, 1000, 20000 },
      P{ "tau_x", "ms", "Excitation time constant (sets the wave onset speed).", 600, 100, 2000 }, P{ "V_base", "mV", "Trough of the slow wave.", -70, -80, -60 },
      P{ "V_amp", "mV", "Amplitude of the slow wave.", 25, 10, 40 }, P{ "I_e", "pA", "Constant input current (100 pA shifts the excitation by 1).", 0, -100, 100 },
      P{ "ach", "a.u.", "Acetylcholine level (depolarises, speeds up).", 0, 0, 1 }, P{ "ne", "a.u.", "Noradrenaline level (hyperpolarises, slows down).", 0, 0, 1 } };
    m.input_weight = 200;  // measured: 200 pA gives a 1 unit step of the excitation (about 1 mV of V_m); the reference coupling is 400 pA
    m.weight_unit = "pA";
    m.ei = "excitatory";
    m.ei_note = "Drives smooth muscle through electrical coupling.";
    m.drive = { "dc", "pA", { -40, -20, -10, 0, 20, 40 } };
    m.nestml = true;
    m.run_ms = 60000;  // a wave every 20 s: extended (one spike per wave onset) until 10 waves, up to 6 min
    m.skip_ms = 10000;
    m.max_ms = 360000;
    m.trace_ms = 30000;
    m.trace_dt_ms = 50.0;
    add( m );
  }
  return v;
}
} // namespace

const std::vector< CatalogModel >& catalog()
{
  static const std::vector< CatalogModel > c = build();
  return c;
}

const CatalogModel* catalog_find( const std::string& id )
{
  for ( const CatalogModel& m : catalog() )
    if ( m.id == id )
      return &m;
  return nullptr;
}

std::string catalog_json()
{
  json models = json::array();
  for ( const CatalogModel& m : catalog() )
  {
    json params = json::array();
    for ( const CatalogParam& p : m.params )
      params.push_back( { { "name", p.name }, { "unit", p.unit }, { "desc", p.desc }, { "default", p.def }, { "min", p.lo }, { "max", p.hi } } );
    models.push_back( { { "id", m.id }, { "model", m.model }, { "title", m.title }, { "family", m.family }, { "summary", m.summary },
      { "examples", m.examples }, { "source", m.source }, { "params", params }, { "preset", json::parse( m.preset_json ) },
      { "input_weight", m.input_weight }, { "weight_unit", m.weight_unit }, { "ei", m.ei }, { "ei_note", m.ei_note },
      { "drive", { { "kind", m.drive.kind }, { "unit", m.drive.unit }, { "levels", m.drive.levels } } }, { "gap_capable", m.gap_capable },
      { "nestml", m.nestml }, { "vm_init", m.vm_hi > m.vm_lo ? json::array( { m.vm_lo, m.vm_hi } ) : json( nullptr ) }, { "params_provisional", m.params_provisional },
      { "probe", { { "run_ms", m.run_ms }, { "skip_ms", m.skip_ms }, { "max_ms", m.max_ms }, { "min_cycles", m.min_cycles }, { "trace_ms", m.trace_ms }, { "trace_dt_ms", m.trace_dt_ms } } } } );
  }
  return json{ { "schema", 1 }, { "models", models } }.dump();
}

std::string catalog_circuit_json( const CatalogModel& m, double level )
{
  json c = { { "schema_version", 1 }, { "name", "catalog " + m.id }, { "resolution_ms", 0.1 }, { "seed", 7 },
    { "populations", json::array( { { { "id", "N" }, { "model", m.model }, { "n", 1 }, { "params", json::parse( m.preset_json ) } } } ) },
    { "inputs", json::array( { { { "name", "drive" }, { "kind", m.drive.kind }, { "targets", json::array( { "N" } ) }, { "weight", 1.0 }, { "value", level } } } ) },
    { "outputs", json::array( { { { "name", "out" }, { "kind", "rate" }, { "neurons", json::array( { "N" } ) } } } ) } };
  return c.dump();
}

} // namespace spike
