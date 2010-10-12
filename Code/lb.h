#ifndef HEMELB_LB_H
#define HEMELB_LB_H

#include "net.h"
#include "lb/collisions/Collisions.h"

class LBM {
 public:
  int steering_session_id;

  double *inlet_density_avg, *inlet_density_amp, *inlet_density_phs;
  double *outlet_density_avg, *outlet_density_amp, *outlet_density_phs;

  char *system_file_name;

  double tau, viscosity;
  double voxel_size;
  double omega;

  int total_fluid_sites;
  int site_min_x, site_min_y, site_min_z;
  int site_max_x, site_max_y, site_max_z;
  int inlets, outlets;
  int cycles_max;
  int period;
  int conv_freq;

  float *block_density;

  int *block_map;

  double lbmConvertPressureToLatticeUnits (double pressure);
  double lbmConvertPressureToPhysicalUnits (double pressure);
  double lbmConvertVelocityToLatticeUnits (double velocity);
  double lbmConvertStressToLatticeUnits (double stress);
  double lbmConvertStressToPhysicalUnits (double stress);
  double lbmConvertVelocityToPhysicalUnits (double velocity);

  void lbmInit (char *system_file_name_in, char *parameters_file_name, Net *net);
  void lbmRestart (Net *net);
  ~LBM();

  int lbmCycle (int perform_rt, Net *net);
  void lbmCalculateFlowFieldValues ();
  void RecalculateTauViscosityOmega ();
  void lbmUpdateBoundaryDensities (int cycle_id, int time_step);
  void lbmUpdateInletVelocities (int time_step, Net *net);

  void lbmSetInitialConditions (Net *net);

  void lbmWriteConfig (int stability, char *output_file_name, Net *net);

  void fInterpolation (double omega, int i, double *density, double *v_x, double *v_y, double *v_z, double f_neq[], Net* net);
  void gzsBoundary (double omega, int i, double *density, double *v_x, double *v_y, double *v_z, double f_neq[], Net* net);

 private:

  int is_inlet_normal_available;

  void lbmInitCollisions();
  void lbmReadConfig (Net *net);
  void lbmReadParameters (char *parameters_file_name, Net *net);

  void allocateInlets(int nInlets);
  void allocateOutlets(int nOutlets);

  double lbmConvertPressureGradToLatticeUnits (double pressure_grad);
  double lbmConvertPressureGradToPhysicalUnits (double pressure_grad);

  hemelb::lb::collisions::MidFluidCollision* mMidFluidCollision;
  hemelb::lb::collisions::WallCollision* mWallCollision;
  hemelb::lb::collisions::InletOutletCollision* mInletCollision;
  hemelb::lb::collisions::InletOutletCollision* mOutletCollision;
  hemelb::lb::collisions::InletOutletWallCollision* mInletWallCollision;
  hemelb::lb::collisions::InletOutletWallCollision* mOutletWallCollision;

  //TODO Get rid of this hack
  hemelb::lb::collisions::Collision* GetCollision(int i);
};

void lbmFeq (double f[], double *density, double *v_x, double *v_y, double *v_z, double f_eq[]);
void lbmFeq (double density, double v_x, double v_y, double v_z, double f_eq[]);
void lbmDensityAndVelocity (double f[], double *density, double *v_x, double *v_y, double *v_z);
void lbmVonMisesStress (double f[], double *stress);
void lbmShearStress (double density, double f[], double nor[], double *stress);
void lbmInitMinMaxValues (void);
void lbmUpdateMinMaxValues (double density, double velocity, double stress);
void lbmCalculateBC (double f[], unsigned int site_data, double *density, double *vx, double *vy, double *vz, double f_neq[]);
unsigned int lbmCollisionType (unsigned int site_data);
void lbmUpdateFlowField (int perform_rt, int i, double density, double vx, double vy, double vz, double f_neq[]);
void lbmUpdateFlowFieldConv (int perform_rt, int i, double density, double vx, double vy, double vz, double f_neq[]);
int lbmIsUnstable (Net *net);


// TODO moving these requires making the collision / streaming functions non-static, which is potentially a big deal.
extern double* inlet_density, *outlet_density;

extern double lbm_stress_type;
extern double lbm_stress_par;
extern double lbm_density_min, lbm_density_max;
extern double lbm_velocity_min, lbm_velocity_max;
extern double lbm_stress_min, lbm_stress_max;
extern double lbm_phys_pressure_min, lbm_phys_pressure_max;
extern double lbm_phys_velocity_min, lbm_phys_velocity_max;
extern double lbm_phys_stress_min, lbm_phys_stress_max;
extern double *lbm_average_inlet_velocity;
extern double *lbm_peak_inlet_velocity;
extern double *lbm_inlet_normal;
extern long int *lbm_inlet_count;

extern int lbm_terminate_simulation;

extern int is_inlet_normal_available;

// TODO: prob don't belong here... 3 variables needed for convergence-enabled simulations
extern int cycle_tag;
#endif // HEMELB_LB_H
