/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2012, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Ioan Sucan */

#include <moveit/planning_interface/planning_interface.hpp>
#include <mutex>
#include <rclcpp/logger.hpp>
#include <rclcpp/logging.hpp>
#include <set>
#include <moveit/utils/logger.hpp>

namespace planning_interface
{
namespace
{
rclcpp::Logger getLogger()
{
  return moveit::getLogger("moveit.core.planning_interface");
}
}  // namespace

namespace
{
// keep track of currently active contexts
struct ActiveContexts
{
  std::mutex mutex_;
  std::set<PlanningContext*> contexts_;
};

ActiveContexts& getActiveContexts()
{
  static ActiveContexts s_ac;
  return s_ac;
}
}  // namespace

PlanningContext::PlanningContext(const std::string& name, const std::string& group) : name_(name), group_(group)
{
  ActiveContexts& ac = getActiveContexts();
  std::scoped_lock _(ac.mutex_);
  ac.contexts_.insert(this);
}

PlanningContext::~PlanningContext()
{
  ActiveContexts& ac = getActiveContexts();
  std::scoped_lock _(ac.mutex_);
  ac.contexts_.erase(this);
}

void PlanningContext::setPlanningScene(const planning_scene::PlanningSceneConstPtr& planning_scene)
{
  planning_scene_ = planning_scene;
}

void PlanningContext::setMotionPlanRequest(const MotionPlanRequest& request)
{
  request_ = request;
  if (request_.allowed_planning_time <= 0.0)
  {
    RCLCPP_INFO(getLogger(), "The timeout for planning must be positive (%lf specified). Assuming one second instead.",
                request_.allowed_planning_time);
    request_.allowed_planning_time = 1.0;
  }
  if (request_.num_planning_attempts < 0)
  {
    RCLCPP_ERROR(getLogger(), "The number of desired planning attempts should be positive. "
                              "Assuming one attempt.");
  }
  request_.num_planning_attempts = std::max(1, request_.num_planning_attempts);
}

bool PlannerManager::initialize(const moveit::core::RobotModelConstPtr& /*unused*/,
                                const rclcpp::Node::SharedPtr& /* unused */, const std::string& /* unused */)
{
  return true;
}

std::string PlannerManager::getDescription() const
{
  return "";
}

PlanningContextPtr PlannerManager::getPlanningContext(const planning_scene::PlanningSceneConstPtr& planning_scene,
                                                      const MotionPlanRequest& req) const
{
  moveit_msgs::msg::MoveItErrorCodes dummy;
  return getPlanningContext(planning_scene, req, dummy);
}

void PlannerManager::getPlanningAlgorithms(std::vector<std::string>& algs) const
{
  // nothing by default
  algs.clear();
}

void PlannerManager::setPlannerConfigurations(const PlannerConfigurationMap& pcs)
{
  config_settings_ = pcs;
}

void PlannerManager::terminate() const
{
  ActiveContexts& ac = getActiveContexts();
  std::scoped_lock _(ac.mutex_);
  for (PlanningContext* context : ac.contexts_)
    context->terminate();
}

}  // end of namespace planning_interface
