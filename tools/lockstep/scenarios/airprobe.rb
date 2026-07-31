$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[6.0, 4.0]); $b = $r.limbs.to_a.first
$b.gravity_override = 0.0
$b.air_resistance_linear = 0.0
$b.air_resistance_angular = 0.5
$b.position = Vector[6.0, 4.0]; $b.orientation = 0.0
$b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 1.0
$e.run_steps(1)
puts format("A1 (%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.gravity_override = 0.0
$b.air_resistance_linear = 0.0
$b.air_resistance_angular = 0.5
$b.position = Vector[6.0, 4.0]; $b.orientation = 0.0
$b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -3.7
$e.run_steps(1)
puts format("A2 (%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.gravity_override = 0.0
$b.air_resistance_linear = 1.3
$b.air_resistance_angular = 0.0
$b.position = Vector[6.0, 4.0]; $b.orientation = 0.0
$b.momentum = Vector[2.0, -5.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("A3 (%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.gravity_override = 0.0
$b.air_resistance_linear = 1.3
$b.air_resistance_angular = 0.5
$b.position = Vector[6.0, 4.0]; $b.orientation = 0.0
$b.momentum = Vector[2.0, -5.0]; $b.angular_momentum = 1.0
$e.run_steps(1)
puts format("A4 (%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.gravity_override = -18.0
$b.air_resistance_linear = 0.0
$b.air_resistance_angular = 0.0
$b.position = Vector[6.0, 4.0]; $b.orientation = 0.0
$b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("A5 (%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.gravity_override = -18.0
$b.air_resistance_linear = 1.3
$b.air_resistance_angular = 0.5
$b.position = Vector[6.0, 4.0]; $b.orientation = 0.0
$b.momentum = Vector[2.0, -5.0]; $b.angular_momentum = 1.0
$e.run_steps(1)
puts format("A6 (%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.gravity_override = 0.0
$b.air_resistance_linear = 0.0
$b.air_resistance_angular = 0.5
$b.position = Vector[6.0, 4.0]; $b.orientation = 0.0
$b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 1.0
$e.run_steps(50)
puts format("A7 (%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.gravity_override = -18.0
$b.air_resistance_linear = 1.3
$b.air_resistance_angular = 0.5
$b.position = Vector[6.0, 4.0]; $b.orientation = 0.4
$b.momentum = Vector[2.0, -5.0]; $b.angular_momentum = 1.0
$e.run_steps(50)
puts format("A8 (%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
