#include "ring_backend.h"
#include "event.h"
#include "io_manager_impl.h"
#include "kernel_manager.h"
#include "recording_device.h"
#include "vp_manager_impl.h"

namespace spike
{
namespace
{
RingBackend* g_instance = nullptr;
}

RingBackend::RingBackend()
{
  g_instance = this;
  size_rings();
}

RingBackend::~RingBackend() throw()
{
  if ( g_instance == this )
    g_instance = nullptr;
}

RingBackend* RingBackend::instance()
{
  return g_instance;
}

void RingBackend::size_rings()
{
  size_t n = std::max< size_t >( 1, nest::kernel().vp_manager.get_num_threads() );
  if ( rings_.size() == n )
    return;
  rings_.clear();
  for ( size_t i = 0; i < n; ++i )
    rings_.push_back( std::make_unique< SpscRing >( kCapacity ) );
}

// NEST calls initialize() again when the thread count changes (after finalize()).
void RingBackend::initialize() { size_rings(); }
void RingBackend::finalize() {}
void RingBackend::enroll( const nest::RecordingDevice&, const Dictionary& ) {}
void RingBackend::disenroll( const nest::RecordingDevice& ) {}
void RingBackend::set_value_names( const nest::RecordingDevice&, const std::vector< std::string >&, const std::vector< std::string >& ) {}
void RingBackend::prepare() { size_rings(); }
void RingBackend::cleanup() {}
void RingBackend::pre_run_hook() {}
void RingBackend::post_run_hook() {}
void RingBackend::post_step_hook() {}
void RingBackend::set_status( const Dictionary& ) {}
void RingBackend::get_status( Dictionary& ) const {}
void RingBackend::check_device_status( const Dictionary& ) const {}
void RingBackend::get_device_defaults( Dictionary& ) const {}
void RingBackend::get_device_status( const nest::RecordingDevice&, Dictionary& ) const {}

void RingBackend::write( const nest::RecordingDevice& device, const nest::Event& e, const std::vector< double >&, const std::vector< long >& )
{
  const size_t t = device.get_thread();
  RingSpike s{ static_cast< int64_t >( e.get_stamp().get_steps() ), e.get_offset(), static_cast< int32_t >( e.get_sender_node_id() ) };
  if ( t >= rings_.size() or not rings_[ t ]->push( s ) )
    dropped_.fetch_add( 1, std::memory_order_relaxed );
}

void register_spike_ring()
{
  auto& io = nest::kernel().io_manager;
  if ( not io.is_valid_recording_backend( "spike_ring" ) )
    io.register_recording_backend< RingBackend >( "spike_ring" );
}

} // namespace spike
