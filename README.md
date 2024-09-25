# ROS2 Control Interface for Unitree Quadrupeds

This repository demonstrates how to use [ros2_control](https://control.ros.org/rolling/index.html) interface to control
unitree quadruped robots. A basic low-level controller from
the [unitree_guide](https://github.com/unitreerobotics/unitree_guide) is ported over and tested with the mujoco
simulator. The packages were initially developed by Zhenbiao (@legubiao) during his internship at Weston Robot.

## Build the packages

* Update rosdep to install dependencies

```bash
$ cd ~/ros2_ws
$ rosdep install --from-paths src --ignore-src -r -y
```

* Build the packages

```bash
$ colcon build --symlink-install
```

## Quick Start

1. Run unitree mujoco simulation
2. Launch the ros2-control
    ```bash
    $ source ~/ros2_ws/install/setup.bash
    $ ros2 launch go2_description hardware.launch.py
    ```
3. Run the keyboard control node

    ```bash
    $ source ~/ros2_ws/install/setup.bash
    $ ros2 run keyboard_input keyboard_input
    ```
4. Enjoy!