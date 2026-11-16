class_name LesionHints
extends RefCounted
## What each classic lesion of a reference circuit means in the body: the "expected biology" column of the lesion tables in docs/circuits/*.md,
## as data the lesion lab can show after a run. A lesion set is matched by its parts, written as tokens:
##   "A>B"  cut the axon from population A to population B     "-X"  silence population X     "~name"  cut the sensor named `name`
## A hint is [tokens, short name, expected biology]. The set must be exactly the tokens (order does not matter). Population names are those of the reference
## circuit; a player's own circuit gets the generic reading of each lesion kind instead (see `generic`).

const HINTS := {
	"reflex": [
		[["SN>IN"], "Cut afferent arc", "Sensory neuropathy or dorsal-root section: the limb is anaesthetic and the withdrawal reflex is lost (areflexia)."],
		[["IN>MN"], "Cut interneuron output", "A lesion between the relay and the motor pool: the stimulus is felt but nothing moves."]],
	"worm": [
		[["ASEL>AIY"], "No run signal", "In C. elegans, ASEL (salt ON) drives the run command through AIY. Without it AIB keeps turning and the worm circles (Iino & Yoshida 2009)."],
		[["AIY>AIB"], "No inhibition of turning", "AIY normally suppresses the turning neuron AIB; without that brake the worm turns constantly and circles."],
		[["AIB>TR"], "Cannot turn", "The turning command never reaches the muscles: the worm walks straight away from the salt."],
		[["ASER>AIB"], "OFF pathway removed", "ASER (salt OFF) is an accelerator: the worm still finds the peak but more slowly (Suzuki et al. 2008)."],
		[["ASER>AIB", "ASEL>AIY"], "No salt sensing", "Both salt cells disconnected: the worm is blind to salt and circles at random."]],
	"lamprey": [
		[["CC_L>E_R", "CC_R>E_L"], "No reciprocal inhibition", "Without crossing inhibition the half-centres fire together: the swimming rhythm becomes a standing co-contraction (Grillner 2003; Buchanan 2001)."],
		[["CC_L>E_R"], "One crossing cut", "One side is no longer inhibited by the other: the left-right alternation shrinks and the rhythm is lopsided."],
		[["E_L>CC_L", "E_R>CC_R"], "Commissural cells silent", "The inhibitory cells never get excited, so the two sides cannot alternate."],
		[["E_L>MN_L", "E_R>MN_R"], "Motor output lost", "Rhythm generator disconnected from the motoneurons (a spinal transection below the CPG): no swimming."],
		[["E_R>MN_R"], "One-sided rhythm", "Only one side reaches the muscles: the body bends one way and no wave travels."]],
	"stretch_reflex": [
		[["Ia_F>MN_F", "Ia_E>MN_E"], "Ia to motoneuron cut", "Dorsal-root section / areflexia: the tendon tap gives no reflex and the limb is floppy (hypotonia)."],
		[["Ia_F>IN_F", "Ia_E>IN_E"], "No reciprocal inhibition", "Without Ia inhibitory interneurons the antagonist is not relaxed: co-contraction and rigid, stiff movement (Sherrington's reciprocal innervation)."],
		[["IN_F>MN_E", "IN_E>MN_F"], "Interneurons cannot inhibit", "Same outcome as losing the Ia interneurons: the antagonist keeps firing against the reflex (as in tetanus or strychnine, which block spinal inhibition)."]],
	"renshaw": [
		[["MN_F>RC_F", "MN_E>RC_E"], "Renshaw cells never driven", "No recurrent inhibition: motoneuron rate rises and tremor is no smaller than the same rate would give anyway (Renshaw 1941; Hultborn 2006)."],
		[["RC_F>MN_F", "RC_E>MN_E"], "Renshaw cells cannot inhibit", "The Renshaw cells fire but cannot feed back: the pool saturates and the limb loses its stretch resistance (as when strychnine blocks glycine receptors)."]],
	"posture": [
		[["Ia_F>MN_F", "Ia_E>MN_E", "II_F>MN_F", "II_E>MN_E"], "Stretch reflex cut", "Deafferentation: the limb loses its length servo and drifts upward when the extensor is stretched (Matthews 1972)."],
		[["TONIC>GAM"], "No gamma drive", "Without gamma motoneurons the spindles go slack when the muscle shortens, so the reflex weakens and the limb sags (fusimotor paralysis)."],
		[["TONIC>MN_F"], "No antigravity tone", "Loss of descending tonic drive to the flexors: the limb falls under gravity (flaccid paralysis)."]],
	"reach": [
		[["B1>MN_F"], "No accelerating burst", "The first agonist burst of the triphasic pattern is missing: reaches are slow, as in cerebellar and basal-ganglia disease (Hallett et al. 1975)."],
		[["B2>MN_E", "B2>MN_F"], "No braking burst", "Without the antagonist braking burst the arm overshoots and rings: dysmetria, a cerebellar sign."],
		[["GO>B1"], "Go cue never starts the burst generator", "The hold command alone still moves the arm, but slowly: like bradykinesia with intact posture."]],
	"heart_pacemaker": [
		[["SA_c>SA_p", "SA_p>SA_c"], "Coupling removed", "Uncoupled node cells of different rates give no single rhythm: sinus node dysfunction or sino-atrial exit block (Jalife 1984)."],
		[["SA_c>SA_p"], "Centre does not drive periphery", "Exit block: the centre of the node beats but the periphery, and so the atrium, cannot follow it."],
		[["SA_p>SA_c"], "Periphery does not feed back", "One-way coupling: the periphery follows but cannot pull the centre, so the rhythm drifts to the faster cell."]],
	"heart_autonomic": [
		[["NA>VG"], "Vagus cut", "Atropine or vagotomy: resting heart rate rises to the intrinsic rate (resting tachycardia; Jose & Collison 1970)."],
		[["IML>SG"], "Sympathetic chain cut", "Beta-blockade or sympathectomy: the exercise heart rate is limited to the intrinsic rate."],
		[["CC>IML", "LOW>IML"], "No drive to sympathetic cells", "The preganglionic sympathetic cells get no command: no exercise tachycardia."],
		[["CC>IML"], "Central command to IML cut", "The error feedback alone still raises NE, but a proportional loop keeps a steady-state error."],
		[["CC>NA"], "Central command to vagal cells cut", "Redundant route: the task still passes, only more slowly, because hr_too_low also withdraws the vagus."]],
	"baroreflex": [
		[["AFF>NTS"], "Sino-aortic denervation", "Baroreceptor denervation removes the tonic depressor drive: pressure is high and unregulated (Cowley, Liard & Guyton 1973)."],
		[["RVLM>IML"], "Spinal cord / sympathetic cut", "Sympathectomy: no vascular tone and orthostatic hypotension on standing."],
		[["IML>SG"], "Ganglionic block", "Hexamethonium-like block: the same loss of sympathetic tone as a spinal lesion."],
		[["NTS>CVLM"], "Depressor pathway lost", "A CVLM lesion raises pressure: nothing inhibits the RVLM (Dampney 1994)."]],
	"breathing": [
		[["RTN>PBC", "RTN>PHR"], "Chemoreceptor input cut", "Central chemoreceptors (RTN) removed: the resting rhythm persists but there is no ventilatory response to CO2 (Guyenet et al. 2010)."],
		[["PBC>PBC"], "Bursters uncoupled", "The preBotzinger cells burst out of phase: no network rhythm and little ventilation (Feldman & Del Negro 2006)."],
		[["PBC>EXP"], "No disinhibition", "The rhythm no longer gates the expiratory cells' silence: the phrenic pool stays quiet, apnoea."],
		[["EXP>PHR"], "No expiratory silence", "Without expiratory inhibition the phrenic fires tonically in shallow, fast breaths with no real ventilation."]],
	"pupil": [
		[["PON>EW"], "Edinger-Westphal denervated", "Oculomotor (parasympathetic) palsy: a mid-dilated pupil that barely reacts to light (Gamlin 2006)."],
		[["EW>CG"], "Ciliary ganglion cut", "Same picture one synapse later: a tonic, poorly reacting pupil."],
		[["IML>SCG"], "Sympathetic chain cut", "Horner-like: the dilator never fires, so the pupil stays small in the dark and fright dilates it less."],
		[["HYP>IML", "HYP>EW"], "Alarm pathway cut", "The light reflex is intact but there is no fright dilation."],
		[["RGC>PON"], "No retinal input to the pretectum", "Afferent defect: no light reflex at all, the pupil is fixed (like blindness with intact oculomotor nerve)."]],
	"peristalsis": [
		[["DIN>IMN"], "Descending inhibition removed", "The segments ahead contract against the bolus: it moves slowly, as in a spastic or obstructed gut (Bayliss & Starling 1899)."],
		[["ICCP>ICC", "ICC>ICC"], "No slow waves", "Loss of the interstitial cells of Cajal: contraction stays near threshold and transit fails (as in gastroparesis)."],
		[["S>AIN", "S>DIN"], "Bolus not sensed", "No enteric reflex: tone and slow waves alone push the bolus only part of the way."],
		[["AIN>EMN"], "Ascending excitation removed", "Partial: tone plus slow waves still carry the bolus, only slower, a reminder that the gut has several propulsion mechanisms."]],
	"fight_or_flight": [
		[["HYP>RVLM"], "No sympathetic outflow", "The heart only speeds by vagal withdrawal and the pressure barely rises: sympathetic denervation. The pupil still opens through Edinger-Westphal withdrawal."],
		[["RVLM>SG"], "No NE on the pacemaker", "Cardiac sympathetic block (beta-blockade): the heart-rate rise is small, pressure and pupil still respond."],
		[["HYP>VRG"], "No respiratory drive", "Ventilation does not rise with the threat."],
		[["RVLM>SCG", "HYP>EW"], "Pupil pathways cut", "Fixed pupil: the rest of the response is intact."],
		[["HYP>NA"], "Vagus not withdrawn", "The vagal brake stays on, so the heart rate rises less."],
		[["HYP>EW"], "Edinger-Westphal still active", "The pupil dilates less (only the sympathetic part)."],
		[["RVLM>SV"], "No vascular sympathetic tone", "Vessels do not constrict: the pressure rise is smaller."]]}

