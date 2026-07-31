$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$t = U6Bluebear.new; $e.toys << $t; $t.move(Vector[6.0, 4.0])
$ls = $t.limbs.to_a
$ls.each { |l| l.air_resistance_linear = 0.0 }
$ls.each { |l| l.air_resistance_angular = 0.0 }
$ls.each { |l| l.gravity_override = 0.0 }
$px = $ls.map { |l| l.position.x }
$py = $ls.map { |l| l.position.y }
$po = $ls.map { |l| l.orientation }
6.times { |i| $ls[i].position = Vector[$px[i], $py[i]] }
6.times { |i| $ls[i].orientation = $po[i] }
6.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
6.times { |i| $ls[i].angular_momentum = 0.0 }
$ls[0].position = Vector[$px[0] + 0.03, $py[0] + -0.02]
$ls[0].orientation = $po[0] + 0.1
$e.run_steps(1)
$q = $ls[0]
puts format("K01.0 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[1]
puts format("K01.1 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[2]
puts format("K01.2 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[3]
puts format("K01.3 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[4]
puts format("K01.4 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[5]
puts format("K01.5 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
6.times { |i| $ls[i].position = Vector[$px[i], $py[i]] }
6.times { |i| $ls[i].orientation = $po[i] }
6.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
6.times { |i| $ls[i].angular_momentum = 0.0 }
$ls[0].position = Vector[$px[0] + 0.03, $py[0] + -0.02]
$ls[0].orientation = $po[0] + 0.1
$e.run_steps(2)
$q = $ls[0]
puts format("K02.0 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[1]
puts format("K02.1 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[2]
puts format("K02.2 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[3]
puts format("K02.3 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[4]
puts format("K02.4 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[5]
puts format("K02.5 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
6.times { |i| $ls[i].position = Vector[$px[i], $py[i]] }
6.times { |i| $ls[i].orientation = $po[i] }
6.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
6.times { |i| $ls[i].angular_momentum = 0.0 }
$ls[0].position = Vector[$px[0] + 0.03, $py[0] + -0.02]
$ls[0].orientation = $po[0] + 0.1
$e.run_steps(3)
$q = $ls[0]
puts format("K03.0 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[1]
puts format("K03.1 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[2]
puts format("K03.2 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[3]
puts format("K03.3 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[4]
puts format("K03.4 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[5]
puts format("K03.5 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
6.times { |i| $ls[i].position = Vector[$px[i], $py[i]] }
6.times { |i| $ls[i].orientation = $po[i] }
6.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
6.times { |i| $ls[i].angular_momentum = 0.0 }
$ls[0].position = Vector[$px[0] + 0.03, $py[0] + -0.02]
$ls[0].orientation = $po[0] + 0.1
$e.run_steps(5)
$q = $ls[0]
puts format("K05.0 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[1]
puts format("K05.1 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[2]
puts format("K05.2 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[3]
puts format("K05.3 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[4]
puts format("K05.4 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[5]
puts format("K05.5 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
6.times { |i| $ls[i].position = Vector[$px[i], $py[i]] }
6.times { |i| $ls[i].orientation = $po[i] }
6.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
6.times { |i| $ls[i].angular_momentum = 0.0 }
$ls[0].position = Vector[$px[0] + 0.03, $py[0] + -0.02]
$ls[0].orientation = $po[0] + 0.1
$e.run_steps(10)
$q = $ls[0]
puts format("K10.0 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[1]
puts format("K10.1 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[2]
puts format("K10.2 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[3]
puts format("K10.3 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[4]
puts format("K10.4 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
$q = $ls[5]
puts format("K10.5 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $q.position.x, $q.position.y, $q.orientation, $q.momentum.x, $q.momentum.y, $q.angular_momentum)
