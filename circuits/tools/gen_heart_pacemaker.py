import json,sys
W=float(sys.argv[1]) if len(sys.argv)>1 else 1.0
NW=float(sys.argv[2]) if len(sys.argv)>2 else 0.1
NR=float(sys.argv[3]) if len(sys.argv)>3 else 100.0
c={"schema_version":1,"name":"Sinoatrial pacemaker cluster","resolution_ms":0.1,"seed":11,
"populations":[
 {"id":"SA_c","model":"sa_node_pacemaker","n":3,"params":{"I_e":-3.5,"V_m":-65.0},"pos":[[120,200],[150,230],[120,260]],"role":"neuron"},
 {"id":"SA_p","model":"sa_node_pacemaker","n":4,"params":{"I_e":2.5,"V_m":-45.0},"pos":[[260,180],[290,210],[260,240],[290,270]],"role":"neuron"}],
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
json.dump(c,open('/home/user/spike/circuits/heart_pacemaker.json','w'),indent=1)
