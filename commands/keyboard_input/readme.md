# Keyboard Input

This node will read the keyboard input and publish a control_input_msgs/Input message.

```bash
cd ~/ros2_ws
colcon build --packages-up-to keyboard_input
```

```bash
source ~/ros2_ws/install/setup.bash
ros2 run keyboard_input keyboard_input
```

## 1. Use Instructions
### 1.1 Control Mode
* Passive Mode: Keyboard 1
* Fixed Stand: Keyboard 2
    * Free Stand: Keyboard 3
    * Trot: Keyboard 4
    * SwingTest: Keyboard 9
    * Balance: Keyboard 0
### 1.2 Control Input
* WASD IJKL: Move robot
* Space: Reset Speed Input