# centreOfResistance: off-origin drag point adds torque (keeps balloons upright).
# C0-C3 balloon (offset y), C4-C5 pirate ship (offset x),
# C6 fixedMove cog (anchored body still gets the angular term).
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$b = BalloonRed.new; $e.toys << $b
$p = PirateShip.new; $e.toys << $p
$m = MotorClockwise.new; $e.toys << $m
$bl = $b.limbs.to_a.first
$pl = $p.limbs.to_a.first
$ml = $m.limbs.to_a.first
$all = [$bl, $pl, $ml]
$all.each { |l| l.gravity_override = 0.0 }
$all.each { |l| l.material_velocity_response = 0.0; l.material_stiffness = 0.0; l.material_dampener = 0.0; l.material_kinetic_friction = 0.0; l.material_static_friction = 0.0 }
$show = lambda { |t, l| puts format("%s %+.9e %+.9e %+.9e", t, l.momentum.x, l.momentum.y, l.angular_momentum) }
$put = lambda { |l, x, y, o, px, py, al| l.position = Vector[x, y]; l.orientation = o; l.momentum = Vector[px, py]; l.angular_momentum = al }
$park = lambda { $put.call($bl, 2.0, 7.0, 0.0, 0.0, 0.0, 0.0); $put.call($pl, 7.0, 7.0, 0.0, 0.0, 0.0, 0.0); $put.call($ml, 12.0, 7.0, 0.0, 0.0, 0.0, 0.0) }
$park.call; $put.call($bl, 2.0, 5.0, 0.0, 1.5, -0.9, 0.0)
$e.run_steps(1)
$show.call("C0", $bl)
$park.call; $put.call($bl, 2.0, 5.0, 0.0, 0.0, 0.0, 0.35)
$e.run_steps(1)
$show.call("C1", $bl)
$park.call; $put.call($bl, 2.0, 5.0, 0.0, 1.5, -0.9, 0.35)
$e.run_steps(1)
$show.call("C2", $bl)
$park.call; $put.call($bl, 2.0, 5.0, 1.1, 1.5, -0.9, 0.35)
$e.run_steps(1)
$show.call("C3", $bl)
$park.call; $put.call($pl, 7.0, 5.0, 0.0, 2.5, 1.25, 0.4)
$e.run_steps(1)
$show.call("C4", $pl)
$park.call; $put.call($pl, 7.0, 5.0, -0.7, 2.5, 1.25, 0.4)
$e.run_steps(1)
$show.call("C5", $pl)
$park.call; $put.call($ml, 12.0, 5.0, 0.0, 0.0, 0.0, 3.0)
$e.run_steps(1)
$show.call("C6", $ml)
$park.call; $put.call($ml, 12.0, 5.0, 0.0, 0.0, 0.0, 3.0)
$e.run_steps(20)
$show.call("C7", $ml)
