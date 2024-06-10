/**
 * \file Sai2Graphics.cpp
 *
 *  Created on: Dec 30, 2016
 *      Author: Shameek Ganguly
 */

#include "Sai2Graphics.h"

#include <deque>
#include <iostream>
#include <unordered_map>

#ifdef MACOSX
#include <filesystem>
#endif

#include "parser/UrdfToSai2Graphics.h"

using namespace std;
using namespace chai3d;

namespace {
// custom aliases that capture the key function
#define ZOOM_IN_KEY GLFW_KEY_A
#define ZOOM_OUT_KEY GLFW_KEY_Z
#define CAMERA_RIGHT_KEY GLFW_KEY_RIGHT
#define CAMERA_LEFT_KEY GLFW_KEY_LEFT
#define CAMERA_UP_KEY GLFW_KEY_UP
#define CAMERA_DOWN_KEY GLFW_KEY_DOWN
#define NEXT_CAMERA_KEY GLFW_KEY_N
#define PREV_CAMERA_KEY GLFW_KEY_B
#define SHOW_CAMERA_POS_KEY GLFW_KEY_S

// map to store key presses. The first bool is true if the key is pressed and
// false otherwise, the second bool is used as a flag to know if the initial key
// press has been consumed when something needs to happen only once when the key
// is pressed and not continuously
std::unordered_map<int, std::pair<bool, bool>> key_presses_map = {
	{ZOOM_IN_KEY, std::make_pair(false, true)},
	{ZOOM_OUT_KEY, std::make_pair(false, true)},
	{CAMERA_RIGHT_KEY, std::make_pair(false, true)},
	{CAMERA_LEFT_KEY, std::make_pair(false, true)},
	{CAMERA_UP_KEY, std::make_pair(false, true)},
	{CAMERA_DOWN_KEY, std::make_pair(false, true)},
	{NEXT_CAMERA_KEY, std::make_pair(false, true)},
	{PREV_CAMERA_KEY, std::make_pair(false, true)},
	{SHOW_CAMERA_POS_KEY, std::make_pair(false, true)},
	{GLFW_KEY_LEFT_SHIFT, std::make_pair(false, true)},
	{GLFW_KEY_LEFT_ALT, std::make_pair(false, true)},
	{GLFW_KEY_LEFT_CONTROL, std::make_pair(false, true)},
};

std::unordered_map<int, std::pair<bool, bool>> mouse_button_presses_map = {
	{GLFW_MOUSE_BUTTON_LEFT, std::make_pair(false, true)},
	{GLFW_MOUSE_BUTTON_RIGHT, std::make_pair(false, true)},
	{GLFW_MOUSE_BUTTON_MIDDLE, std::make_pair(false, true)},
};

std::deque<double> mouse_scroll_buffer;

bool is_pressed(int key) {
	if (key_presses_map.count(key) > 0) {
		return key_presses_map.at(key).first;
	}
	if (mouse_button_presses_map.count(key) > 0) {
		return mouse_button_presses_map.at(key).first;
	}
	return false;
}

bool consume_first_press(int key) {
	if (key_presses_map.count(key) > 0) {
		if (key_presses_map.at(key).first && key_presses_map.at(key).second) {
			key_presses_map.at(key).second = false;
			return true;
		}
	}
	if (mouse_button_presses_map.count(key) > 0) {
		if (mouse_button_presses_map.at(key).first &&
			mouse_button_presses_map.at(key).second) {
			mouse_button_presses_map.at(key).second = false;
			return true;
		}
	}
	return false;
}

// callback to print glfw errors
void glfwError(int error, const char* description) {
	cerr << "GLFW Error: " << description << endl;
	exit(1);
}

// callback when a key is pressed
void keySelect(GLFWwindow* window, int key, int scancode, int action,
			   int mods) {
	bool set = (action != GLFW_RELEASE);
	if (key == GLFW_KEY_ESCAPE) {
		// handle esc separately to exit application
		glfwSetWindowShouldClose(window, GL_TRUE);
	} else {
		if (key_presses_map.count(key) > 0) {
			key_presses_map.at(key).first = set;
			if (!set) {
				key_presses_map.at(key).second = true;
			}
		}
	}
}

// callback when a mouse button is pressed
void mouseClick(GLFWwindow* window, int button, int action, int mods) {
	bool set = (action != GLFW_RELEASE);
	if (mouse_button_presses_map.count(button) > 0) {
		mouse_button_presses_map.at(button).first = set;
		if (!set) {
			mouse_button_presses_map.at(button).second = true;
		}
	}
}

// callback when the mouse wheel is scrolled
void mouseScroll(GLFWwindow* window, double xoffset, double yoffset) {
	if (yoffset != 0) {
		mouse_scroll_buffer.push_back(yoffset);
	} else if (xoffset != 0) {
		mouse_scroll_buffer.push_back(xoffset);
	}
}

GLFWwindow* glfwInitialize(const std::string& window_name) {
	/*------- Set up visualization -------*/
	// set up error callback
	glfwSetErrorCallback(glfwError);

	// initialize GLFW
	glfwInit();

	// retrieve resolution of computer display and position window accordingly
	GLFWmonitor* primary = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(primary);

	// information about computer screen and GLUT display window
	int screenW = mode->width;
	int screenH = mode->height;
	int windowW = 0.8 * screenH;
	int windowH = 0.5 * screenH;
	int windowPosY = (screenH - windowH) / 2;
	int windowPosX = windowPosY;

	// create window and make it current context
	glfwWindowHint(GLFW_VISIBLE, 0);
	GLFWwindow* window =
		glfwCreateWindow(windowW, windowH, window_name.c_str(), NULL, NULL);
	glfwSetWindowPos(window, windowPosX, windowPosY);
	glfwShowWindow(window);
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	return window;
}
}  // namespace

