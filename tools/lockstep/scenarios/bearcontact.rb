$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$t1 = U6Bluebear.new; $e.toys << $t1; $t1.move(Vector[6.0, 4.0])
$t2 = U6Bluebear.new; $e.toys << $t2; $t2.move(Vector[6.1, 4.0])
$l1 = $t1.limbs.to_a
$l2 = $t2.limbs.to_a
$all = $l1 + $l2
$all.each { |l| l.air_resistance_linear = 0.0 }
$all.each { |l| l.air_resistance_angular = 0.0 }
$all.each { |l| l.gravity_override = 0.0 }
($t1.joints.to_a + $t2.joints.to_a).each { |j| j.stiffness = 0.0 }
($t1.joints.to_a + $t2.joints.to_a).each { |j| j.dampener = 0.0 }
$px = $all.map { |l| l.position.x }
$py = $all.map { |l| l.position.y }
$po = $all.map { |l| l.orientation }
12.times { |i| $all[i].position = Vector[$px[i], $py[i]] }
12.times { |i| $all[i].orientation = $po[i] }
12.times { |i| $all[i].momentum = Vector[0.0, 0.0] }
12.times { |i| $all[i].angular_momentum = 0.0 }
6.times { |i| $all[6+i].position = Vector[$px[6+i] + -0.06, $py[6+i] + 0.0] }
6.times { |i| $all[6+i].momentum = Vector[0.0, 0.0] }
6.times { |i| $all[6+i].angular_momentum = 0.0 }
$e.run_steps(1)
$q = $all[0]
puts format("C1.00 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[1]
puts format("C1.01 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[2]
puts format("C1.02 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[3]
puts format("C1.03 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[4]
puts format("C1.04 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[5]
puts format("C1.05 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[6]
puts format("C1.06 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[7]
puts format("C1.07 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[8]
puts format("C1.08 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[9]
puts format("C1.09 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[10]
puts format("C1.10 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[11]
puts format("C1.11 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
12.times { |i| $all[i].position = Vector[$px[i], $py[i]] }
12.times { |i| $all[i].orientation = $po[i] }
12.times { |i| $all[i].momentum = Vector[0.0, 0.0] }
12.times { |i| $all[i].angular_momentum = 0.0 }
6.times { |i| $all[6+i].position = Vector[$px[6+i] + -0.06, $py[6+i] + 0.0] }
6.times { |i| $all[6+i].momentum = Vector[-2.5, 0.0] }
6.times { |i| $all[6+i].angular_momentum = 0.0 }
$e.run_steps(1)
$q = $all[0]
puts format("C2.00 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[1]
puts format("C2.01 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[2]
puts format("C2.02 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[3]
puts format("C2.03 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[4]
puts format("C2.04 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[5]
puts format("C2.05 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[6]
puts format("C2.06 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[7]
puts format("C2.07 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[8]
puts format("C2.08 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[9]
puts format("C2.09 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[10]
puts format("C2.10 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[11]
puts format("C2.11 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
12.times { |i| $all[i].position = Vector[$px[i], $py[i]] }
12.times { |i| $all[i].orientation = $po[i] }
12.times { |i| $all[i].momentum = Vector[0.0, 0.0] }
12.times { |i| $all[i].angular_momentum = 0.0 }
6.times { |i| $all[6+i].position = Vector[$px[6+i] + -0.06, $py[6+i] + 0.02] }
6.times { |i| $all[6+i].momentum = Vector[-2.5, 1.3] }
6.times { |i| $all[6+i].angular_momentum = 0.8 }
$e.run_steps(1)
$q = $all[0]
puts format("C3.00 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[1]
puts format("C3.01 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[2]
puts format("C3.02 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[3]
puts format("C3.03 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[4]
puts format("C3.04 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[5]
puts format("C3.05 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[6]
puts format("C3.06 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[7]
puts format("C3.07 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[8]
puts format("C3.08 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[9]
puts format("C3.09 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[10]
puts format("C3.10 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[11]
puts format("C3.11 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
12.times { |i| $all[i].position = Vector[$px[i], $py[i]] }
12.times { |i| $all[i].orientation = $po[i] }
12.times { |i| $all[i].momentum = Vector[0.0, 0.0] }
12.times { |i| $all[i].angular_momentum = 0.0 }
6.times { |i| $all[6+i].position = Vector[$px[6+i] + -0.06, $py[6+i] + 0.02] }
6.times { |i| $all[6+i].momentum = Vector[-2.5, 1.3] }
6.times { |i| $all[6+i].angular_momentum = 0.8 }
$e.run_steps(5)
$q = $all[0]
puts format("C4.00 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[1]
puts format("C4.01 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[2]
puts format("C4.02 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[3]
puts format("C4.03 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[4]
puts format("C4.04 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[5]
puts format("C4.05 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[6]
puts format("C4.06 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[7]
puts format("C4.07 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[8]
puts format("C4.08 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[9]
puts format("C4.09 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[10]
puts format("C4.10 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $all[11]
puts format("C4.11 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
