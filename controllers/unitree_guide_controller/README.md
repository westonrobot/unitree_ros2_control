# Unitree Guide Controller

This is a ros2-control controller based on unitree guide. The original unitree guide project could be found [here](https://github.com/unitreerobotics/unitree_guide)

```bash
cd ~/ros2_ws
colcon build --packages-up-to unitree_guide_controller
```

```bash
source ~/ros2_ws/install/setup.bash
ros2 launch unitree_guide_controller controller.launch.py
```