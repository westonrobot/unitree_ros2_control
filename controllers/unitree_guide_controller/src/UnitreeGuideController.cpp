//
// Created by tlab-uav on 24-9-6.
//

#include "unitree_guide_controller/UnitreeGuideController.h"
#include "unitree_guide_controller/FSM/StatePassive.h"
#include "unitree_guide_controller/robot/QuadrupedRobot.h"

namespace unitree_guide_controller {
    using config_type = controller_interface::interface_configuration_type;

    controller_interface::InterfaceConfiguration UnitreeGuideController::command_interface_configuration() const {
        controller_interface::InterfaceConfiguration conf = {config_type::INDIVIDUAL, {}};

        conf.names.reserve(joint_names_.size() * command_interface_types_.size());
        for (const auto &joint_name: joint_names_) {
            for (const auto &interface_type: command_interface_types_) {
                conf.names.push_back(joint_name + "/" += interface_type);
            }
        }

        return conf;
    }

    controller_interface::InterfaceConfiguration UnitreeGuideController::state_interface_configuration() const {
        controller_interface::InterfaceConfiguration conf = {config_type::INDIVIDUAL, {}};

        conf.names.reserve(joint_names_.size() * state_interface_types_.size());
        for (const auto &joint_name: joint_names_) {
            for (const auto &interface_type: state_interface_types_) {
                conf.names.push_back(joint_name + "/" += interface_type);
            }
        }

        for (const auto &interface_type: imu_interface_types_) {
            conf.names.push_back(imu_name_ + "/" += interface_type);
        }

        return conf;
    }

    controller_interface::return_type UnitreeGuideController::
    update(const rclcpp::Time &time, const rclcpp::Duration &period) {
        // auto now = std::chrono::steady_clock::now();
        // std::chrono::duration<double> time_diff = now - last_update_time_;
        // last_update_time_ = now;
        //
        // // Calculate the frequency
        // update_frequency_ = 1.0 / time_diff.count();
        // RCLCPP_INFO(get_node()->get_logger(), "Update frequency: %f Hz", update_frequency_);

        ctrl_comp_.robot_model_.update();
        ctrl_comp_.wave_generator_.update();
        ctrl_comp_.estimator_.update();

        if (mode_ == FSMMode::NORMAL) {
            current_state_->run();
            next_state_name_ = current_state_->checkChange();
            if (next_state_name_ != current_state_->state_name) {
                mode_ = FSMMode::CHANGE;
                next_state_ = getNextState(next_state_name_);
                RCLCPP_INFO(get_node()->get_logger(), "Switched from %s to %s",
                            current_state_->state_name_string.c_str(), next_state_->state_name_string.c_str());
            }
        } else if (mode_ == FSMMode::CHANGE) {
            current_state_->exit();
            current_state_ = next_state_;

            current_state_->enter();
            mode_ = FSMMode::NORMAL;
        }

        return controller_interface::return_type::OK;
    }

    controller_interface::CallbackReturn UnitreeGuideController::on_init() {
        try {
            joint_names_ = auto_declare<std::vector<std::string> >("joints", joint_names_);
            command_interface_types_ =
                    auto_declare<std::vector<std::string> >("command_interfaces", command_interface_types_);
            state_interface_types_ =
                    auto_declare<std::vector<std::string> >("state_interfaces", state_interface_types_);

            // imu sensor
            imu_name_ = auto_declare<std::string>("imu_name", imu_name_);
            imu_interface_types_ = auto_declare<std::vector<std::string> >("imu_interfaces", state_interface_types_);
        } catch (const std::exception &e) {
            fprintf(stderr, "Exception thrown during init stage with message: %s \n", e.what());
            return controller_interface::CallbackReturn::ERROR;
        }

        return CallbackReturn::SUCCESS;
    }

