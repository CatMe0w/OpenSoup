# Deep overlap with single-limb toys (no joints, no parked limbs).
# Same sweep as depthprobe.rb but minimal scene.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$r0 = RectangleThin48.new; $e.toys << $r0; $r1 = RectangleThin48.new; $e.toys << $r1
$a = $r0.limbs.to_a.first; $b = $r1.limbs.to_a.first
[$a, $b].each { |l| l.gravity_override = 0.0; l.air_resistance_linear = 0.0; l.air_resistance_angular = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.9550000000000001, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("H005a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("H005b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.9400000000000013, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("H020a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("H020b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.9100000000000001, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("H050a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("H050b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.8800000000000008, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("H080a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("H080b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.8500000000000014, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("H110a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("H110b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.8200000000000003, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("H140a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("H140b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.05
$b.position = Vector[8.9550000000000001, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.03
$e.run_steps(1)
puts format("I005a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("I005b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.05
$b.position = Vector[8.9400000000000013, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.03
$e.run_steps(1)
puts format("I020a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("I020b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.05
$b.position = Vector[8.9100000000000001, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.03
$e.run_steps(1)
puts format("I050a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("I050b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.05
$b.position = Vector[8.8800000000000008, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.03
$e.run_steps(1)
puts format("I080a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("I080b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.05
$b.position = Vector[8.8500000000000014, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.03
$e.run_steps(1)
puts format("I110a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("I110b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.05
$b.position = Vector[8.8200000000000003, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.03
$e.run_steps(1)
puts format("I140a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("I140b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
