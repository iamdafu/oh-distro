#include "laser_gpf_lib.hpp"
#include <path_util/path_util.h>

const std::string LaserGPF::laser_gpf_substate_stings[num_substates] = {
    "pos_only",
    "pos_yaw",
    "pos_chi",
    "all_states",
    "z_only",
    "xy_only" };

#include <lcmtypes/octomap_utils.h>

#define MIN_VALID_BEAMS 5

using namespace std;
using namespace Eigen;
using namespace eigen_utils;

void LaserGPF::LaserGPFBaseConstructor(int num_samples, bool gpf_vis, LaserLikelihoodInterface * likelihood_interface,
    laser_gpf_substate gpf_orientation_mode, lcm_t * lcm, BotParam * param, BotFrames * frames)
{

  this->lcm = lcm;
  this->frames = frames;
  this->param = param;

  this->verbose = false;
  this->motion_project = true;

  this->num_samples = num_samples;
  this->laser_like_iface = likelihood_interface;
  this->gpf_substate_mode = gpf_orientation_mode;

  this->beam_skip = bot_param_get_int_or_fail(param, "state_estimator.laser_gpf.beam_skip"); //FIXME important hard coded parameter
  this->spatial_decimation_min = bot_param_get_double_or_fail(param, "state_estimator.laser_gpf.spatial_decimation_min");
  this->spatial_decimation_max = bot_param_get_double_or_fail(param, "state_estimator.laser_gpf.spatial_decimation_max");

  //  //--- publish the map (blurred, not ideal)
//  octomap_raw_t msg;
//  msg.utime = bot_timestamp_now();
//
//  std::stringstream datastream;
//  ocTree->writeBinaryConst(datastream);
//  std::string datastring = datastream.str();
//  msg.data = (uint8_t *) datastring.c_str();
//  msg.length = datastring.size();
//
//  octomap_raw_t_publish(lcm, "OCTOMAP", &msg);
//  //----------------------------------------------------

  this->laser_projector = laser_projector_new(this->param, this->frames, "laser", 1); //TODO: laser name should be a param

  BotTrans body_to_laser;
  bot_frames_get_trans(this->frames, "body", this->laser_projector->coord_frame, &body_to_laser);
  Quaterniond body_to_laser_quat;
  botDoubleToQuaternion(body_to_laser_quat, body_to_laser.rot_quat);
  this->R_body_to_laser = body_to_laser_quat.toRotationMatrix();

  int delta_start_ind = RBIS::position_ind;
  int chi_start_ind = RBIS::chi_ind;

  fprintf(stderr, "LaserGpf is using the " "%s" " substate mode\n",
      laser_gpf_substate_stings[this->gpf_substate_mode].c_str());

  switch (this->gpf_substate_mode) {
  case pos_only:
    this->laser_gpf_measurement_indices = VectorXi::Zero(3);
    this->laser_gpf_measurement_indices = RBIS::positionInds();
    break;
  case pos_yaw:
    this->laser_gpf_measurement_indices = VectorXi(4);
    this->laser_gpf_measurement_indices.bottomRows(3) = RBIS::positionInds();
    this->laser_gpf_measurement_indices(0) = RBIS::chiInds()[2];
    break;
  case pos_chi:
    this->laser_gpf_measurement_indices = VectorXi(6);
    this->laser_gpf_measurement_indices.topRows(3) = RBIS::chiInds();
    this->laser_gpf_measurement_indices.bottomRows(3) = RBIS::positionInds();
    break;
  case all_states:
    this->laser_gpf_measurement_indices = VectorXi(9);
    this->laser_gpf_measurement_indices.segment < 3 > (0) = RBIS::velocityInds();
    this->laser_gpf_measurement_indices.segment < 3 > (3) = RBIS::chiInds();
    this->laser_gpf_measurement_indices.segment < 3 > (6) = RBIS::positionInds();
    break;
  case z_only:
    this->laser_gpf_measurement_indices = VectorXi(1);
    this->laser_gpf_measurement_indices(0) = RBIS::position_ind + 2;
    break;
  default:
    assert(false);
    break;
  }

  eigen_dump(laser_gpf_measurement_indices);

  if (gpf_vis) {
    this->lcmgl_laser = bot_lcmgl_init(this->lcm, "GPF_laser");
    this->lcmgl_particles = bot_lcmgl_init(this->lcm, "GPF_particles");
  }
  else {
    this->lcmgl_laser = NULL;
    this->lcmgl_particles = NULL;
  }

}

