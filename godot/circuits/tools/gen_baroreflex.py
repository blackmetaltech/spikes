import sys,json
from gl import *
P=dict(w_in=1600.0, aff_nts=280.0, nts_na=30.0, nts_cv=200.0, cv_rv=-140.0, ie_rv=380.0, ie_na=372.0,
 rv_il=1500.0, il_sg=1500.0, na_vg=1500.0,
 g_ach=0.012,tau_ach=500.0,g_ne=0.0076,tau_ne=1000.0,gain_ach=1.0,gain_ne=0.25)
for a in sys.argv[2:]:
    k,v=a.split('='); P[k]=float(v)
out=sys.argv[1] if len(sys.argv)>1 else __import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)),'..','baroreflex.json')
pops=[
 pop("AFF","iaf_psc_alpha",6,40,120),
 pop("NTS","iaf_psc_alpha",8,180,120),
 pop("NA","iaf_psc_alpha",4,320,40,{"I_e":P['ie_na']}),
 pop("VG","iaf_psc_alpha",4,460,40),
 pop("CVLM","iaf_psc_alpha",6,320,200),
 pop("RVLM","iaf_psc_alpha",6,460,200,{"I_e":P['ie_rv']}),
 pop("IML","iaf_psc_alpha",6,600,200),
 pop("SG","iaf_psc_alpha",6,740,200),
 pop("SA","sa_node_pacemaker",1,740,40,{"V_m":-60.0}),
]
projs=[proj("AFF","NTS",P['aff_nts']),proj("NTS","NA",P['nts_na']),proj("NA","VG",P['na_vg'],rule="one_to_one"),
 proj("NTS","CVLM",P['nts_cv']),proj("CVLM","RVLM",P['cv_rv']),proj("RVLM","IML",P['rv_il'],rule="one_to_one"),proj("IML","SG",P['il_sg'],rule="one_to_one")]
c={"schema_version":1,"name":"Baroreflex","resolution_ms":0.1,"seed":31,
"populations":pops,"projections":projs,
"inputs":[{"name":"baroreceptor","kind":"poisson","targets":["AFF"],"weight":P['w_in'],"value":41.0}],
"outputs":[
 {"name":"sa_beat","kind":"rate","neurons":[{"pop":"SA"}]},
 {"name":"vagal","kind":"modulator","modulator":"ACh","neurons":[{"pop":"VG"}],"gain":P['g_ach'],"tau_ms":P['tau_ach']},
 {"name":"sympathetic","kind":"modulator","modulator":"NE","neurons":[{"pop":"SG"}],"gain":P['g_ne'],"tau_ms":P['tau_ne']}],
"modulation":[
 {"modulator":"ACh","pop":"SA","param":"ach","gain":P['gain_ach']},
 {"modulator":"NE","pop":"SA","param":"ne","gain":P['gain_ne']}],
"record":{"spikes":"all"},
"meta":{"author":"spike","task":"baroreflex","notes":"docs/circuits/baroreflex.md"}}
save(c,out)
if '--scan' in sys.argv: pass
