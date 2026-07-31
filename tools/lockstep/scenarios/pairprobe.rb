# One overlapping limb pair per case, isolated (no joints/gravity/air).
# A-E cover circle-circle, circle-polygon, and polygon-polygon pairs.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$t0 = TBCabin.new; $e.toys << $t0; $t1 = U6Bluebear.new; $e.toys << $t1
$all = $t0.limbs.to_a + $t1.limbs.to_a
$js = $t0.joints.to_a + $t1.joints.to_a
$js.each { |j| j.stiffness = 0.0; j.dampener = 0.0 }
$all.each { |l| l.gravity_override = 0.0; l.air_resistance_linear = 0.0; l.air_resistance_angular = 0.0 }
# case A: circle vs circle   (bear Left Arm vs bear Right Arm)
$all.each_with_index { |l, k| l.position = Vector[1.0 + k * 1.3, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$all[6].position = Vector[+8.507521629e+00, +4.547595978e-02 + 4.0]; $all[6].orientation = -1.962741089e+01; $all[6].momentum = Vector[+4.224954128e+00, +2.941877246e-01]; $all[6].angular_momentum = -1.486007962e-02
$all[9].position = Vector[+8.537106514e+00, +1.378965974e-01 + 4.0]; $all[9].orientation = -6.564006805e+00; $all[9].momentum = Vector[+4.700734615e+00, +3.605680764e-01]; $all[9].angular_momentum = -1.468823384e-02
$e.run_steps(1)
puts format("A06 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $all[6].position.x, $all[6].position.y, $all[6].orientation, $all[6].momentum.x, $all[6].momentum.y, $all[6].angular_momentum)
puts format("A09 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $all[9].position.x, $all[9].position.y, $all[9].orientation, $all[9].momentum.x, $all[9].momentum.y, $all[9].angular_momentum)
# case B: circle vs polygon  (cabin Left Arm vs bear Body)
$all.each_with_index { |l, k| l.position = Vector[1.0 + k * 1.3, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$all[0].position = Vector[+8.586190224e+00, +3.673676550e-01 + 4.0]; $all[0].orientation = -2.048368835e+01; $all[0].momentum = Vector[+4.879552364e+00, +2.233204693e-01]; $all[0].angular_momentum = -3.407052951e-03
$all[7].position = Vector[+8.447123528e+00, +1.768566370e-01 + 4.0]; $all[7].orientation = -2.011102676e+01; $all[7].momentum = Vector[+6.805247307e+00, +1.129974723e+00]; $all[7].angular_momentum = -7.866945863e-02
$e.run_steps(1)
puts format("B00 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $all[0].position.x, $all[0].position.y, $all[0].orientation, $all[0].momentum.x, $all[0].momentum.y, $all[0].angular_momentum)
puts format("B07 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $all[7].position.x, $all[7].position.y, $all[7].orientation, $all[7].momentum.x, $all[7].momentum.y, $all[7].angular_momentum)
# case C: polygon vs polygon (bear Body vs cabin Body, depth 0.10)
$all.each_with_index { |l, k| l.position = Vector[1.0 + k * 1.3, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$all[7].position = Vector[+8.447123528e+00, +1.768566370e-01 + 4.0]; $all[7].orientation = -2.011102676e+01; $all[7].momentum = Vector[+6.805247307e+00, +1.129974723e+00]; $all[7].angular_momentum = -7.866945863e-02
$all[1].position = Vector[+8.712181091e+00, +1.845053285e-01 + 4.0]; $all[1].orientation = -2.494241524e+01; $all[1].momentum = Vector[+6.925653934e+00, +3.472106457e-01]; $all[1].angular_momentum = -4.717595503e-02
$e.run_steps(1)
puts format("C07 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $all[7].position.x, $all[7].position.y, $all[7].orientation, $all[7].momentum.x, $all[7].momentum.y, $all[7].angular_momentum)
puts format("C01 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $all[1].position.x, $all[1].position.y, $all[1].orientation, $all[1].momentum.x, $all[1].momentum.y, $all[1].angular_momentum)
# case D: polygon vs polygon (bear Left Leg vs bear Right Leg)
$all.each_with_index { |l, k| l.position = Vector[1.0 + k * 1.3, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$all[10].position = Vector[+8.306422234e+00, +1.476010233e-01 + 4.0]; $all[10].orientation = -2.075485611e+01; $all[10].momentum = Vector[+4.159520149e+00, +1.501370668e-01]; $all[10].angular_momentum = +4.234922677e-02
$all[11].position = Vector[+8.234767914e+00, +2.450243086e-01 + 4.0]; $all[11].orientation = -7.758211613e+00; $all[11].momentum = Vector[+4.690376759e+00, +1.317611217e+00]; $all[11].angular_momentum = -3.718020022e-02
$e.run_steps(1)
puts format("D10 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $all[10].position.x, $all[10].position.y, $all[10].orientation, $all[10].momentum.x, $all[10].momentum.y, $all[10].angular_momentum)
puts format("D11 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $all[11].position.x, $all[11].position.y, $all[11].orientation, $all[11].momentum.x, $all[11].momentum.y, $all[11].angular_momentum)
# case E: polygon vs polygon (bear Head vs cabin Body, depth 0.081)
$all.each_with_index { |l, k| l.position = Vector[1.0 + k * 1.3, 7.5]; l.orientation = 0.0; l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0 }
$all[8].position = Vector[+8.637675285e+00, +2.346964926e-01 + 4.0]; $all[8].orientation = -7.546275616e+00; $all[8].momentum = Vector[+4.717682362e+00, +2.936705351e-01]; $all[8].angular_momentum = -3.210085630e-02
$all[1].position = Vector[+8.712181091e+00, +1.845053285e-01 + 4.0]; $all[1].orientation = -2.494241524e+01; $all[1].momentum = Vector[+6.925653934e+00, +3.472106457e-01]; $all[1].angular_momentum = -4.717595503e-02
$e.run_steps(1)
puts format("E08 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $all[8].position.x, $all[8].position.y, $all[8].orientation, $all[8].momentum.x, $all[8].momentum.y, $all[8].angular_momentum)
puts format("E01 %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $all[1].position.x, $all[1].position.y, $all[1].orientation, $all[1].momentum.x, $all[1].momentum.y, $all[1].angular_momentum)
