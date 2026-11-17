// What the lab hands a novice, for the micro-puzzle tests (tasks_micro_a_test, tasks_micro_b_test): the cell the parts picker opens on
// (Task::default_model(), a catalogue card id) and the default weights of a first connection. The gate "a novice passes every micro-puzzle in
// at most 8 runs" is only meaningful when the first build is made with exactly these, so the tests build their benches through this header.
//
//   part_of( card )      the NEST model, its preset parameters and the weight of one typical synapse onto it (pA for current cells, nS for
//                        conductance cells), as godot/game/circuits.gd (weight_of, PRESETS) gives them: iaf 80 pA, aeif 8 nS, SA node 0.2 nS,
//                        Izhikevich 25.
//   sensor_weight( ... ) the weight of the first connection of a sensor: the task's own ChannelKind::weight when it declares one (the
//                        lab uses it), else 300 pA per unit for a dc sensor and 3.5 x the synapse weight for a Poisson sensor.
#pragma once
#include <string>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

namespace microlab
{

struct Part
{
  std::string model;
  nlohmann::json params = nlohmann::json::object();
  double weight = 0;  // one typical synapse
};

inline Part part_of( const std::string& card )
{
  Part p;
  if ( card == "iaf_psc_alpha" )
    p = { "iaf_psc_alpha", nlohmann::json::object(), 80.0 };
  else if ( card == "aeif_cond_alpha" )
    p = { "aeif_cond_alpha", nlohmann::json::object(), 8.0 };
  else if ( card == "sa_node_pacemaker" )
    p = { "sa_node_pacemaker", nlohmann::json::object(), 0.2 };
  else if ( card == "izhikevich_CH" )
    p = { "izhikevich", { { "a", 0.02 }, { "b", 0.2 }, { "c", -50.0 }, { "d", 2.0 } }, 25.0 };
  else
    CHECK_MSG( false, "unknown default_model card '" + card + "'" );
  return p;
}

inline double sensor_weight( const spike::Task& t, const std::string& sensor, const Part& target )
{
  auto kinds = t.channel_kinds();
  auto it = kinds.find( sensor );
  if ( it != kinds.end() && it->second.weight > 0 )
    return it->second.weight;
  const bool dc = it != kinds.end() && it->second.kind == "dc";
  return dc ? 300.0 : 3.5 * target.weight;
}

}  // namespace microlab
