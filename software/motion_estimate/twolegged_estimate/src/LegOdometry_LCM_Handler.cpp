
/*
 * Refer to "LegOdometry_LCM_Handler.h" for comments on the purpose of each function and member, comments here in the .cpp relate
 * to more development issues and specifics. The user only need worry about the definitions and descriptions given in the header file,
 * assuming a good software design was done.
 * d fourie
 * 3/24/2013
*/

#include <iostream>
#include <exception>
//#include <stdio.h>
//#include <inttypes.h>

#include "LegOdometry_LCM_Handler.hpp"



using namespace TwoLegs;
using namespace std;

LegOdometry_Handler::LegOdometry_Handler(boost::shared_ptr<lcm::LCM> &lcm_) : _finish(false), lcm_(lcm_) {
	// Create the object we want to use to estimate the robot's pelvis position
	// In this case its a two legged vehicle and we use TwoLegOdometry class for this task
	_leg_odo = new TwoLegOdometry();
	
	if(!lcm_->good())
	  return;
	
	model_ = boost::shared_ptr<ModelClient>(new ModelClient(lcm_->getUnderlyingLCM(), 0));
	
	lcm_->subscribe("TRUE_ROBOT_STATE",&LegOdometry_Handler::robot_state_handler,this); 
	
	// Parse KDL tree
	  if (!kdl_parser::treeFromString(  model_->getURDFString() ,tree)){
	    std::cerr << "ERROR: Failed to extract kdl tree from xml robot description" << std::endl;
	    return;
	  }
	  std::cout << "Before\n";
	  fksolver_ = boost::shared_ptr<KDL::TreeFkSolverPosFull_recursive>(new KDL::TreeFkSolverPosFull_recursive(tree));

	
	stillbusy = false;
	
	// This is for viewing results in the collections_viewer. check delete of new memory
	lcm_viewer = lcm_create(NULL);
	
	poseplotcounter = 0;
	collectionindex = 101;
	_obj = new ObjectCollection(1, std::string("Objects"), VS_OBJ_COLLECTION_T_POSE3D);
	
	return;
}

LegOdometry_Handler::~LegOdometry_Handler() {
	
	//delete model_;
	delete _leg_odo;
	delete _obj;
	
	lcm_destroy(lcm_viewer); //destroy viewer memory at executable end
	
	
	cout << "Everything Destroyed in LegOdometry_Handler::~LegOdometry_Handler()" << endl;
	return;
}

void LegOdometry_Handler::setupLCM() {
	
//	_lcm = lcm_create(NULL);
	// TODO
	// robot_pose_channel = "TRUE_ROBOT_STATE";
	// drc_robot_state_t_subscribe(_lcm, robot_pose_channel, TwoLegOdometry::on_robot_state_aux, this);
	
	
	return;
}
			

void LegOdometry_Handler::run(bool testingmode) {
	
	// TODO
	cout << "LegOdometry_Handler::run(bool) is NOT finished yet." << endl;
	
	if (testingmode)
	{
		cout << "LegOdometry_Handler::run(bool) in tesing mode." << endl;
		
		for (int i = 0 ; i<10 ; i++)
		{
			_leg_odo->CalculateBodyStates_Testing(i);
			
		}
		
		
	}
	else
	{
		cout << "Attempting to start lcm_handle loop..." << endl;
		
		try
		{
			// This is the highest referrence point for the 
			//This is in main now...
			//while(0 == lcm_->handle());
		    
		}
		catch (exception& e)
		{
			cout << "LegOdometry_Handler::run() - Oops something went wrong when we tried to listen and respond to a new lcm message:" << endl;
			cout << e.what() << endl;
			
		}
	}
	
	return;
}


//void LegOdometry_Handler::robot_state_handler(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const  drc::robot_state_t* msg) {

