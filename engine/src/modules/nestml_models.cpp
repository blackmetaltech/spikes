// Registration of the NESTML-generated neuron models. The generated sources (third_party/nestml-gen) are compiled
// into libspike by engine/CMakeLists.txt; when they are absent SPIKE_HAVE_NESTML is undefined and this is a stub.
#include "modules/nestml_models.h"

#ifdef SPIKE_HAVE_NESTML
#include "exceptions.h"
#include "kernel_manager.h"
#include "model_manager.h"
#include "nest.h"
#include "nestml_includes.inc"
#endif

namespace spike
{
const std::vector< std::string >& nestml_model_names()
{
  static const std::vector< std::string > names = {
#ifdef SPIKE_HAVE_NESTML
#include "nestml_names.inc"
#endif
  };
  return names;
}

void register_nestml_models()
{
#ifdef SPIKE_HAVE_NESTML
  // register_node_model throws if the name is already taken, so test first
  try
  {
    nest::kernel().model_manager.get_node_model_id( nestml_model_names().front() );
    return;  // already registered in this kernel state
  }
  catch ( const nest::UnknownModelName& )
  {
  }
#include "nestml_register.inc"
#endif
}
} // namespace spike
