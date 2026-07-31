# Head-on block contact, one material term at a time.
# Complements response.rb which only tests vertical stacking at rest.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$r0 = RectangleThin48.new; $e.toys << $r0; $r1 = RectangleThin48.new; $e.toys << $r1
$a = $r0.limbs.to_a.first; $b = $r1.limbs.to_a.first
[$a, $b].each { |l| l.gravity_override = 0.0; l.air_resistance_linear = 0.0; l.air_resistance_angular = 0.0 }
$seed = lambda { $a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.angular_momentum = 0.0; $b.position = Vector[8.955, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0 }
$seed.call; $a.momentum = Vector[-40.0, 0.0]
$e.run_steps(1)
puts format("L1 %+.9e %+.9e", $b.momentum.x, $a.momentum.x)
$seed.call; $a.momentum = Vector[-400.0, 0.0]
$e.run_steps(1)
puts format("L2 %+.9e %+.9e", $b.momentum.x, $a.momentum.x)
[$a, $b].each { |l| l.material_dampener = 0.0 }
$seed.call; $a.momentum = Vector[-40.0, 0.0]
$e.run_steps(1)
puts format("L3 %+.9e %+.9e", $b.momentum.x, $a.momentum.x)
[$a, $b].each { |l| l.material_dampener = 90.0; l.material_stiffness = 0.0 }
$seed.call; $a.momentum = Vector[-40.0, 0.0]
$e.run_steps(1)
puts format("L4 %+.9e %+.9e", $b.momentum.x, $a.momentum.x)
[$a, $b].each { |l| l.material_stiffness = 2500.0; l.material_dampener = 0.0; l.material_velocity_response = 0.0; l.material_kinetic_friction = 0.0; l.material_static_friction = 0.0 }
$seed.call; $a.momentum = Vector[-40.0, 0.0]
$e.run_steps(1)
puts format("L5 %+.9e %+.9e", $b.momentum.x, $a.momentum.x)
$seed.call; $a.momentum = Vector[5.0, 0.0]
$e.run_steps(1)
puts format("L6 %+.9e %+.9e", $b.momentum.x, $a.momentum.x)
