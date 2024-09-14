import os

import xacro
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

package_description = "go2_description"


def process_xacro(context):
    robot_type_value = context.launch_configurations['robot_type']
    pkg_path = os.path.join(get_package_share_directory(package_description))
    xacro_file = os.path.join(pkg_path, 'xacro', 'robot.xacro')
    robot_description_config = xacro.process_file(xacro_file, mappings={'robot_type': robot_type_value})
    return (robot_description_config.toxml(), robot_type_value)


def launch_setup(context, *args, **kwargs):
    (robot_description, robot_type) = process_xacro(context)
    robot_controllers = PathJoinSubstitution(
        [
            FindPackageShare(package_description),
            "config",
            "robot_control.yaml",
        ]
    )
    return [
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            parameters=[
                {
                    'publish_frequency': 20.0,
                    'use_tf_static': True,
                    'robot_description': robot_description,
                    'ignore_timestamp': True
                }
            ],
        ),
        Node(
            package="controller_manager",
            executable="ros2_control_node",
            parameters=[robot_controllers],
            remappings=[
                ("~/robot_description", "/robot_description"),
            ],
            output="both",
        ),
        Node(
            package="controller_manager",
            executable="spawner",
            arguments=["joint_state_broadcaster",
                       "--controller-manager", "/controller_manager"],
        ),
        Node(
            package="controller_manager",
            executable="spawner",
            arguments=["imu_sensor_broadcaster",
                       "--controller-manager", "/controller_manager"],
        ),
    ]


def generate_launch_description():
    robot_type_arg = DeclareLaunchArgument(
        'robot_type',
        default_value='go2',
        description='Type of the robot'
    )

    rviz_config_file = os.path.join(get_package_share_directory(package_description), "config", "visualize_urdf.rviz")

    return LaunchDescription([
        robot_type_arg,
        OpaqueFunction(function=launch_setup),
        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz_ocs2',
            output='screen',
            arguments=["-d", rviz_config_file]
        )
    ])
