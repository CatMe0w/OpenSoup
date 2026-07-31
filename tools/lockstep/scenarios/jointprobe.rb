$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$t = U6Bluebear.new; $e.toys << $t; $t.move(Vector[6.0, 4.0])
$ls = $t.limbs.to_a
$js = $t.joints.to_a
$ls.each { |l| l.air_resistance_linear = 0.0 }
$ls.each { |l| l.air_resistance_angular = 0.0 }
$ls.each { |l| l.gravity_override = 0.0 }
$ls.each { |l| l.momentum = Vector[0.0, 0.0] }
$ls.each { |l| l.angular_momentum = 0.0 }
$js.each { |j| j.stiffness = 0.0 }
$js.each { |j| j.dampener = 0.0 }
puts format("SETUP limbs=%d joints=%d", $ls.length, $js.length)
$j = $js[0]
$a = $j.limb1
$b = $j.limb2
puts format("JOINT a=(%+.6f %+.6f) b=(%+.6f %+.6f)", $a.position.x, $a.position.y, $b.position.x, $b.position.y)
$ax = $a.position.x
$ay = $a.position.y
$bx = $b.position.x
$by = $b.position.y
$j.stiffness = 6000.0
$j.dampener = 0.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.0; $a.momentum = Vector[0, 0]; $a.angular_momentum = 0
$b.position = Vector[$bx + 0.05, $by + 0.0]; $b.orientation = 0.0; $b.momentum = Vector[0, 0]; $b.angular_momentum = 0
$e.run_steps(1)
puts format("J1 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 0.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.0; $a.momentum = Vector[0, 0]; $a.angular_momentum = 0
$b.position = Vector[$bx + 0.03, $by + -0.04]; $b.orientation = 0.0; $b.momentum = Vector[0, 0]; $b.angular_momentum = 0
$e.run_steps(1)
puts format("J2 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 0.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.0; $a.momentum = Vector[0, 0]; $a.angular_momentum = 0
$b.position = Vector[$bx + 0.0, $by + 0.0]; $b.orientation = 0.3; $b.momentum = Vector[0, 0]; $b.angular_momentum = 0
$e.run_steps(1)
puts format("J3 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 0.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = -0.2; $a.momentum = Vector[0, 0]; $a.angular_momentum = 0
$b.position = Vector[$bx + 0.02, $by + 0.01]; $b.orientation = 0.3; $b.momentum = Vector[0, 0]; $b.angular_momentum = 0
$e.run_steps(1)
puts format("J4 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 0.0
$j.dampener = 10.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.0; $a.momentum = Vector[0.0, 0.0]; $a.angular_momentum = 0.0
$b.position = Vector[$bx + 0.0, $by + 0.0]; $b.orientation = 0.0; $b.momentum = Vector[1.5, -0.8]; $b.angular_momentum = 0.0
$e.run_steps(1)
puts format("J5 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 0.0
$j.dampener = 10.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.0; $a.momentum = Vector[0.0, 0.0]; $a.angular_momentum = 0.6
$b.position = Vector[$bx + 0.0, $by + 0.0]; $b.orientation = 0.0; $b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = -0.4
$e.run_steps(1)
puts format("J6 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 0.0
$j.dampener = 10.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.1; $a.momentum = Vector[0.7, -0.3]; $a.angular_momentum = 0.5
$b.position = Vector[$bx + 0.02, $by + 0.01]; $b.orientation = -0.1; $b.momentum = Vector[-1.1, 0.9]; $b.angular_momentum = -0.6
$e.run_steps(1)
puts format("J7 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 10.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.1; $a.momentum = Vector[0.7, -0.3]; $a.angular_momentum = 0.5
$b.position = Vector[$bx + 0.02, $by + 0.01]; $b.orientation = -0.1; $b.momentum = Vector[-1.1, 0.9]; $b.angular_momentum = -0.6
$e.run_steps(1)
puts format("J8 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 0.0
$j.rest_length = 0.05
$a.position = Vector[$ax, $ay]; $a.orientation = 0.0; $a.momentum = Vector[0, 0]; $a.angular_momentum = 0
$b.position = Vector[$bx + 0.08, $by + 0.0]; $b.orientation = 0.0; $b.momentum = Vector[0, 0]; $b.angular_momentum = 0
$e.run_steps(1)
puts format("J9 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 10.0
$j.rest_length = 0.05
$a.position = Vector[$ax, $ay]; $a.orientation = 0.1; $a.momentum = Vector[0.7, -0.3]; $a.angular_momentum = 0.5
$b.position = Vector[$bx + 0.08, $by + 0.03]; $b.orientation = -0.1; $b.momentum = Vector[-1.1, 0.9]; $b.angular_momentum = -0.6
$e.run_steps(1)
puts format("J10 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 10.0
$j.rest_length = 0.05
$a.position = Vector[$ax, $ay]; $a.orientation = 0.0; $a.momentum = Vector[0, 0]; $a.angular_momentum = 0
$b.position = Vector[$bx + 0.02, $by + 0.01]; $b.orientation = 0.0; $b.momentum = Vector[0, 0]; $b.angular_momentum = 0
$e.run_steps(1)
puts format("J11 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 10.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.1; $a.momentum = Vector[0.7, -0.3]; $a.angular_momentum = 0.5
$b.position = Vector[$bx + 0.02, $by + 0.01]; $b.orientation = -0.1; $b.momentum = Vector[-1.1, 0.9]; $b.angular_momentum = -0.6
$j.move1 = false
$e.run_steps(1)
puts format("J12 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 10.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.1; $a.momentum = Vector[0.7, -0.3]; $a.angular_momentum = 0.5
$b.position = Vector[$bx + 0.02, $by + 0.01]; $b.orientation = -0.1; $b.momentum = Vector[-1.1, 0.9]; $b.angular_momentum = -0.6
$j.move1 = true
$j.rotate1 = false
$e.run_steps(1)
puts format("J13 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 10.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.1; $a.momentum = Vector[0.7, -0.3]; $a.angular_momentum = 0.5
$b.position = Vector[$bx + 0.02, $by + 0.01]; $b.orientation = -0.1; $b.momentum = Vector[-1.1, 0.9]; $b.angular_momentum = -0.6
$j.rotate1 = true
$j.move2 = false
$e.run_steps(1)
puts format("J14 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 10.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.1; $a.momentum = Vector[0.7, -0.3]; $a.angular_momentum = 0.5
$b.position = Vector[$bx + 0.02, $by + 0.01]; $b.orientation = -0.1; $b.momentum = Vector[-1.1, 0.9]; $b.angular_momentum = -0.6
$j.move2 = true
$j.rotate2 = false
$e.run_steps(1)
puts format("J15 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 10.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.1; $a.momentum = Vector[0.7, -0.3]; $a.angular_momentum = 0.5
$b.position = Vector[$bx + 0.02, $by + 0.01]; $b.orientation = -0.1; $b.momentum = Vector[-1.1, 0.9]; $b.angular_momentum = -0.6
$j.rotate2 = true
$j.axis = Vector[1.0, 0.0]
$j.axis_on = true
$e.run_steps(1)
puts format("J16 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 10.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.1; $a.momentum = Vector[0.7, -0.3]; $a.angular_momentum = 0.5
$b.position = Vector[$bx + 0.02, $by + 0.01]; $b.orientation = -0.1; $b.momentum = Vector[-1.1, 0.9]; $b.angular_momentum = -0.6
$j.axis = Vector[0.6, 0.8]
$e.run_steps(1)
puts format("J17 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$j.stiffness = 6000.0
$j.dampener = 10.0
$j.rest_length = 0.0
$a.position = Vector[$ax, $ay]; $a.orientation = 0.1; $a.momentum = Vector[0.7, -0.3]; $a.angular_momentum = 0.5
$b.position = Vector[$bx + 0.02, $by + 0.01]; $b.orientation = -0.1; $b.momentum = Vector[-1.1, 0.9]; $b.angular_momentum = -0.6
$j.axis_on = false
$e.run_steps(1)
puts format("J18 A(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e) B(%+.9e %+.9e %+.9e %+.9e %+.9e %+.9e)", $a.position.x, $a.position.y, $a.orientation, $a.momentum.x, $a.momentum.y, $a.angular_momentum, $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
