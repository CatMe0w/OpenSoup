# Wall visit order: two wall contacts accumulate in one slot
# (float addition is not associative). C0-C3 four corners, C4 asymmetric spin.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$r = RectangleThin48.new; $e.toys << $r
$l = $r.limbs.to_a.first
$l.gravity_override = 0.0; $l.air_resistance_linear = 0.0; $l.air_resistance_angular = 0.0
$show = lambda { |t| puts format("%s %+.9e %+.9e %+.9e", t, $l.momentum.x, $l.momentum.y, $l.angular_momentum) }
$put = lambda { |x, y, o| $l.position = Vector[x, y]; $l.orientation = o; $l.momentum = Vector[0.0, 0.0]; $l.angular_momentum = 0.0 }
$put.call(0.4, 0.05, 0.0)
$e.run_steps(1)
$show.call("C0")
$put.call(14.75, 0.05, 0.0)
$e.run_steps(1)
$show.call("C1")
$put.call(0.4, 9.44, 0.0)
$e.run_steps(1)
$show.call("C2")
$put.call(14.75, 9.44, 0.0)
$e.run_steps(1)
$show.call("C3")
$put.call(0.42, 0.08, 0.4)
$e.run_steps(1)
$show.call("C4")
$put.call(14.7, 9.41, -0.4)
$e.run_steps(1)
$show.call("C5")
$put.call(0.05, 4.5, 1.5707963267948966)
$e.run_steps(1)
$show.call("C6")
