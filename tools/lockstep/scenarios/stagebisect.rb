# Which RK4 stage does deep-contact divergence enter at?
# J = separating (contact in stage 0 only), K = approaching (all four stages).
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$r0 = RectangleThin48.new; $e.toys << $r0; $r1 = RectangleThin48.new; $e.toys << $r1
$a = $r0.limbs.to_a.first; $b = $r1.limbs.to_a.first
[$a, $b].each { |l| l.gravity_override = 0.0; l.air_resistance_linear = 0.0; l.air_resistance_angular = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-40.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.955, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("J005a %+.9e %+.9e %+.9e", $a.momentum.x, $a.position.x, $b.momentum.x)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[-40.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.91, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("J050a %+.9e %+.9e %+.9e", $a.momentum.x, $a.position.x, $b.momentum.x)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.955, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("K005a %+.9e %+.9e %+.9e", $a.momentum.x, $a.position.x, $b.momentum.x)