namespace Sai2Graphics {

Sai2Graphics::Sai2Graphics(const std::string& path_to_world_file,
						   const std::string& window_name, 
						   bool verbose, 
						   bool headless) {
	// initialize a chai world
	initializeWorld(path_to_world_file, verbose);

	if (!headless) {
	#ifdef MACOSX
		auto path = std::__fs::filesystem::current_path();
		initializeWindow(window_name);
		std::__fs::filesystem::current_path(path);
	#else
		initializeWindow(window_name);
	#endif
	} 
}

// dtor
Sai2Graphics::~Sai2Graphics() {
	glfwDestroyWindow(_window);
	glfwTerminate();
	clearWorld();
	_world = NULL;
}

void Sai2Graphics::resetWorld(const std::string& path_to_world_file,
							  const bool verbose) {
	clearWorld();
	initializeWorld(path_to_world_file, verbose);
}

void Sai2Graphics::initializeWorld(const std::string& path_to_world_file,
								   const bool verbose) {
	_world = new chai3d::cWorld();
	Parser::UrdfToSai2GraphicsWorld(path_to_world_file, _world,
									_robot_filenames, _object_poses,
									_camera_names, verbose);
	_current_camera_index = 0;
	for (auto robot_filename : _robot_filenames) {
		// get robot base object in chai world
		cRobotBase* base = NULL;
		for (unsigned int i = 0; i < _world->getNumChildren(); ++i) {
			if (robot_filename.first == _world->getChild(i)->m_name) {
				// cast to cRobotBase
				base = dynamic_cast<cRobotBase*>(_world->getChild(i));
				if (base != NULL) {
					break;
				}
			}
		}
		Eigen::Affine3d T_robot_base;
		T_robot_base.translation() = base->getLocalPos().eigen();
		T_robot_base.linear() = base->getLocalRot().eigen();
		_robot_models[robot_filename.first] =
			std::make_shared<Sai2Model::Sai2Model>(robot_filename.second);
		_robot_models[robot_filename.first]->setTRobotBase(T_robot_base);
		updateRobotGraphics(robot_filename.first,
							_robot_models[robot_filename.first]->q());
	}
	for (auto object_pose : _object_poses) {
		_object_velocities[object_pose.first] =
			std::make_shared<Eigen::Vector6d>(Eigen::Vector6d::Zero());
	}
	_right_click_interaction_occurring = false;
}

void Sai2Graphics::clearWorld() {
	delete _world;
	_robot_filenames.clear();
	_robot_models.clear();
	_object_poses.clear();
	_object_velocities.clear();
	_camera_names.clear();
	_force_sensor_displays.clear();
	_ui_force_widgets.clear();
}

void Sai2Graphics::initializeWindow(const std::string& window_name) {
	_window = glfwInitialize(window_name);

	// set callbacks
	glfwSetKeyCallback(_window, keySelect);
	glfwSetMouseButtonCallback(_window, mouseClick);
	glfwSetScrollCallback(_window, mouseScroll);
}

void Sai2Graphics::addForceSensorDisplay(
	const Sai2Model::ForceSensorData& sensor_data) {
	if (!robotExistsInWorld(sensor_data.robot_name,
									sensor_data.link_name)) {
		std::cout << "\n\nWARNING: trying to add a force sensor display to an "
					 "unexisting robot or link in "
					 "Sai2Simulation::addForceSensorDisplay\n"
				  << std::endl;
		return;
	}
	if (findForceSensorDisplay(sensor_data.robot_name, sensor_data.link_name) !=
		-1) {
		std::cout << "\n\nWARNING: only one force sensor is supported per "
					 "link in Sai2Graphics::addForceSensorDisplay. Not "
					 "adding the second one\n"
				  << std::endl;
		return;
	}
	_force_sensor_displays.push_back(std::make_shared<ForceSensorDisplay>(
		sensor_data.robot_name, sensor_data.link_name,
		sensor_data.transform_in_link, _robot_models[sensor_data.robot_name],
		_world));
}

void Sai2Graphics::updateDisplayedForceSensor(
	const Sai2Model::ForceSensorData& force_data) {
	int sensor_index =
		findForceSensorDisplay(force_data.robot_name, force_data.link_name);
	if (sensor_index == -1) {
		throw std::invalid_argument(
			"no force sensor on robot " + force_data.robot_name + " on link " +
			force_data.link_name +
			". Impossible to update the displayed force in graphics world");
		return;
	}
	if (!_force_sensor_displays.at(sensor_index)
			 ->T_link_sensor()
			 .isApprox(force_data.transform_in_link)) {
		throw std::invalid_argument(
			"transformation matrix between link and sensor inconsistent "
			"between the input force_data and the sensor_display in "
			"Sai2Graphics::updateDisplayedForceSensor");
		return;
	}
	_force_sensor_displays.at(sensor_index)
		->update(force_data.force_world_frame, force_data.moment_world_frame);
}

bool Sai2Graphics::robotExistsInWorld(
	const std::string& robot_name, const std::string& link_name) const {
	auto it = _robot_models.find(robot_name);
	if (it == _robot_models.end()) {
		return false;
	}
	if (link_name != "") {
		return _robot_models.at(robot_name)->isLinkInRobot(link_name);
	}
	return true;
}

bool Sai2Graphics::objectExistsInWorld(
	const std::string& object_name) const {
	auto it = _object_poses.find(object_name);
	if (it == _object_poses.end()) {
		return false;
	}
	return true;
}

int Sai2Graphics::findForceSensorDisplay(const std::string& robot_name,
										 const std::string& link_name) const {
	for (int i = 0; i < _force_sensor_displays.size(); ++i) {
		if (_force_sensor_displays.at(i)->robot_name() == robot_name &&
			_force_sensor_displays.at(i)->link_name() == link_name) {
			return i;
		}
	}
	return -1;
}

void Sai2Graphics::addUIForceInteraction(
	const std::string& robot_or_object_name,
	const bool interact_at_object_center) {
	bool is_robot = robotExistsInWorld(robot_or_object_name);
	bool is_object = objectExistsInWorld(robot_or_object_name);
	if (!is_robot && !is_object) {
		throw std::invalid_argument(
			"robot or object not found in Sai2Graphics::addUIForceInteraction");
	}
	for (auto widget : _ui_force_widgets) {
		if (robot_or_object_name == widget->getRobotOrObjectName()) {
			return;
		}
	}
	chai3d::cShapeLine* display_line = new chai3d::cShapeLine();
	_world->addChild(display_line);
	if (is_robot) {
		_ui_force_widgets.push_back(std::make_shared<UIForceWidget>(
			robot_or_object_name, interact_at_object_center,
			_robot_models[robot_or_object_name], display_line));
	} else {
		_ui_force_widgets.push_back(std::make_shared<UIForceWidget>(
			robot_or_object_name, interact_at_object_center,
			_object_poses[robot_or_object_name],
			_object_velocities[robot_or_object_name], display_line));
	}
}

Eigen::VectorXd Sai2Graphics::getUITorques(
	const std::string& robot_or_object_name) {
	bool is_robot = robotExistsInWorld(robot_or_object_name);
	bool is_object = objectExistsInWorld(robot_or_object_name);
	if (!is_robot && !is_object) {
		throw std::invalid_argument(
			"robot or object not found in Sai2Graphics::getUITorques");
	}
	for (auto widget : _ui_force_widgets) {
		if (robot_or_object_name == widget->getRobotOrObjectName()) {
			return widget->getUIJointTorques();
		}
	}
	return is_robot ? Eigen::VectorXd::Zero(
						  _robot_models[robot_or_object_name]->dof())
					: Eigen::VectorXd::Zero(6);
}

const std::vector<std::string> Sai2Graphics::getRobotNames() const {
	std::vector<std::string> robot_names;
	for (const auto& it : _robot_filenames) {
		robot_names.push_back(it.first);
	}
	return robot_names;
}

const std::vector<std::string> Sai2Graphics::getObjectNames() const {
	std::vector<std::string> object_names;
	for (const auto& it : _object_poses) {
		object_names.push_back(it.first);
	}
	return object_names;
}

void Sai2Graphics::renderGraphicsWorld(const std::string& render_camera_name) {

	std::string camera_name = render_camera_name;
	if (render_camera_name == "" ) {
		// swap camera if needed
		if (consume_first_press(NEXT_CAMERA_KEY)) {
			_current_camera_index =
				(_current_camera_index + 1) % _camera_names.size();
		}
		if (consume_first_press(PREV_CAMERA_KEY)) {
			_current_camera_index =
				(_current_camera_index - 1) % _camera_names.size();
		}
		camera_name = _camera_names[_current_camera_index];
	}

	// update graphics. this automatically waits for the correct amount of time
	glfwGetFramebufferSize(_window, &_window_width, &_window_height);
	glfwSwapBuffers(_window);
	glFinish();

	// poll for events
	glfwPollEvents();

	// handle mouse button presses
	Eigen::Vector3d camera_pos, camera_lookat_point, camera_up_axis;
	getCameraPose(camera_name, camera_pos, camera_up_axis, camera_lookat_point);
	Vector3d cam_depth_axis = camera_lookat_point - camera_pos;
	cam_depth_axis.normalize();
	Vector3d cam_right_axis = cam_depth_axis.cross(camera_up_axis);
	cam_right_axis.normalize();

	double cursorx, cursory;
	glfwGetCursorPos(_window, &cursorx, &cursory);
	double mouse_x_increment = (cursorx - _last_cursorx);
	double mouse_y_increment = (cursory - _last_cursory);

	// 0 - scroll event
	double scroll_value = 0.0;
	if (!mouse_scroll_buffer.empty()) {
		scroll_value = mouse_scroll_buffer.front();
		mouse_scroll_buffer.pop_front();
	}

	// 1 - mouse right button to generate a force/torque
	if (is_pressed(GLFW_MOUSE_BUTTON_RIGHT)) {
		if (consume_first_press(GLFW_MOUSE_BUTTON_RIGHT)) {
			for (auto widget : _ui_force_widgets) {
				widget->setEnable(true);
			}
		}

		int wwidth_scr, wheight_scr;
		glfwGetWindowSize(_window, &wwidth_scr, &wheight_scr);

		int viewx = floor(cursorx / wwidth_scr * _window_width);
		int viewy = floor(cursory / wheight_scr * _window_height);

		for (auto widget : _ui_force_widgets) {
			if (widget->getState() == UIForceWidget::Active) {
				_right_click_interaction_occurring = true;
			}

			double depth_change = 0.0;
			if (_right_click_interaction_occurring) {
				depth_change = 0.1 * scroll_value;
				if (is_pressed(ZOOM_IN_KEY)) {
					depth_change += 0.05;
				}
				if (is_pressed(ZOOM_OUT_KEY)) {
					depth_change -= 0.05;
				}
			}
			if (is_pressed(GLFW_KEY_LEFT_SHIFT)) {
				widget->setMomentMode();
			} else {
				widget->setForceMode();
			}
			widget->setInteractionParams(getCamera(camera_name), viewx,
										_window_height - viewy, _window_width,
										_window_height, depth_change);
		}
	} else {
		for (auto widget : _ui_force_widgets) {
			widget->setEnable(false);
		}
		_right_click_interaction_occurring = false;
	}

	// 2 - mouse left button for camera motion
	if (!_right_click_interaction_occurring) {
		if (is_pressed(GLFW_MOUSE_BUTTON_LEFT)) {
			if (is_pressed(GLFW_KEY_LEFT_CONTROL)) {
				Eigen::Vector3d cam_motion =
					0.01 * (mouse_x_increment * cam_right_axis -
							mouse_y_increment * camera_up_axis);
				camera_pos -= cam_motion;
				camera_lookat_point -= cam_motion;
			} else if (is_pressed(GLFW_KEY_LEFT_ALT) ||
					is_pressed(GLFW_KEY_LEFT_SHIFT)) {
				Eigen::Vector3d cam_motion =
					0.02 * mouse_y_increment * cam_depth_axis;
				camera_pos -= cam_motion;
				camera_lookat_point -= cam_motion;
			} else {
				Matrix3d m_tilt;
				m_tilt = AngleAxisd(0.006 * mouse_y_increment, -cam_right_axis);
				camera_pos = camera_lookat_point +
							m_tilt * (camera_pos - camera_lookat_point);
				Matrix3d m_pan;
				m_pan = AngleAxisd(0.006 * mouse_x_increment, -camera_up_axis);
				camera_pos = camera_lookat_point +
							m_pan * (camera_pos - camera_lookat_point);
				camera_up_axis = m_pan * camera_up_axis;
			}
		}
		if (is_pressed(GLFW_MOUSE_BUTTON_MIDDLE)) {
			Eigen::Vector3d cam_motion =
				0.01 * (mouse_x_increment * cam_right_axis -
						mouse_y_increment * camera_up_axis);
			camera_pos -= cam_motion;
			camera_lookat_point -= cam_motion;
		}

		// 3 - mouse scrolling for zoom
		camera_pos += 0.2 * scroll_value * cam_depth_axis;
		camera_lookat_point += 0.2 * scroll_value * cam_depth_axis;

		// handle keyboard key presses
		if (is_pressed(CAMERA_RIGHT_KEY)) {
			camera_pos += 0.05 * cam_right_axis;
			camera_lookat_point += 0.05 * cam_right_axis;
		}
		if (is_pressed(CAMERA_LEFT_KEY)) {
			camera_pos -= 0.05 * cam_right_axis;
			camera_lookat_point -= 0.05 * cam_right_axis;
		}
		if (is_pressed(CAMERA_UP_KEY)) {
			camera_pos += 0.05 * camera_up_axis;
			camera_lookat_point += 0.05 * camera_up_axis;
		}
		if (is_pressed(CAMERA_DOWN_KEY)) {
			camera_pos -= 0.05 * camera_up_axis;
			camera_lookat_point -= 0.05 * camera_up_axis;
		}
		if (is_pressed(ZOOM_IN_KEY)) {
			camera_pos += 0.1 * cam_depth_axis;
			camera_lookat_point += 0.1 * cam_depth_axis;
		}
		if (is_pressed(ZOOM_OUT_KEY)) {
			camera_pos -= 0.1 * cam_depth_axis;
			camera_lookat_point -= 0.1 * cam_depth_axis;
		}

		if (consume_first_press(SHOW_CAMERA_POS_KEY)) {
			cout << endl;
			cout << "camera position : " << camera_pos.transpose() << endl;
			cout << "camera lookat point : " << camera_lookat_point.transpose()
				<< endl;
			cout << "camera up axis : " << camera_up_axis.transpose() << endl;
			cout << endl;
		}
	}

	//setCameraPose(camera_name, camera_pos, camera_up_axis, camera_lookat_point);
	glfwGetCursorPos(_window, &_last_cursorx, &_last_cursory);

	// update shadow maps
	_world->updateShadowMaps();

	//render(camera_name);
}

static void updateGraphicsLink(
	cRobotLink* link, std::shared_ptr<Sai2Model::Sai2Model> robot_model) {
	cVector3d local_pos;
	cMatrix3d local_rot;

	// get link name
	std::string link_name = link->m_name;

	// get chai parent name
	cGenericObject* parent = link->getParent();
	std::string parent_name = parent->m_name;

	// if parent is cRobotBase, then simply get transform relative to base from
	// model
	if (dynamic_cast<cRobotBase*>(parent) != NULL) {
		Eigen::Affine3d T = robot_model->transform(link_name);
		local_pos = cVector3d(T.translation());
		local_rot = cMatrix3d(T.rotation());
	} else if (dynamic_cast<cRobotLink*>(parent) != NULL) {
		// if parent is cRobotLink, then calculate transform for both links,
		// then apply inverse transform
		Eigen::Affine3d T_me, T_parent, T_rel;
		T_me = robot_model->transform(link_name);
		T_parent = robot_model->transform(parent_name);
		T_rel = T_parent.inverse() * T_me;
		local_pos = cVector3d(T_rel.translation());
		local_rot = cMatrix3d(T_rel.rotation());
	} else {
		cerr << "Parent to link " << link_name << " is neither link nor base"
			 << endl;
		abort();
		// TODO: throw exception
	}
	link->setLocalPos(local_pos);
	link->setLocalRot(local_rot);

	// call on children
	cRobotLink* child;
	for (unsigned int i = 0; i < link->getNumChildren(); ++i) {
		child = dynamic_cast<cRobotLink*>(link->getChild(i));
		if (child != NULL) {
			updateGraphicsLink(child, robot_model);
		}
	}
}

// update frame for a particular robot
void Sai2Graphics::updateRobotGraphics(const std::string& robot_name,
									   const Eigen::VectorXd& joint_angles) {
	auto it = _robot_models.find(robot_name);
	if (it == _robot_models.end()) {
		throw std::invalid_argument(
			"Robot not found in Sai2Graphics::updateRobotGraphics");
	}
	updateRobotGraphics(
		robot_name, joint_angles,
		Eigen::VectorXd::Zero(_robot_models.at(robot_name)->dof()));
}

void Sai2Graphics::updateRobotGraphics(
	const std::string& robot_name, const Eigen::VectorXd& joint_angles,
	const Eigen::VectorXd& joint_velocities) {
	// update corresponfing robot model
	auto it = _robot_models.find(robot_name);
	if (it == _robot_models.end()) {
		throw std::invalid_argument(
			"Robot not found in Sai2Graphics::updateRobotGraphics");
	}
	auto robot_model = _robot_models.at(robot_name);
	if (joint_angles.size() != robot_model->qSize()) {
		throw std::invalid_argument(
			"size of joint angles inconsistent with robot model in "
			"Sai2Graphics::updateRobotGraphics");
	}
	if (joint_velocities.size() != robot_model->dof()) {
		throw std::invalid_argument(
			"size of joint velocities inconsistent with robot model in "
			"Sai2Graphics::updateRobotGraphics");
	}
	robot_model->setQ(joint_angles);
	robot_model->setDq(joint_velocities);
	robot_model->updateKinematics();

	// get robot base object in chai world
	cRobotBase* base = NULL;
	for (unsigned int i = 0; i < _world->getNumChildren(); ++i) {
		if (robot_name == _world->getChild(i)->m_name) {
			// cast to cRobotBase
			base = dynamic_cast<cRobotBase*>(_world->getChild(i));
			if (base != NULL) {
				break;
			}
		}
	}
	if (base == NULL) {
		// TODO: throw exception
		cerr << "Could not find robot in chai world: " << robot_name << endl;
		abort();
	}
	// recursively update graphics for all children
	cRobotLink* link;
	for (unsigned int i = 0; i < base->getNumChildren(); ++i) {
		link = dynamic_cast<cRobotLink*>(base->getChild(i));
		if (link != NULL) {
			updateGraphicsLink(link, robot_model);
		}
	}
}

void Sai2Graphics::updateObjectGraphics(
	const std::string& object_name, const Eigen::Affine3d& object_pose,
	const Eigen::Vector6d& object_velocity) {
	if (!objectExistsInWorld(object_name)) {
		throw std::invalid_argument(
			"object not found in Sai2Graphics::updateObjectGraphics");
	}
	cGenericObject* object = NULL;
	for (unsigned int i = 0; i < _world->getNumChildren(); ++i) {
		if (object_name == _world->getChild(i)->m_name) {
			// cast to cRobotBase
			object = _world->getChild(i);
			if (object != NULL) {
				break;
			}
		}
	}
	if (object == NULL) {
		// TODO: throw exception
		cerr << "Could not find object in chai world: " << object_name << endl;
		abort();
	}

	// update pose
	*_object_poses.at(object_name) = object_pose;
	*_object_velocities.at(object_name) = object_velocity;
	object->setLocalPos(object_pose.translation());
	object->setLocalRot(object_pose.rotation());
}

void Sai2Graphics::showObjectLinkFrame(bool show_frame, const std::string& object_name, 
									   const double frame_pointer_length) {
	if (!objectExistsInWorld(object_name)) {
		throw std::invalid_argument(
			"object not found in Sai2Graphics::updateObjectGraphics");
	}
	cGenericObject* object = NULL;
	for (unsigned int i = 0; i < _world->getNumChildren(); ++i) {
		if (object_name == _world->getChild(i)->m_name) {
			// cast to cRobotBase
			object = _world->getChild(i);
			if (object != NULL) {
				break;
			}
		}
	}
	if (object == NULL) {
		// TODO: throw exception
		cerr << "Could not find object in chai world: " << object_name << endl;
		abort();
	}

	// show object frame 
	object->setFrameSize(frame_pointer_length, false);
	object->setShowFrame(show_frame, false);
}

Eigen::VectorXd Sai2Graphics::getRobotJointPos(const std::string& robot_name) {
	auto it = _robot_models.find(robot_name);
	if (it == _robot_models.end()) {
		throw std::invalid_argument(
			"robot not found in Sai2Graphics::getRobotJointPos");
	}
	return _robot_models[robot_name]->q();
}

Eigen::Affine3d Sai2Graphics::getObjectPose(const std::string& object_name) {
	if (!objectExistsInWorld(object_name)) {
		throw std::invalid_argument(
			"object not found in Sai2Graphics::getObjectPose");
	}
	return *_object_poses.at(object_name);
}

void Sai2Graphics::render(const std::string& camera_name) {
	auto camera = getCamera(camera_name);
	// TODO: support link mounted cameras
	// TODO: support stereo. see cCamera::renderView
	//	to do so, we need to search through the descendent tree
	// render view from this camera
	// NOTE: we don't use the display context id right now since chai no longer
	// supports it in 3.2.0
	camera->renderView(_window_width, _window_height);
}

void Sai2Graphics::addFrameBuffer(const std::string camera_name,
								  const int width,
								  const int height) {
	auto camera = getCamera(camera_name);
	_frame_buffer[camera_name] = chai3d::cFrameBuffer::create();
	_frame_buffer[camera_name]->setup(camera, width, height, true, true);
	// _frame_buffer[camera_name]->setSize(width, height);
}

void Sai2Graphics::saveFrameBuffer(const std::string camera_name,
								   const std::string fname) {
	_frame_buffer[camera_name]->renderView();
	chai3d::cImagePtr image = chai3d::cImage::create();
	_frame_buffer[camera_name]->copyImageBuffer(image);
	image->saveToFile(fname);
}

void Sai2Graphics::writeFrameBuffer(const std::string camera_name,
								    const std::string fname) {
	_frame_buffer[camera_name]->renderView();
	chai3d::cImagePtr image = chai3d::cImage::create();
	_frame_buffer[camera_name]->copyImageBuffer(image);
	unsigned char* data = image->getData();  // GL_RGBA
	std::FILE* fp = std::fopen((fname + ".bin").c_str(), "wb");
	std::fwrite(data, 4, _frame_buffer[camera_name]->getWidth() * _frame_buffer[camera_name]->getHeight(), fp);
	std::fclose(fp);
}

std::vector<unsigned char> Sai2Graphics::getFrameBuffer(const std::string camera_name) {
	_frame_buffer[camera_name]->renderView();
	chai3d::cImagePtr image = chai3d::cImage::create();
	_frame_buffer[camera_name]->copyImageBuffer(image);
	unsigned char* data = image->getData();  // GL_RGBA
	unsigned int size = image->getSizeInBytes();
	return std::vector<unsigned char>(data, data + size);
}

// get current camera pose
void Sai2Graphics::getCameraPose(const std::string& camera_name,
								 Eigen::Vector3d& ret_position,
								 Eigen::Vector3d& ret_vertical_axis,
								 Eigen::Vector3d& ret_lookat_point) {
	auto camera = getCamera(camera_name);
	cVector3d pos, vert, lookat;
	pos = camera->getLocalPos();
	ret_position << pos.x(), pos.y(), pos.z();
	vert = camera->getUpVector();
	ret_vertical_axis << vert.x(), vert.y(), vert.z();
	lookat = camera->getLookVector();
	ret_lookat_point << lookat.x(), lookat.y(), lookat.z();
	ret_lookat_point += ret_position;
}

// set camera pose
void Sai2Graphics::setCameraPose(const std::string& camera_name,
								 const Eigen::Vector3d& position,
								 const Eigen::Vector3d& vertical_axis,
								 const Eigen::Vector3d& lookat_point) {
	auto camera = getCamera(camera_name);
	cVector3d pos(position[0], position[1], position[2]);
	cVector3d vert(vertical_axis[0], vertical_axis[1], vertical_axis[2]);
	cVector3d look(lookat_point[0], lookat_point[1], lookat_point[2]);
	camera->set(pos, look, vert);
}

void Sai2Graphics::setCameraOnRobot(const std::string& camera_name,
									const std::string& robot_name,
									const std::string& link_name,
									const Eigen::Affine3d& rel_pose,
									const std::vector<int>& order) {
	auto camera = getCamera(camera_name);
	auto it = _robot_models.find(robot_name);
	if (it == _robot_models.end()) {
		throw std::invalid_argument(
			"Robot not found in Sai2Graphics::updateRobotGraphics");
	}
	Vector3d pos_in_world = _robot_models.at(robot_name)->positionInWorld(link_name, rel_pose.translation());
	Matrix3d rot_in_world = _robot_models.at(robot_name)->rotationInWorld(link_name, rel_pose.linear());
	cVector3d pos(pos_in_world(0), pos_in_world(1), pos_in_world(2));
	cVector3d look(rot_in_world(0, order[0]), rot_in_world(1, order[0]), rot_in_world(2, order[0]));
	cVector3d vert(rot_in_world(0, order[1]), rot_in_world(1, order[1]), rot_in_world(2, order[1]));
	camera->set(pos, look, vert);
}

void Sai2Graphics::setCameraFov(const std::string& camera_name,
								const double& fov_rad) {
	auto camera = getCamera(camera_name);
	camera->setFieldViewAngleRad(fov_rad);
}

// get camera object
cCamera* Sai2Graphics::getCamera(const std::string& camera_name) {
	cCamera* camera;
	// find camera among children
	// TODO: this can be optimized by locally storing list of cameras
	for (unsigned int i = 0; i < _world->getNumChildren(); ++i) {
		cGenericObject* child = _world->getChild(i);
		if (camera_name == child->m_name) {
			camera = dynamic_cast<cCamera*>(child);
			if (camera) {
				break;
			}
		}
	}
	if (!camera) {
		cerr << "Could not find camera named " << camera_name << endl;
		abort();
		// TODO: throw exception instead
	}
	return camera;
}

cRobotLink* Sai2Graphics::findLinkObjectInParentLinkRecursive(
	cRobotLink* parent, const std::string& link_name) {
	// call on children
	cRobotLink* child;
	cRobotLink* ret_link = NULL;
	for (unsigned int i = 0; i < parent->getNumChildren(); ++i) {
		child = dynamic_cast<cRobotLink*>(parent->getChild(i));
		if (child != NULL) {
			if (child->m_name == link_name) {
				ret_link = child;
				break;
			} else {
				ret_link =
					findLinkObjectInParentLinkRecursive(child, link_name);
			}
		}
	}
	return ret_link;
}

cRobotLink* Sai2Graphics::findLink(const std::string& robot_name,
								   const std::string& link_name) {
	// get robot base
	cRobotBase* base = NULL;
	for (unsigned int i = 0; i < _world->getNumChildren(); ++i) {
		if (robot_name == _world->getChild(i)->m_name) {
			// cast to cRobotBase
			base = dynamic_cast<cRobotBase*>(_world->getChild(i));
			if (base != NULL) {
				break;
			}
		}
	}
	if (base == NULL) {
		// TODO: throw exception
		cerr << "Could not find robot in chai world: " << robot_name << endl;
		abort();
	}

	cRobotLink* target_link = NULL;
	// get target link
	cRobotLink* base_link;
	for (unsigned int i = 0; i < base->getNumChildren(); ++i) {
		base_link = dynamic_cast<cRobotLink*>(base->getChild(i));
		if (base_link != NULL) {
			if (base_link->m_name == link_name) {
				target_link = base_link;
				break;
			} else {
				target_link =
					findLinkObjectInParentLinkRecursive(base_link, link_name);
				if (target_link != NULL) {
					break;
				}
			}
		}
	}
	return target_link;
}

void Sai2Graphics::showLinkFrameRecursive(cRobotLink* parent, bool show_frame,
										  const double frame_pointer_length) {
	// call on children
	cRobotLink* child;
	for (unsigned int i = 0; i < parent->getNumChildren(); ++i) {
		child = dynamic_cast<cRobotLink*>(parent->getChild(i));
		if (child != NULL) {
			child->setFrameSize(frame_pointer_length, false);
			child->setShowFrame(show_frame, false);
			showLinkFrameRecursive(child, show_frame, frame_pointer_length);
		}
	}
}

void Sai2Graphics::showLinkFrame(bool show_frame,
								 const std::string& robot_or_object_name,
								 const std::string& link_name,
								 const double frame_pointer_length) {
	if (link_name.empty()) {  // apply to all links
		cGenericObject* base = NULL;
		for (unsigned int i = 0; i < _world->getNumChildren(); ++i) {
			if (robot_or_object_name == _world->getChild(i)->m_name) {
				base = _world->getChild(i);
				if (base != NULL) {
					break;
				}
			}
		}
		if (base == NULL) {
			cerr << "Could not find robot in chai world: "
				 << robot_or_object_name << ". Cannot show frame." << endl;
		}
		base->setFrameSize(frame_pointer_length, false);
		base->setShowFrame(show_frame, false);
		// get target link
		cRobotLink* base_link;
		for (unsigned int i = 0; i < base->getNumChildren(); ++i) {
			base_link = dynamic_cast<cRobotLink*>(base->getChild(i));
			if (base_link != NULL) {
				base_link->setFrameSize(frame_pointer_length, false);
				base_link->setShowFrame(show_frame, false);
				showLinkFrameRecursive(base_link, show_frame,
									   frame_pointer_length);
			}
		}
	} else {
		auto target_link = findLink(robot_or_object_name, link_name);
		target_link->setFrameSize(frame_pointer_length, false);
		target_link->setShowFrame(show_frame, false);
	}
}

void Sai2Graphics::showWireMesh(bool show_wiremesh,
								const std::string& robot_or_object_name,
								const std::string& link_name) {
	if (link_name.empty()) {  // apply to all links
		cGenericObject* base = NULL;
		for (unsigned int i = 0; i < _world->getNumChildren(); ++i) {
			if (robot_or_object_name == _world->getChild(i)->m_name) {
				base = _world->getChild(i);
				if (base != NULL) {
					break;
				}
			}
		}
		if (base == NULL) {
			cerr << "Could not find robot or object in chai graphics world: "
				 << robot_or_object_name << ". Cannot show wire mesh." << endl;
		}
		base->setWireMode(show_wiremesh, true);
	} else {
		auto target_link = findLink(robot_or_object_name, link_name);
		target_link->setWireMode(show_wiremesh, true);
	}
}

void Sai2Graphics::setRenderingEnabled(const bool rendering_enabled,
									   const string robot_or_object_name,
									   const string link_name) {
	if (link_name.empty()) {  // apply to all links
		cGenericObject* base = NULL;
		for (unsigned int i = 0; i < _world->getNumChildren(); ++i) {
			if (robot_or_object_name == _world->getChild(i)->m_name) {
				base = _world->getChild(i);
				if (base != NULL) {
					break;
				}
			}
		}
		if (base == NULL) {
			cerr << "Could not find robot or object in chai graphics world: "
				 << robot_or_object_name << ". Cannot enable/disable rendering."
				 << endl;
		}
		base->setEnabled(rendering_enabled, true);
	} else {
		auto target_link = findLink(robot_or_object_name, link_name);
		target_link->setEnabled(rendering_enabled, false);
		cGenericObject* child;
		for (unsigned int i = 0; i < target_link->getNumChildren(); ++i) {
			child = target_link->getChild(i);
			// only apply to children that are visual elements (supposed to have
			// the same name), not children links (which will have different names)
			if (child->m_name == link_name) {
				child->setEnabled(rendering_enabled, false);
			}
		}
	}
}

void Sai2Graphics::showTransparency(bool show_transparency, const std::string& robot_name, const double level) {
	// get robot base
	cRobotBase* base = NULL;
	for (unsigned int i = 0; i < _world->getNumChildren(); ++i) {
		if (robot_name == _world->getChild(i)->m_name) {
			// cast to cRobotBase
			base = dynamic_cast<cRobotBase*>(_world->getChild(i));
			if (base != NULL) {
				break;
			}
		}
	}
	if (base == NULL) {
		// TODO: throw exception
		cerr << "Could not find robot in chai world: " << robot_name
				<< endl;
		abort();
	}
	base->setTransparencyLevel(level, true, true, true);
	base->setUseTransparency(true, true);
}

void Sai2Graphics::showObjectTransparency(bool show_transparency, const std::string& object_name, const double level) {
	if (!objectExistsInWorld(object_name)) {
		throw std::invalid_argument(
			"object not found in Sai2Graphics::updateObjectGraphics");
	}
	cGenericObject* object = NULL;
	for (unsigned int i = 0; i < _world->getNumChildren(); ++i) {
		if (object_name == _world->getChild(i)->m_name) {
			// cast to cRobotBase
			object = _world->getChild(i);
			if (object != NULL) {
				break;
			}
		}
	}
	if (object == NULL) {
		// TODO: throw exception
		cerr << "Could not find object in chai world: " << object_name << endl;
		abort();
	}

	// show object frame 
	object->setTransparencyLevel(level, true, true, true);
	object->setUseTransparency(true, true);
}

}  // namespace Sai2Graphics
