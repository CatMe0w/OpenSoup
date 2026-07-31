# Magnet edge cases: biDirectional reaction (G0-G1), inverted force (G2),
# same-toy guard (G3). Materials zeroed, joints slackened.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$q0 = SquobGreen.new; $e.toys << $q0
$q1 = SquobBlue.new; $e.toys << $q1
$g = U7SimpleGhost.new; $e.toys << $g
$d = RubberDucky.new; $e.toys << $d
$r = RBeeBee.new; $e.toys << $r
$toys = [$q0, $q1, $g, $d, $r]
$limbs = $toys.map { |t| t.limbs.to_a }.flatten
$limbs.each { |l| l.gravity_override = 0.0; l.air_resistance_linear = 0.0; l.air_resistance_angular = 0.0 }
$limbs.each { |l| l.material_velocity_response = 0.0; l.material_stiffness = 0.0; l.material_dampener = 0.0; l.material_kinetic_friction = 0.0; l.material_static_friction = 0.0 }
$toys.each { |t| t.joints.to_a.each { |j| j.stiffness = 0.0; j.dampener = 0.0 } }
$park = lambda { $limbs.each { |l| l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0; l.orientation = 0.0 } }
$show = lambda { |t, l| puts format("%s %+.9e %+.9e %+.9e", t, l.momentum.x, l.momentum.y, l.angular_momentum) }
$qa = $q0.limbs.to_a.first
$qb = $q1.limbs.to_a.first
$gl = $g.limbs.to_a.first
$dl = $d.limbs.to_a.first
$rb = $r.limbs.to_a
$gl.position = Vector[5.0, 3.0]; $dl.position = Vector[9.0, 3.0]
$rb[0].position = Vector[2.0, 8.0]; $rb[1].position = Vector[2.4, 8.0]; $rb[2].position = Vector[2.8, 8.0]
$park.call; $qa.position = Vector[8.0, 5.3]; $qb.position = Vector[8.0, 5.0]
$e.run_steps(1)
$show.call("G0a", $qa); $show.call("G0b", $qb)
$park.call; $qa.position = Vector[8.06, 5.3]; $qb.position = Vector[8.0, 5.0]; $qb.momentum = Vector[0.0, 4.0]
$e.run_steps(1)
$show.call("G1a", $qa); $show.call("G1b", $qb)
$park.call; $qa.position = Vector[1.0, 1.0]; $qb.position = Vector[14.0, 8.0]
$gl.position = Vector[5.0, 3.0]; $dl.position = Vector[5.0, 3.2]
$e.run_steps(1)
$show.call("G2d", $dl); $show.call("G2g", $gl)
$park.call; $gl.position = Vector[5.0, 3.0]; $dl.position = Vector[9.0, 3.0]
$rb[1].position = Vector[6.0, 7.0]; $rb[2].position = Vector[6.0, 7.09]
$e.run_steps(1)
$show.call("G3l", $rb[1]); $show.call("G3r", $rb[2])
