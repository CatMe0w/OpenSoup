# Measure the penetration via pure spring (only stiffness nonzero, stage 0 only).
# Penetration divides back out of the momentum.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$r0 = RectangleThin48.new; $e.toys << $r0; $r1 = RectangleThin48.new; $e.toys << $r1
$a = $r0.limbs.to_a.first; $b = $r1.limbs.to_a.first
[$a, $b].each { |l| l.gravity_override = 0.0; l.air_resistance_linear = 0.0; l.air_resistance_angular = 0.0 }
[$a, $b].each { |l| l.material_dampener = 0.0; l.material_velocity_response = 0.0; l.material_kinetic_friction = 0.0; l.material_static_friction = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.9550000000000001, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("M0 %+.9e", $b.momentum.x)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.9400000000000013, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("M1 %+.9e", $b.momentum.x)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.9100000000000001, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("M2 %+.9e", $b.momentum.x)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.8800000000000008, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("M3 %+.9e", $b.momentum.x)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.8500000000000014, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("M4 %+.9e", $b.momentum.x)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.8200000000000003, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("M5 %+.9e", $b.momentum.x)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.7600000000000016, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("M6 %+.9e", $b.momentum.x)