    controller_interface::CallbackReturn UnitreeGuideController::on_configure(
        const rclcpp_lifecycle::State &previous_state) {
        control_input_subscription_ = get_node()->create_subscription<control_input_msgs::msg::Inputs>(
            "/control_input", 10, [this](const control_input_msgs::msg::Inputs::SharedPtr msg) {
                // Handle message
                ctrl_comp_.control_inputs_.get().command = msg->command;
                ctrl_comp_.control_inputs_.get().lx = msg->lx;
                ctrl_comp_.control_inputs_.get().ly = msg->ly;
                ctrl_comp_.control_inputs_.get().rx = msg->rx;
                ctrl_comp_.control_inputs_.get().ry = msg->ry;
            });

        robot_description_subscription_ = get_node()->create_subscription<std_msgs::msg::String>(
            "~/robot_description", rclcpp::QoS(rclcpp::KeepLast(1)).transient_local(),
            [this](const std_msgs::msg::String::SharedPtr msg) {
                ctrl_comp_.robot_model_.init(msg->data);
                ctrl_comp_.balance_ctrl_.init(ctrl_comp_.robot_model_);
            });

        get_node()->get_parameter("update_rate", ctrl_comp_.frequency_);
        RCLCPP_INFO(get_node()->get_logger(), "Controller Manager Update Rate: %d Hz", ctrl_comp_.frequency_);

        ctrl_comp_.wave_generator_.init(0.45, 0.5, Vec4(0, 0.5, 0.5, 0));

        return CallbackReturn::SUCCESS;
    }

    controller_interface::CallbackReturn
    UnitreeGuideController::on_activate(const rclcpp_lifecycle::State &previous_state) {
        // clear out vectors in case of restart
        ctrl_comp_.clear();

        // assign command interfaces
        for (auto &interface: command_interfaces_) {
            command_interface_map_[interface.get_interface_name()]->push_back(interface);
        }

        // assign state interfaces
        for (auto &interface: state_interfaces_) {
            if (interface.get_prefix_name() == imu_name_) {
                ctrl_comp_.imu_state_interface_.emplace_back(interface);
            } else {
                state_interface_map_[interface.get_interface_name()]->push_back(interface);
            }
        }

        state_list_.passive = std::make_shared<StatePassive>(ctrl_comp_);
        state_list_.fixedDown = std::make_shared<StateFixedDown>(ctrl_comp_);
        state_list_.fixedStand = std::make_shared<StateFixedStand>(ctrl_comp_);
        state_list_.swingTest = std::make_shared<StateSwingTest>(ctrl_comp_);
        state_list_.freeStand = std::make_shared<StateFreeStand>(ctrl_comp_);
        state_list_.balanceTest = std::make_shared<StateBalanceTest>(ctrl_comp_);
        state_list_.trotting = std::make_shared<StateTrotting>(ctrl_comp_);

        // Initialize FSM
        current_state_ = state_list_.passive;
        current_state_->enter();
        next_state_ = current_state_;
        next_state_name_ = current_state_->state_name;
        mode_ = FSMMode::NORMAL;

        return CallbackReturn::SUCCESS;
    }

    controller_interface::CallbackReturn UnitreeGuideController::on_deactivate(
        const rclcpp_lifecycle::State &previous_state) {
        release_interfaces();
        return CallbackReturn::SUCCESS;
    }

    controller_interface::CallbackReturn
    UnitreeGuideController::on_cleanup(const rclcpp_lifecycle::State &previous_state) {
        return CallbackReturn::SUCCESS;
    }

    controller_interface::CallbackReturn
    UnitreeGuideController::on_error(const rclcpp_lifecycle::State &previous_state) {
        return CallbackReturn::SUCCESS;
    }

    controller_interface::CallbackReturn
    UnitreeGuideController::on_shutdown(const rclcpp_lifecycle::State &previous_state) {
        return CallbackReturn::SUCCESS;
    }

    std::shared_ptr<FSMState> UnitreeGuideController::getNextState(FSMStateName stateName) const {
        switch (stateName) {
            case FSMStateName::INVALID:
                return state_list_.invalid;
            case FSMStateName::PASSIVE:
                return state_list_.passive;
            case FSMStateName::FIXEDDOWN:
                return state_list_.fixedDown;
            case FSMStateName::FIXEDSTAND:
                return state_list_.fixedStand;
            case FSMStateName::FREESTAND:
                return state_list_.freeStand;
            case FSMStateName::TROTTING:
                return state_list_.trotting;
            case FSMStateName::SWINGTEST:
                return state_list_.swingTest;
            case FSMStateName::BALANCETEST:
                return state_list_.balanceTest;
            default:
                return state_list_.invalid;
        }
    }
}

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(unitree_guide_controller::UnitreeGuideController, controller_interface::ControllerInterface);
