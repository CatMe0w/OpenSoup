$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$r1 = RectangleThin48.new; $e.toys << $r1; $r1.move(Vector[4.0, 3.0]); $b = $r1.limbs.to_a.first
$r2 = RectangleThin48.new; $e.toys << $r2; $r2.move(Vector[4.0, 3.4]); $u = $r2.limbs.to_a.first
$b.air_resistance_linear = 0.0
$b.air_resistance_angular = 0.0
$b.gravity_override = 0.0
$u.air_resistance_linear = 0.0
$u.air_resistance_angular = 0.0
$u.gravity_override = 0.0
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.0, 3.192]; $u.orientation = 0.0; $u.momentum = Vector[0.0, 0.0]; $u.angular_momentum = 0.0
$b.material_velocity_response = 0.0
$u.material_velocity_response = 0.0
$b.material_dampener = 0.0
$u.material_dampener = 0.0
$b.material_static_friction = 0.0
$u.material_static_friction = 0.0
$b.material_kinetic_friction = 0.0
$u.material_kinetic_friction = 0.0
$e.run_steps(1)
puts format("T1 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.0, 3.192]; $u.orientation = 0.05; $u.momentum = Vector[0.0, 0.0]; $u.angular_momentum = 0.0
$e.run_steps(1)
puts format("T2 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.0, 3.192]; $u.orientation = 0.05; $u.momentum = Vector[0.0, 0.0]; $u.angular_momentum = -0.7
$e.run_steps(1)
puts format("T3 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.0, 3.192]; $u.orientation = 0.05; $u.momentum = Vector[0.5, -1.8]; $u.angular_momentum = -0.7
$e.run_steps(1)
puts format("T4 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.45, 3.192]; $u.orientation = 0.05; $u.momentum = Vector[0.5, -1.8]; $u.angular_momentum = -0.7
$e.run_steps(1)
puts format("T5 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.45, 3.192]; $u.orientation = -0.05; $u.momentum = Vector[0.5, -1.8]; $u.angular_momentum = -0.7
$e.run_steps(1)
puts format("T6 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.0, 3.192]; $u.orientation = 0.0; $u.momentum = Vector[0.0, 0.0]; $u.angular_momentum = 0.0
$b.material_velocity_response = 105.0
$u.material_velocity_response = 105.0
$b.material_dampener = 90.0
$u.material_dampener = 90.0
$b.material_static_friction = 0.37
$u.material_static_friction = 0.37
$b.material_kinetic_friction = 1.2
$u.material_kinetic_friction = 1.2
$e.run_steps(1)
puts format("T7 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.0, 3.192]; $u.orientation = 0.05; $u.momentum = Vector[0.0, 0.0]; $u.angular_momentum = 0.0
$e.run_steps(1)
puts format("T8 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.0, 3.192]; $u.orientation = 0.05; $u.momentum = Vector[0.0, 0.0]; $u.angular_momentum = -0.7
$e.run_steps(1)
puts format("T9 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.0, 3.192]; $u.orientation = 0.05; $u.momentum = Vector[0.5, -1.8]; $u.angular_momentum = -0.7
$e.run_steps(1)
puts format("T10 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.45, 3.192]; $u.orientation = 0.05; $u.momentum = Vector[0.5, -1.8]; $u.angular_momentum = -0.7
$e.run_steps(1)
puts format("T11 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.45, 3.192]; $u.orientation = -0.05; $u.momentum = Vector[0.5, -1.8]; $u.angular_momentum = -0.7
$e.run_steps(1)
puts format("T12 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$u.position = Vector[4.0, 3.6]; $u.orientation = 0.05; $u.momentum = Vector[0.5, -1.8]; $u.angular_momentum = -0.7
$e.run_steps(1)
puts format("T13 B(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f) U(%+.6f %+.6f %+.6f | %+.6f %+.6f %+.6f)", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum, $u.position.x, $u.position.y, $u.orientation, $u.momentum.x, $u.momentum.y, $u.angular_momentum)
