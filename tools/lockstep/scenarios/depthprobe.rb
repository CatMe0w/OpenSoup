# Deep polygon-vs-polygon contact depth sweep. F = no spin, G = spinning.
# Park unused limbs well inside the world - near a wall they pick up contacts.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$t0 = TBCabin.new; $e.toys << $t0; $t1 = U6Bluebear.new; $e.toys << $t1
$all = $t0.limbs.to_a + $t1.limbs.to_a
$js = $t0.joints.to_a + $t1.joints.to_a
$js.each { |j| j.stiffness = 0.0; j.dampener = 0.0 }
$all.each { |l| l.gravity_override = 0.0; l.air_resistance_linear = 0.0; l.air_resistance_angular = 0.0 }
$a = $all[1]; $b = $all[7]
$all.each_with_index { |l, k| l.position = Vector[0.8 + k * 1.15, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.2236821751999987, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("F005a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("F005b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$all.each_with_index { |l, k| l.position = Vector[0.8 + k * 1.15, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.2086821751999999, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("F020a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("F020b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$all.each_with_index { |l, k| l.position = Vector[0.8 + k * 1.15, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.1786821751999987, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("F050a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("F050b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$all.each_with_index { |l, k| l.position = Vector[0.8 + k * 1.15, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.1486821751999994, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("F080a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("F080b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$all.each_with_index { |l, k| l.position = Vector[0.8 + k * 1.15, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.1186821752, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("F110a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("F110b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$all.each_with_index { |l, k| l.position = Vector[0.8 + k * 1.15, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[8.0886821751999989, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("F140a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("F140b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$all.each_with_index { |l, k| l.position = Vector[0.8 + k * 1.15, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.05
$b.position = Vector[8.2236821751999987, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.03
$e.run_steps(1)
puts format("G005a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("G005b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$all.each_with_index { |l, k| l.position = Vector[0.8 + k * 1.15, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.05
$b.position = Vector[8.2086821751999999, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.03
$e.run_steps(1)
puts format("G020a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("G020b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$all.each_with_index { |l, k| l.position = Vector[0.8 + k * 1.15, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.05
$b.position = Vector[8.1786821751999987, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.03
$e.run_steps(1)
puts format("G050a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("G050b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$all.each_with_index { |l, k| l.position = Vector[0.8 + k * 1.15, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.05
$b.position = Vector[8.1486821751999994, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.03
$e.run_steps(1)
puts format("G080a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("G080b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$all.each_with_index { |l, k| l.position = Vector[0.8 + k * 1.15, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.05
$b.position = Vector[8.1186821752, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.03
$e.run_steps(1)
puts format("G110a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("G110b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
$all.each_with_index { |l, k| l.position = Vector[0.8 + k * 1.15, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0; $a.momentum = Vector[5.0, 0.0]; $a.angular_momentum = 0.05
$b.position = Vector[8.0886821751999989, 4.5]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.03
$e.run_steps(1)
puts format("G140a %+.9e %+.9e %+.9e %+.9e", $a.momentum.x, $a.momentum.y, $a.angular_momentum, $a.orientation)
puts format("G140b %+.9e %+.9e %+.9e %+.9e", $b.momentum.x, $b.momentum.y, $b.angular_momentum, $b.orientation)
