// {task, {{pre, post, weight factor}...}, description}: each lesioned reference circuit must FAIL its task.
// (included by tasks_circuits_test.cpp)
{ "reflex", { { "SN", "IN", 0.0 } }, "afferents disconnected from the interneurons (arc cut)" },
{ "reflex", { { "IN", "MN", 0.1 } }, "interneuron -> motoneuron weight x0.1 (motor pool never bursts)" },
{ "reflex", { { "SN", "IN", 4.0 }, { "IN", "MN", 4.0 } }, "gain x4 at both synapses (no coincidence requirement: noise triggers it)" },
{ "lamprey", { { "CC_L", "E_R", 0.0 }, { "CC_R", "E_L", 0.0 } }, "commissural inhibition deleted (no half-centre: L and R fire in phase)" },
{ "lamprey", { { "CC_L", "E_R", 0.0 } }, "one of the two inhibitory crossings deleted" },
{ "lamprey", { { "E_L", "CC_L", 0.0 }, { "E_R", "CC_R", 0.0 } }, "excitatory interneurons no longer recruit the inhibitory ones" },
{ "lamprey", { { "E_L", "MN_L", 0.0 }, { "E_R", "MN_R", 0.0 } }, "motoneurons disconnected (no muscle command)" },
{ "worm", { { "ASEL", "AIY", 0.0 } }, "ON cell cut off from AIY: AIB turns forever (worm circles)" },
{ "worm", { { "AIY", "AIB", 0.0 } }, "AIY no longer inhibits AIB (worm circles)" },
{ "worm", { { "AIB", "TR", 0.0 } }, "AIB cannot drive the turn motors (worm never turns)" },