LaserGPF::LaserGPF(lcm_t * lcm, BotParam * param, BotFrames * frames)
{

  //param loading
  int num_samples_ = bot_param_get_int_or_fail(param, "state_estimator.laser_gpf.gpf_num_samples");
  bool gpf_vis = bot_param_get_boolean_or_fail(param, "state_estimator.laser_gpf.gpf_vis");

  char * tmpstr = bot_param_get_str_or_fail(param, "state_estimator.laser_gpf.map_name");
  std::string data_dir = getDataPath();
  string map_name;
  if (g_path_is_absolute (tmpstr))
    map_name = tmpstr;
  else if (g_str_has_prefix(tmpstr, "~"))
    map_name = g_build_filename(g_get_home_dir(), tmpstr + 1, NULL);
  else
    map_name = data_dir + "/" + tmpstr;

  free(tmpstr);

  double unknown_loglike = bot_param_get_double_or_fail(param, "state_estimator.laser_gpf.unknown_loglike");
  double cov_scaling = bot_sq(bot_param_get_double_or_fail(param, "state_estimator.laser_gpf.sigma_scaling"));
  LaserLikelihoodInterface * laser_like_iface_ = new OctomapLikelihoodInterface(map_name.c_str(), unknown_loglike, cov_scaling);


  laser_gpf_substate gpf_substate_mode_ = LaserGPF::num_substates;
  char * substate_str = bot_param_get_str_or_fail(param, "state_estimator.laser_gpf.gpf_substate");
  for (int i = 0; i < LaserGPF::num_substates; i++) {
    if (substate_str == LaserGPF::laser_gpf_substate_stings[i]) {
      gpf_substate_mode_ = (LaserGPF::laser_gpf_substate) i;
      break;
    }
  }
  if (gpf_substate_mode_ == LaserGPF::num_substates) {
    fprintf(stderr, "ERROR: %s is not a valid gpf substate!\n", substate_str);
    exit(1);
  }
  free(substate_str); // the string returned by bot_param needs to be freed

  LaserGPFBaseConstructor(num_samples_, gpf_vis, laser_like_iface_, gpf_substate_mode_, lcm, param, frames);

}

LaserGPF::~LaserGPF()
{
  laser_projector_destroy(laser_projector);
  delete laser_like_iface;

  if (this->lcmgl_laser != NULL) {
    bot_lcmgl_destroy(lcmgl_laser);
  }
}

double LaserGPF::likelihoodFunction(const RBIS & state)
{

  BotTrans state_to_map;
  state.getBotTrans(&state_to_map);
  double likelihood = this->laser_like_iface->evaluateScanLogLikelihood(this->projected_laser_scan, state);

  if (this->lcmgl_laser != NULL) {

    bot_lcmgl_point_size(this->lcmgl_laser, 2);
    bot_lcmgl_begin(this->lcmgl_laser, GL_POINTS);

    double point_likelihood = 0;
    for (int i = 0; i < this->projected_laser_scan->npoints; i++) {
      if (this->projected_laser_scan->point_status[i] > laser_valid_projection)
        continue;
      double proj_xyz[3];
      bot_trans_apply_vec(&state_to_map, point3d_as_array(&this->projected_laser_scan->points[i]), proj_xyz);
      point_likelihood = this->laser_like_iface->evaluatePointLogLikelihood(proj_xyz);
      //      point_likelihood = getOctomapLogLikelihood(this->laser_likelihood_interface->ocTree, proj_xyz);
      float * color = bot_color_util_jet(exp(point_likelihood + this->laser_like_iface->minNegLogLike));
      bot_lcmgl_color3f(this->lcmgl_laser, color[0], color[1], color[2]);
      bot_lcmgl_vertex3f(this->lcmgl_laser, proj_xyz[0], proj_xyz[1], proj_xyz[2]);

    }
    bot_lcmgl_end(this->lcmgl_laser);

  }

  return likelihood;
}

bool LaserGPF::getMeasurement(const RBIS & state, const RBIM & cov, const bot_core_planar_lidar_t * laser_msg,
    Eigen::VectorXd & z_effective, Eigen::MatrixXd & R_effective)
{
  printf("mfallon: LaserGPF::getMeasurement\n");

  if (this->motion_project) {
    Vector3d laser_omega = this->R_body_to_laser * state.angularVelocity();
    Vector3d laser_vel = this->R_body_to_laser * state.velocity();
    this->projected_laser_scan = laser_create_projected_scan_from_planar_lidar_with_motion(this->laser_projector,
        laser_msg, "body", laser_omega.data(), laser_vel.data());
  }
  else {
    this->projected_laser_scan = laser_create_projected_scan_from_planar_lidar(this->laser_projector, laser_msg,
        "body");
  }
  if (this->projected_laser_scan->numValidPoints < MIN_VALID_BEAMS) {
    if (this->verbose) {
      fprintf(stderr, "Not enough valid beams (%d), discarding scan!\n", this->projected_laser_scan->numValidPoints);
      laser_destroy_projected_scan(this->projected_laser_scan);
      return false;
    }
  }

  laser_decimate_projected_scan(this->projected_laser_scan, this->beam_skip, this->spatial_decimation_min, this->spatial_decimation_max);

  gpfMeasurement(this, state, cov, this->laser_gpf_measurement_indices, z_effective, R_effective, this->num_samples,
      this->lcmgl_particles);
//
//    gpfMeasurementRzx(this, state, cov, this->laser_gpf_measurement_indices, z_effective, R_effective, this->num_samples,
//        NULL);

  laser_destroy_projected_scan(this->projected_laser_scan);

  if (this->lcmgl_laser != NULL) {
    bot_lcmgl_switch_buffer(this->lcmgl_laser);
  }
  if (this->lcmgl_particles != NULL) {
    bot_lcmgl_switch_buffer(this->lcmgl_particles);
  }
  bool ret = true;
  if (hasNan(z_effective)) {
    fprintf(stderr, "ERROR: z_effective has a Nan!\n");
    eigen_dump(z_effective.transpose());
    ret = false;
  }
  if (hasNan(R_effective)) {
    fprintf(stderr, "ERROR: R_effective has a Nan!\n");
    eigen_dump(R_effective);
    ret = false;
  }

  return ret;
}

