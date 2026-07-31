$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$t = U6Bluebear.new; $e.toys << $t; $t.move(Vector[6.0, 4.0])
$ls = $t.limbs.to_a
$js = $t.joints.to_a
$js.each { |j| j.stiffness = 0.0 }
$js.each { |j| j.dampener = 0.0 }
$ls.each { |l| l.air_resistance_linear = 0.0 }
$ls.each { |l| l.air_resistance_angular = 0.0 }
$ls.each { |l| l.gravity_override = 0.0 }
$ls.each { |l| l.momentum = Vector[0.0, 0.0] }
$ls.each { |l| l.orientation = 0.0 }
$ls.each { |l| l.angular_momentum = 1.0 }
$e.run_steps(1)
puts format("SPIN0 %+.9e", $ls[0].orientation)
puts format("SPIN1 %+.9e", $ls[1].orientation)
puts format("SPIN2 %+.9e", $ls[2].orientation)
puts format("SPIN3 %+.9e", $ls[3].orientation)
puts format("SPIN4 %+.9e", $ls[4].orientation)
puts format("SPIN5 %+.9e", $ls[5].orientation)
$ls.each { |l| l.orientation = 0.0 }
$ls.each { |l| l.angular_momentum = 0.37 }
$e.run_steps(1)
puts format("SPINB0 %+.9e", $ls[0].orientation)
puts format("SPINB1 %+.9e", $ls[1].orientation)
puts format("SPINB2 %+.9e", $ls[2].orientation)
puts format("SPINB3 %+.9e", $ls[3].orientation)
puts format("SPINB4 %+.9e", $ls[4].orientation)
puts format("SPINB5 %+.9e", $ls[5].orientation)
