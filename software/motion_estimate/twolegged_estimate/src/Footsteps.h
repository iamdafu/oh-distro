#ifndef FOOTSTEPS_H_
#define FOOTSTEPS_H_

#include <vector>

#include <Eigen/Dense>

#include "TwoLegsEstimate_types.h"

#include "lcmtypes/drc_lcmtypes.hpp"


namespace TwoLegs {

class Footsteps {
	private:
		footstep active_step;
		std::vector<footstep> footstep_hist;
		int thisisatestvariable;
		
	public:
		EIGEN_MAKE_ALIGNED_OPERATOR_NEW
		
		Footsteps();
		void addFootstep(const Eigen::Isometry3d &RelativeFrameLocation, const int &foot);
		void newFootstep(footstep newstep);
		
		int getLastStepState(state const &RelativeFrameLocation);
	
		Eigen::Isometry3d getLastStep();
		
		int lastFoot();
		void reset();
};

}

#endif /*FOOTSTEPS_H_*/
