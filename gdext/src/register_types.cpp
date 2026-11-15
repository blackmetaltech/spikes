#include "spike_sim.h"
#include "spike_task.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

static void initialize_spike_module( ModuleInitializationLevel level )
{
  if ( level != MODULE_INITIALIZATION_LEVEL_SCENE )
    return;
  GDREGISTER_CLASS( SpikeSim );
  GDREGISTER_CLASS( SpikeTask );
}

static void uninitialize_spike_module( ModuleInitializationLevel ) {}

extern "C"
{
  GDExtensionBool GDE_EXPORT spike_library_init( GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization* r_initialization )
  {
    GDExtensionBinding::InitObject init_obj( p_get_proc_address, p_library, r_initialization );
    init_obj.register_initializer( initialize_spike_module );
    init_obj.register_terminator( uninitialize_spike_module );
    init_obj.set_minimum_library_initialization_level( MODULE_INITIALIZATION_LEVEL_SCENE );
    return init_obj.init();
  }
}
