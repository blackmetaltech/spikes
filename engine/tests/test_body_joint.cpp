// Joint: gravity, agonist/antagonist drive, co-contraction stiffness, push, limits, load, spindle and tendon afferents.
#include <cstdio>
#include "check.h"
#include "spike/body/joint.h"

using namespace spike::body;

static JointInput drive( double ag, double ant )
{
  JointInput in;
  in.ag.excitation = ag;
  in.ant.excitation = ant;
  return in;
}
static void run( Joint& j, const JointInput& in, int ms )
{
  for ( int t = 0; t < ms; ++t )
    j.step( 1.0, in );
}

int main()
{
  JointParams P;
  // ---- 1. a limp limb sags under gravity; stays inside the stops ----
  {
    Joint j( P );
    j.reset( 0.8 );
    run( j, drive( 0, 0 ), 3000 );
    CHECK_MSG( j.theta < -0.6, "limp limb falls: theta " + std::to_string( j.theta ) );
    CHECK( j.theta >= P.theta_min - 0.05 );
  }
  // ---- 2. graded agonist drive holds the limb higher (monotone), antagonist pulls it down ----
  {
    double prev = -10;
    for ( double e : { 0.0, 0.1, 0.2, 0.3, 0.45, 0.6 } )
    {
      Joint j( P );
      j.reset( 0.5 );
      run( j, drive( e, 0 ), 4000 );
      CHECK_MSG( j.theta >= prev - 1e-3, "monotone angle vs flexor drive at e=" + std::to_string( e ) );
      CHECK( std::fabs( j.omega ) < 0.3 );   // settled (ripple of unfused slow units only)
      prev = j.theta;
      std::printf( "flexor %.2f: theta %.2f rad, torque_g %.2f Nm\n", e, j.theta, j.torque_gravity );
    }
    CHECK( prev > 0.9 );
    Joint a( P ), b( P );
    a.reset( 1.0 );
    b.reset( 1.0 );
    run( a, drive( 0.4, 0 ), 3000 );
    run( b, drive( 0.4, 0.3 ), 3000 );
    CHECK( b.theta < a.theta );
  }
  // ---- 3. co-contraction stiffens the joint against a push ----
  {
    auto push = [ & ]( double ant, double ag ) {
      Joint j( P );
      j.reset( 0.9 );
      run( j, drive( ag, ant ), 2000 );
      const double th0 = j.theta;
      JointInput in = drive( ag, ant );
      in.ext_torque = -3.0;  // push towards extension
      double mx = 0;
      for ( int t = 0; t < 150; ++t )
      {
        j.step( 1.0, in );
        mx = std::max( mx, th0 - j.theta );
      }
      return mx;
    };
    const double lo = push( 0.05, 0.4 ), hi = push( 0.4, 0.65 );
    std::printf( "push deflection: low co-contraction %.3f rad, high %.3f rad\n", lo, hi );
    CHECK( hi < 0.75 * lo );
  }
  // ---- 4. joint limits ----
  {
    Joint j( P );
    j.reset( 1.0 );
    run( j, drive( 1.0, 0 ), 3000 );
    CHECK_MSG( j.theta <= P.theta_max + 0.15 && j.theta > P.theta_max - 0.3, "upper stop: " + std::to_string( j.theta ) );
    j.reset( 1.0 );
    run( j, drive( 0, 1.0 ), 3000 );
    CHECK_MSG( j.theta >= P.theta_min - 0.15 && j.theta < P.theta_min + 0.3, "lower stop: " + std::to_string( j.theta ) );
  }
  // ---- 5. held load lowers the equilibrium angle ----
  {
    JointParams L = P;
    L.load_mass_kg = 2.0;
    Joint a( P ), b( L );
    a.reset( 0.5 );
    b.reset( 0.5 );
    run( a, drive( 0.35, 0 ), 4000 );
    run( b, drive( 0.35, 0 ), 4000 );
    CHECK( b.theta < a.theta - 0.1 );
  }
  // ---- 6. proprioceptors ----
  {
    auto settle = [ & ]( double th0, double ag, double ant ) {
      Joint j( P );
      j.reset( th0 );
      JointInput in = drive( ag, ant );
      run( j, in, 2500 );
      return j;
    };
    // static: flexor length (II) is larger when the limb is extended (low theta), smaller when flexed; extensor opposite
    Joint ext = settle( 0.0, 0.0, 0.0 ), flx = settle( 0.9, 0.6, 0.0 );
    std::printf( "II flexor: theta %.2f -> %.1f Hz, theta %.2f -> %.1f Hz\n", ext.theta, ext.ag_aff.ii, flx.theta, flx.ag_aff.ii );
    CHECK( ext.ag_aff.ii > flx.ag_aff.ii + 10 );
    CHECK( ext.ant_aff.ii < flx.ant_aff.ii - 10 );
    // dynamic: a ramp stretch of the flexor raises Ia far more than II, scales with velocity, and is absent on release
    auto stretch = [ & ]( double torque, double& ia_pk, double& ii_pk, double& ia_base, double& ii_base, double& ant_ia_min, double& ant_base ) {
      Joint j( P );
      j.reset( 0.9 );
      JointInput in = drive( 0.35, 0.05 );
      run( j, in, 2000 );
      ia_base = j.ag_aff.ia;
      ii_base = j.ag_aff.ii;
      ant_base = j.ant_aff.ia;
      ia_pk = ii_pk = 0;
      ant_ia_min = 1e9;
      in.ext_torque = torque;
      for ( int t = 0; t < 120; ++t )
      {
        j.step( 1.0, in );
        ia_pk = std::max( ia_pk, j.ag_aff.ia );
        ii_pk = std::max( ii_pk, j.ag_aff.ii );
        ant_ia_min = std::min( ant_ia_min, j.ant_aff.ia );
      }
    };
    double iaS, iiS, iaB, iiB, aiS, iaL, iiL, iaB2, iiB2, aiL, abS, abL;
    stretch( -2.5, iaS, iiS, iaB, iiB, aiS, abS );
    stretch( -6.0, iaL, iiL, iaB2, iiB2, aiL, abL );
    std::printf( "stretch: Ia %.0f -> %.0f / %.0f Hz (small/large), II %.0f -> %.0f / %.0f\n", iaB, iaS, iaL, iiB, iiS, iiL );
    CHECK( iaS > iaB + 20 );
    CHECK( iaL > iaS + 10 );                               // velocity sensitivity
    CHECK( ( iaL - iaB ) > 2.0 * ( iiL - iiB ) );          // Ia dynamic, II mostly static
    CHECK( aiL < abL - 10 );                               // the shortened antagonist's Ia falls
    // Ib follows force: more flexor force -> more Ib; the idle antagonist has near-zero Ib
    Joint lo = settle( 0.9, 0.15, 0.0 ), hi = settle( 0.9, 0.6, 0.0 );
    CHECK( hi.ag_aff.ib > lo.ag_aff.ib + 5 );
    CHECK( hi.ag_aff.ib > 1.5 * hi.ant_aff.ib );
    CHECK( hi.ag_aff.ib < P.gto.max_hz );
    // all rates are finite and non-negative
    CHECK( iaS >= 0 && iiS >= 0 && lo.ag_aff.ib >= 0 );
  }
  // ---- 7. determinism and readouts ----
  {
    auto go = [ & ] {
      Joint j( P );
      j.reset( 0.5 );
      JointInput in = drive( 0.4, 0.1 );
      in.ag.excitation = 0.4;
      run( j, in, 1500 );
      return j.theta;
    };
    CHECK( go() == go() );
    Joint j( P );
    CHECK( j.readouts().count( "ag_Ia_hz" ) && j.readouts().count( "theta_rad" ) );
  }
  return chk::finish( "test_body_joint" );
}