void LegOdometry_Handler::robot_state_handler(	const lcm::ReceiveBuffer* rbuf, 
												const std::string& channel, 
												const  drc::robot_state_t* msg) {
	
	/*
	std::cout << msg->utime << ", ";
	std::cout << msg->contacts.contact_force[0].z << ", ";
	std::cout << msg->contacts.contact_force[1].z << ", ";*/
	
	//pass left and right leg forces and torques to TwoLegOdometry
	// TODO temporary testing interface
	

	getTransforms(msg);

	Eigen::Isometry3d currentPelvis;
	currentPelvis = _leg_odo->getPelvisFromStep();
	
	//_leg_odo->setPelvisPosition(currentPelvis);
	
	_leg_odo->DetectFootTransistion(msg->utime, msg->contacts.contact_force[1].z , msg->contacts.contact_force[0].z);

	/*
	if (_leg_odo->primary_foot() == LEFTFOOT)
		 std::cout << "LEFT  ";// << std:: endl;
	else
		std::cout << "RIGHT ";
	//std::cout << std::endl;
	*/
	//std::cout << _leg_odo->primary_foot() << ", ";
	//std::cout << _leg_odo->getPrimaryInLocal().translation().transpose() << ", ";
	//std::cout << "Pelvis is at   : ";
	//std::cout << currentPelvis.translation().transpose() << ", ";
	
	//std::cout << _leg_odo->pelvis_to_left.translation().transpose() << ", ";
	//std::cout << _leg_odo->left_to_pelvis.translation().transpose() << ", ";
	
	//std::cout << "Secondary is at: " << _leg_odo->getSecondaryInLocal().translation().transpose() << std::endl;
	//std::cout << std::endl;
	
	
	// here comes the drawing of poses
	
	//LinkCollection link(2, std::string("Links"));
	poseplotcounter++;
	//std::cout << poseplotcounter << std::endl;
	_obj->add(100, isam::Pose3d(currentPelvis.translation().x(),-currentPelvis.translation().y(),-currentPelvis.translation().z(),0,0,0));
	
	// TODO - This is highly inefficient
	Viewer viewer(lcm_viewer);
	if ((poseplotcounter)%1000 == 0)
	{
		poseplotcounter = 0;
		_obj->add(collectionindex++, isam::Pose3d(currentPelvis.translation().x(),-currentPelvis.translation().y(),-currentPelvis.translation().z(),0,0,0));
	}
	viewer.sendCollection(*_obj, true);

        
        bot_core::pose_t pose;
        pose.pos[0] =currentPelvis.translation().x();
        pose.pos[1] =-currentPelvis.translation().y();
        pose.pos[2] =-currentPelvis.translation().z();
        pose.orientation[0] =1.;
        pose.orientation[1] =0.;
        pose.orientation[2] =0.;
        pose.orientation[3] =0.;
        lcm_->publish("POSE_KIN",&pose);

	
}

void LegOdometry_Handler::getTransforms(const drc::robot_state_t * msg) {
  bool kinematics_status;
  bool flatten_tree=true; // determines absolute transforms to robot origin, otherwise relative transforms between joints.
  
  // 1. Solve for Forward Kinematics:
    _link_tfs.clear();
    
    // call a routine that calculates the transforms the joint_state_t* msg.
    map<string, double> jointpos_in;
    map<string, drc::transform_t > cartpos_out;
    
        
    
    for (uint i=0; i< (uint) msg->num_joints; i++) //cast to uint to suppress compiler warning
      jointpos_in.insert(make_pair(msg->joint_name[i], msg->joint_position[i]));
   
    if (!stillbusy)
    {
    	//std::cout << "Trying to solve for Joints to Cartesian\n";
    	stillbusy = true;
    	kinematics_status = fksolver_->JntToCart(jointpos_in,cartpos_out,flatten_tree);
    	stillbusy = false;
    }
    else
    {
    	std::cout << "JntToCart is still busy" << std::endl;
    	// This should generate some type of error or serious warning
    }
    
    //bot_core::rigid_transform_t tf;
    //KDL::Frame T_body_head;
    
    map<string, drc::transform_t >::iterator transform_it_lf;
    map<string, drc::transform_t >::iterator transform_it_rf;
    
    transform_it_lf=cartpos_out.find("l_foot");
    transform_it_rf=cartpos_out.find("r_foot");
    
    //T_body_head = KDL::Frame::Identity();
	  if(transform_it_lf!=cartpos_out.end()){// fk cart pos exists
		// This gives us the translation from body to left foot
#ifdef VERBOSE_DEBUG
	    std::cout << " LEFT: " << transform_it_lf->second.translation.x << ", " << transform_it_lf->second.translation.y << ", " << transform_it_lf->second.translation.z << std::endl;
#endif
	    
	    //std::cout << "ROTATION.x: " << transform_it->second.rotation.x << ", " << transform_it->second.rotation.y << std::endl;
	  }else{
	    std::cout<< "fk position does not exist" <<std::endl;
	  }
	  
	  if(transform_it_lf!=cartpos_out.end()){// fk cart pos exists
#ifdef VERBOSE_DEBUG
  	    std::cout << "RIGHT: " << transform_it_rf->second.translation.x << ", " << transform_it_rf->second.translation.y << ", " << transform_it_rf->second.translation.z << std::endl;
#endif
  	    transform_it_rf->second.rotation;
	  }else{
        std::cout<< "fk position does not exist" <<std::endl;
  	  }
    
	  Eigen::Isometry3d left;
	  Eigen::Isometry3d right;
	  
	  left.translation() << transform_it_lf->second.translation.x, transform_it_lf->second.translation.y, transform_it_lf->second.translation.z;
	  right.translation() << transform_it_rf->second.translation.x, transform_it_rf->second.translation.y, transform_it_rf->second.translation.z;

	  // TODO - Confirm the quaternion scale and vector ordering is correct and the ->second pointer is the correct use
	  Eigen::Quaterniond  leftq(transform_it_lf->second.rotation.w, transform_it_lf->second.rotation.x,transform_it_lf->second.rotation.y,transform_it_lf->second.rotation.z);
	  Eigen::Quaterniond rightq(transform_it_rf->second.rotation.w, transform_it_rf->second.rotation.x,transform_it_rf->second.rotation.y,transform_it_rf->second.rotation.z);
	  	  
	  left.rotate(leftq);
	  right.rotate(rightq);
	  
	  // TODo - ensure the ordering is correct here
	  _leg_odo->setLegTransforms(right, left);
}

