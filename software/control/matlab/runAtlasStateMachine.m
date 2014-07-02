function runAtlasStateMachine(controller_type)

if nargin < 1
  controller_type = 2; % 1: PID, 2: PID+manip params, 3: PD+gravity comp, 4: inverse dynamics
end

% silence some warnings
warning('off','Drake:RigidBodyManipulator:UnsupportedContactPoints')
warning('off','Drake:RigidBodyManipulator:UnsupportedJointLimits')
warning('off','Drake:RigidBodyManipulator:UnsupportedVelocityLimits')
options.visual = false; % loads faster
options.floating = true;
options.ignore_friction = true;
r = Atlas(strcat(getenv('DRC_PATH'),'/models/mit_gazebo_models/mit_robot_drake/model_minimal_contact_point_hands.urdf'),options);

options.controller_type = controller_type;
init_controller = SilentInitController('init',r);
manip_controller = AtlasManipController('manip',r,options);
standing_controller = AtlasBalancingController('stand',r,options);
walking_controller = AtlasWalkingController('walk',r,options);
state_machine = DRCStateMachine(struct(manip_controller.name,manip_controller,...
  init_controller.name,init_controller,standing_controller.name,standing_controller,...
  walking_controller.name,walking_controller),init_controller.name);

state_machine.run();

end
