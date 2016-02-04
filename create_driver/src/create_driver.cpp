#include <tf/transform_datatypes.h>
#include "create_driver/create_driver.h"

CreateDriver::CreateDriver(ros::NodeHandle& nh_) : nh(nh_), privNh("~") {
  privNh.param<double>("loop_hz", loopHz, 10);
  privNh.param<std::string>("dev", dev, "/dev/ttyUSB0");
  privNh.param<int>("baud", baud, 115200);
  privNh.param<double>("latch_cmd_duration", latchDuration, 0.5);

  ROS_INFO("[CREATE] loop hz: %.2f", loopHz);
  robot = new create::Create();

  if (!robot->connect(dev, baud)) {
    ROS_FATAL("[CREATE] Failed to establish serial connection with Create.");
    ros::shutdown();
  }
 
  ROS_INFO("[CREATE] Connection established.");
  
  // Put into full control mode
  //TODO: Make option to run in safe mode as parameter
  robot->setMode(create::MODE_FULL);

  // Show robot's battery level
  ROS_INFO("[CREATE] Battery level %.2f %%", (robot->getBatteryCharge() / (float)robot->getBatteryCapacity()) * 100.0);

  tfOdom.header.frame_id = "odom";
  tfOdom.child_frame_id = "base_footprint";
  odom.header.frame_id = "odom";
  odom.child_frame_id = "base_footprint";

  cmdVelSub = nh.subscribe(std::string("cmd_vel"), 1, &CreateDriver::cmdVelCallback, this);

  odomPub = nh.advertise<nav_msgs::Odometry>(std::string("odom"), 10);
}

CreateDriver::~CreateDriver() { 
  ROS_INFO("[CREATE] Destruct sequence initiated.");
  robot->disconnect();
  delete robot;
}

void CreateDriver::cmdVelCallback(const geometry_msgs::TwistConstPtr& msg) {
  robot->drive(msg->linear.x, msg->angular.z);
  lastCmdVelTime = ros::Time::now();
}

bool CreateDriver::update() {
  publishOdom();

  // If last velocity command was sent longer than latch duration, stop robot
  if (ros::Time::now() - lastCmdVelTime >= ros::Duration(latchDuration)) {
    robot->drive(0, 0);
  }

  return true;
}

void CreateDriver::publishOdom() {
  create::Pose pose = robot->getPose();
  create::Vel vel = robot->getVel();

  // Populate position info
  geometry_msgs::Quaternion quat = tf::createQuaternionMsgFromRollPitchYaw(0, 0, pose.yaw);
  odom.pose.pose.position.x = pose.x;
  odom.pose.pose.position.y = pose.y;
  odom.pose.pose.orientation = quat;
  tfOdom.header.stamp = ros::Time::now();
  tfOdom.transform.translation.x = pose.x;
  tfOdom.transform.translation.y = pose.y;
  tfOdom.transform.rotation = quat;

  // Populate velocity info
  odom.twist.twist.linear.x = vel.x;
  odom.twist.twist.linear.y = vel.y;
  odom.twist.twist.angular.z = vel.yaw;
  
  // TODO: Populate covariances
  //odom.pose.covariance = ?
  //odom.twist.covariance = ?
  
  tfBroadcaster.sendTransform(tfOdom);
  odomPub.publish(odom);
}

void CreateDriver::spinOnce() {
  update();
  ros::spinOnce();
}

void CreateDriver::spin() {
  ros::Rate rate(loopHz);
  while (ros::ok()) {
    spinOnce();
    if (!rate.sleep()) {
      ROS_WARN("[CREATE] Loop running slowly.");
    }
  }
}

int main(int argc, char** argv) {
  ros::init(argc, argv, "create_driver");
  ros::NodeHandle nh;

  CreateDriver createDriver(nh);

  try {
    createDriver.spin();
  }
  catch (std::runtime_error& ex) {
    ROS_FATAL_STREAM("[CREATE] Runtime error: " << ex.what());
    return 1;
  }
  return 0;
}