## Token set of a lesion set (dictionary of keys) as readable tokens, sorted.
static func tokens(c: Dictionary, lesions: Dictionary) -> Array:
	var out := []
	for key in lesions:
		var k := Lesion.parse_key(str(key))
		match k[0]:
			"proj":
				var i := int(k[1]); var projs: Array = c.get("projections", [])
				if i >= 0 and i < projs.size(): out.append("%s>%s" % [Circuits.proj_pop(projs[i].get("pre", "")), Circuits.proj_pop(projs[i].get("post", ""))])
			"pop": out.append("-" + str(k[1]))
			"in": out.append("~" + str(k[1]))
	out.sort(); return out

## The hint for a lesion set on `task_id`: {name, bio, exact} (exact false = the generic reading). Intact: the baseline sentence.
static func find(task_id: String, c: Dictionary, lesions: Dictionary) -> Dictionary:
	if lesions.is_empty(): return {"name": "Intact", "bio": "The healthy circuit: every other row is compared with this one.", "exact": true}
	var tk := tokens(c, lesions)
	for h in HINTS.get(task_id, []):
		var ht: Array = (h[0] as Array).duplicate(); ht.sort()
		if ht == tk: return {"name": str(h[1]), "bio": str(h[2]), "exact": true}
	return generic(c, lesions)

## What a lesion of each kind means when the table has no entry for it.
static func generic(c: Dictionary, lesions: Dictionary) -> Dictionary:
	var kinds := {}
	for key in lesions: kinds[Lesion.parse_key(str(key))[0]] = true
	var bio := "No textbook correlate in this task's table. Compare the verdict and the score parts with the intact row to see what the lesioned part contributed."
	if kinds.has("in") and kinds.size() == 1: bio = "Deafferentation: the circuit loses a sensory channel (like a nerve or dorsal-root section). Look for the score part that collapses."
	elif kinds.has("pop") and kinds.size() == 1: bio = "A focal lesion: every cell of this group is silenced, as in a stroke or a tumour of one nucleus. The score parts show what that nucleus was for."
	elif kinds.has("proj") and kinds.size() == 1: bio = "A disconnection: the cells are intact but one pathway between them is cut, as in a tract lesion. The score parts show what travelled along it."
	return {"name": "", "bio": bio, "exact": false}
