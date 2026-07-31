# Same as penmeasure.rb but at six different world positions. Penetration must not depend on placement.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$r0 = RectangleThin48.new; $e.toys << $r0; $r1 = RectangleThin48.new; $e.toys << $r1
$a = $r0.limbs.to_a.first; $b = $r1.limbs.to_a.first
[$a, $b].each { |l| l.gravity_override = 0.0; l.air_resistance_linear = 0.0; l.air_resistance_angular = 0.0 }
[$a, $b].each { |l| l.material_dampener = 0.0; l.material_velocity_response = 0.0; l.material_kinetic_friction = 0.0; l.material_static_friction = 0.0 }
$a.position = Vector[0.5, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[1.4099999999999999, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("P0 %+.9e", $b.momentum.x)
$a.position = Vector[1.5, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[2.4100000000000001, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("P1 %+.9e", $b.momentum.x)
$a.position = Vector[3, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[3.9100000000000001, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("P2 %+.9e", $b.momentum.x)
$a.position = Vector[6, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[6.9100000000000001, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("P3 %+.9e", $b.momentum.x)
$a.position = Vector[8, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.9100000000000001, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("P4 %+.9e", $b.momentum.x)
$a.position = Vector[12, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-400.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[12.91, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("P5 %+.9e", $b.momentum.x)
