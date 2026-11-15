// "spike_ring": a NEST RecordingBackend that pushes (node_id, step, offset) into one lock-free
// single-producer/single-consumer ring per NEST thread. The consumer (Engine::step, on the caller's thread)
// pops after nest::run() returns. NEST deletes all backends on ResetKernel, so register_spike_ring() must be
// called after every init_nest()/reset_kernel() (the Engine does).
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include "recording_backend.h"

namespace spike
{

struct RingSpike
{
  int64_t step;    // NEST time stamp in steps (spike emitted at the end of step-1 .. step: same as memory backend 'times' with time_in_steps)
  double offset;   // precise-spike offset in ms (0 for grid-based models); t_ms = step * resolution - offset
  int32_t node_id; // NEST node id (1-based)
};

class SpscRing
{
public:
  explicit SpscRing( size_t capacity_pow2 ) : buf_( capacity_pow2 ), mask_( capacity_pow2 - 1 ) {}
  bool push( const RingSpike& s ) // producer
  {
    const uint64_t h = head_.load( std::memory_order_relaxed );
    if ( h - tail_.load( std::memory_order_acquire ) > mask_ )
      return false;
    buf_[ h & mask_ ] = s;
    head_.store( h + 1, std::memory_order_release );
    return true;
  }
  template < class F > size_t pop_all( F&& f ) // consumer
  {
    uint64_t t = tail_.load( std::memory_order_relaxed );
    const uint64_t h = head_.load( std::memory_order_acquire );
    for ( uint64_t i = t; i != h; ++i )
      f( buf_[ i & mask_ ] );
    tail_.store( h, std::memory_order_release );
    return h - t;
  }

private:
  std::vector< RingSpike > buf_;
  size_t mask_;
  alignas( 64 ) std::atomic< uint64_t > head_{ 0 };
  alignas( 64 ) std::atomic< uint64_t > tail_{ 0 };
};

class RingBackend : public nest::RecordingBackend
{
public:
  static constexpr size_t kCapacity = size_t( 1 ) << 17; // spikes per thread between two pops

  RingBackend();
  ~RingBackend() throw() override;
  void initialize() override;
  void finalize() override;
  void enroll( const nest::RecordingDevice&, const Dictionary& ) override;
  void disenroll( const nest::RecordingDevice& ) override;
  void set_value_names( const nest::RecordingDevice&, const std::vector< std::string >&, const std::vector< std::string >& ) override;
  void prepare() override;
  void cleanup() override;
  void pre_run_hook() override;
  void post_run_hook() override;
  void post_step_hook() override;
  void write( const nest::RecordingDevice&, const nest::Event&, const std::vector< double >&, const std::vector< long >& ) override;
  void set_status( const Dictionary& ) override;
  void get_status( Dictionary& ) const override;
  void check_device_status( const Dictionary& ) const override;
  void get_device_defaults( Dictionary& ) const override;
  void get_device_status( const nest::RecordingDevice&, Dictionary& ) const override;

  /// Consumer side: pops every ring (call after run() returns).
  template < class F > void pop_all( F&& f )
  {
    for ( auto& r : rings_ )
      r->pop_all( f );
  }
  uint64_t dropped() const { return dropped_.load( std::memory_order_relaxed ); }

  /// The live instance (owned by NEST's IOManager); nullptr before register_spike_ring().
  static RingBackend* instance();

private:
  void size_rings();
  std::vector< std::unique_ptr< SpscRing > > rings_;
  std::atomic< uint64_t > dropped_{ 0 };
};

/// Register "spike_ring" with the kernel's IOManager. Idempotent per kernel life.
void register_spike_ring();

} // namespace spike
