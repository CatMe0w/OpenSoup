# Magnets: range, force law, identity guards. One step, gravity and air off, so bearing momentum = magnet force alone.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$m = Magnet.new; $e.toys << $m
$b = BallBearing.new; $e.toys << $b
$s = SquobGreen.new; $e.toys << $s
$ml = $m.limbs.to_a.first
$bl = $b.limbs.to_a.first
$sl = $s.limbs.to_a.first
$all = [$ml, $bl, $sl]
$all.each { |l| l.gravity_override = 0.0; l.air_resistance_linear = 0.0; l.air_resistance_angular = 0.0 }
$park = lambda { $all.each { |l| l.momentum = Vector[0.0, 0.0]; l.angular_momentum = 0.0; l.orientation = 0.0 } }
$show = lambda { |t, l| puts format("%s %+.9e %+.9e %+.9e", t, l.momentum.x, l.momentum.y, l.angular_momentum) }
$sl.position = Vector[13.0, 8.0]
$park.call; $ml.position = Vector[4.0, 5.0]; $bl.position = Vector[4.5, 5.0]
$e.run_steps(1)
$show.call("M0b", $bl); $show.call("M0m", $ml)
$park.call; $ml.position = Vector[4.0, 5.0]; $bl.position = Vector[6.0, 5.0]
$e.run_steps(1)
$show.call("M1b", $bl); $show.call("M1m", $ml)
$park.call; $ml.position = Vector[4.0, 5.0]; $bl.position = Vector[4.0, 5.6]
$e.run_steps(1)
$show.call("M2b", $bl); $show.call("M2m", $ml)
$park.call; $ml.position = Vector[4.0, 5.0]; $bl.position = Vector[3.6, 5.0]; $bl.momentum = Vector[0.0, 6.0]
$e.run_steps(1)
$show.call("M3b", $bl); $show.call("M3m", $ml)
$park.call; $ml.position = Vector[4.0, 5.0]; $bl.position = Vector[4.5, 5.0]; $ml.orientation = 1.0
$e.run_steps(1)
$show.call("M4b", $bl); $show.call("M4m", $ml)
$park.call; $ml.position = Vector[1.0, 2.0]; $bl.position = Vector[13.5, 8.5]; $sl.position = Vector[8.0, 5.0]
$e.run_steps(1)
$show.call("M5s", $sl)
