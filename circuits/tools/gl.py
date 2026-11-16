# Shared helpers for the circuit generators (population / projection / save). Generated JSON goes to circuits/.
import json
def grid(n,x0,y0,dx=26,dy=26,cols=2):
    return [[x0+(i%cols)*dx,y0+(i//cols)*dy] for i in range(n)]
def pop(id,model,n,x,y,params=None,cols=2):
    return {"id":id,"model":model,"n":n,"params":params or {},"pos":grid(n,x,y,cols=cols),"role":"neuron"}
def proj(pre,post,w,d=1.0,rule="all_to_all",**kw):
    p={"pre":pre,"post":post,"rule":rule,"synapse":{"synapse_model":"static_synapse","weight":w,"delay":d}}
    p.update(kw); return p
def save(c,path):
    json.dump(c,open(path,'w'),indent=1)
