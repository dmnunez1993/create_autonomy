#!/usr/bin/env python

import os
import re
import rospy
from random import randint
from geometry_msgs.msg import Pose, Quaternion, PoseWithCovarianceStamped
from gazebo_msgs.srv import SpawnModel, SpawnModelRequest, SpawnModelResponse
from tf.transformations import quaternion_from_euler

class RobotSpawner(object):
  
  SPAWN_URDF_TOPIC = '/gazebo/spawn_urdf_model'

  def __init__(self):
    rospy.init_node('robot_spawner')
    self.ns = rospy.get_namespace()

    # Get robot index
    try:
      index = re.findall('[0-9]+', self.ns)[0]
    except IndexError:
      index = 0
    i = index
    
    # Spawn URDF service client
    rospy.wait_for_service(RobotSpawner.SPAWN_URDF_TOPIC)
    try:
      spawn_urdf_model = rospy.ServiceProxy(RobotSpawner.SPAWN_URDF_TOPIC, SpawnModel)
      
      # Filling model spawner request
      msg = SpawnModelRequest()
      
      # Model name
      msg.model_name = "irobot_create2.{}".format(i)
      
      # Robot information from robot_description
      robot_description_param = "/create{}/robot_description".format(i)
      if rospy.has_param(robot_description_param):
        msg.model_xml = rospy.get_param(robot_description_param)
      
      msg.robot_namespace = self.ns
      
      # Using pose from parameter server
      msg.initial_pose = Pose()
      msg.initial_pose.position.x = rospy.get_param(
            "{}/amcl/initial_pose_x".format(self.ns), randint(-10, 10))
      msg.initial_pose.position.y = rospy.get_param(
            "{}/amcl/initial_pose_y".format(self.ns), randint(-10, 10))
      yaw = rospy.get_param(
            "{}/amcl/initial_pose_a".format(self.ns), 0.)
      q = quaternion_from_euler(0, 0, yaw)
      msg.initial_pose.orientation = Quaternion(q[0], q[1], q[2], q[3])
      
      msg.reference_frame = "world"

      # Spawn model
      res = spawn_urdf_model(msg)
      print(res.status_message)
    except rospy.ServiceException:
      print("Could not spawn {}".format(msg.model_name))
      exit(1)
    
    print("{} spawned correctly".format(msg.model_name))

def main():
  try:
    RobotSpawner()
  except rospy.ROSInterruptException: pass

if __name__ == "__main__":
  main()