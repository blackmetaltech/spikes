# Generate circuits/heart_pacemaker.json.   usage: gen_heart_pacemaker.py [key=value ...] [out=path]
# keys: w (coupling weight, nS), ie_c, ie_p (I_e of the centre / periphery cells, pA)
# The task adds its own seeded per-cell C_m spread and the resting vagal tone (docs/circuits/heart_pacemaker.md).
import json,sys,os
P=dict(w=1.0, ie_c=-3.5, ie_p=2.5, out=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','heart_pacemaker.json'))
for a in sys.argv[1:]:
    k,v=a.split('=',1); P[k]=v if k=='out' else float(v)
W=P['w']
c={"schema_version":1,"name":"Sinoatrial pacemaker cluster","resolution_ms":0.1,"seed":11,
"populations":[
 {"id":"SA_c","model":"sa_node_pacemaker","n":3,"params":{"I_e":P['ie_c'],"V_m":-65.0},"pos":[[120,200],[150,230],[120,260]],"role":"neuron"},
 {"id":"SA_p","model":"sa_node_pacemaker","n":4,"params":{"I_e":P['ie_p'],"V_m":-45.0},"pos":[[260,180],[290,210],[260,240],[290,270]],"role":"neuron"}],
"projections":[
 {"pre":"SA_c","post":"SA_p","rule":"all_to_all","synapse":{"synapse_model":"static_synapse","weight":W,"delay":1.0}},
 {"pre":"SA_p","post":"SA_c","rule":"all_to_all","synapse":{"synapse_model":"static_synapse","weight":W,"delay":1.0}}],
"inputs":[],
"outputs":[{"name":"sa_beat","kind":"rate","neurons":[{"pop":"SA_c"},{"pop":"SA_p"}]}],
"modulation":[
 {"modulator":"ACh","pop":"SA_c","param":"ach","gain":1.0},{"modulator":"NE","pop":"SA_c","param":"ne","gain":1.0},
 {"modulator":"ACh","pop":"SA_p","param":"ach","gain":1.0},{"modulator":"NE","pop":"SA_p","param":"ne","gain":1.0}],
"record":{"spikes":"all"},
"meta":{"author":"spike","task":"heart_pacemaker","notes":"docs/circuits/heart_pacemaker.md"}}
json.dump(c,open(P['out'],'w'),indent=1)
