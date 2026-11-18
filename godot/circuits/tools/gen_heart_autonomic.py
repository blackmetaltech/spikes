import sys,json
from gl import *
P=dict(
 w_in=800.0,       # sensor -> relay cells (pA)
 ie_na=376.0, ie_il=0.0,
 cc_na=-8.0, hi_na=10.0, lo_na=-6.0,
 na_vg=1500.0, 
 cc_il=12.0, lo_il=10.0, hi_il=-25.0, ie_il_=368.0,
 il_sg=1500.0,
 g_ach=0.012, tau_ach=500.0, g_ne=0.0031, tau_ne=1500.0,
 gain_ach=1.0, gain_ne=1.0)
for a in sys.argv[2:]:
    k,v=a.split('='); P[k]=float(v)
out=sys.argv[1] if len(sys.argv)>1 else __import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)),'..','heart_autonomic.json')
N=lambda k:k
pops=[
 pop("CC","iaf_psc_alpha",4,40,40),
 pop("LOW","iaf_psc_alpha",4,40,140),
 pop("HIGH","iaf_psc_alpha",4,40,240),
 pop("NA","iaf_psc_alpha",6,200,240,{"I_e":P['ie_na']}),
 pop("VG","iaf_psc_alpha",6,360,240),
 pop("IML","iaf_psc_alpha",6,200,60,{"I_e":P['ie_il_']}),
 pop("SG","iaf_psc_alpha",6,360,60),
 pop("SA","sa_node_pacemaker",1,520,150,{"V_m":-60.0}),
]
projs=[
 proj("CC","NA",P['cc_na']),proj("HIGH","NA",P['hi_na']),proj("LOW","NA",P['lo_na']),
 proj("NA","VG",P['na_vg'],rule="one_to_one"),
 proj("CC","IML",P['cc_il']),proj("LOW","IML",P['lo_il']),proj("HIGH","IML",P['hi_il']),
 proj("IML","SG",P['il_sg'],rule="one_to_one"),
]
c={"schema_version":1,"name":"Autonomic control of heart rate","resolution_ms":0.1,"seed":21,
"populations":pops,"projections":projs,
"inputs":[
 {"name":"exercise_cue","kind":"poisson","targets":["CC"],"weight":P['w_in'],"value":0.0},
 {"name":"hr_too_low","kind":"poisson","targets":["LOW"],"weight":P['w_in'],"value":0.0},
 {"name":"hr_too_high","kind":"poisson","targets":["HIGH"],"weight":P['w_in'],"value":0.0}],
"outputs":[
 {"name":"sa_beat","kind":"rate","neurons":[{"pop":"SA"}]},
 {"name":"vagal","kind":"modulator","modulator":"ACh","neurons":[{"pop":"VG"}],"gain":P['g_ach'],"tau_ms":P['tau_ach']},
 {"name":"sympathetic","kind":"modulator","modulator":"NE","neurons":[{"pop":"SG"}],"gain":P['g_ne'],"tau_ms":P['tau_ne']}],
"modulation":[
 {"modulator":"ACh","pop":"SA","param":"ach","gain":P['gain_ach']},
 {"modulator":"NE","pop":"SA","param":"ne","gain":P['gain_ne']}],
"record":{"spikes":"all"},
"meta":{"author":"spike","task":"heart_autonomic","notes":"docs/circuits/heart_autonomic.md"}}
save(c,out)
