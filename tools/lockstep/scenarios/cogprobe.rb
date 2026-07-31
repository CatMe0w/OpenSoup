# Gear transmission: teeth interleave for angular-only response.
# G0-G2 three relative orientations, G3-G5 multi-step settle.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$a = SCogLarge.new; $e.toys << $a
$b = SCogLarge.new; $e.toys << $b
$al = $a.limbs.to_a.first
$bl = $b.limbs.to_a.first
$show = lambda { |t| puts format("%s %+.9e %+.9e %+.9e %+.9e", t, $al.orientation, $al.angular_momentum, $bl.orientation, $bl.angular_momentum) }
$set = lambda { |ob, l| $al.position = Vector[6.0, 5.0]; $al.orientation = 0.0; $al.angular_momentum = l; $bl.position = Vector[7.05, 5.0]; $bl.orientation = ob; $bl.angular_momentum = 0.0 }
$set.call(0.0, 4.0)
$e.run_steps(1)
$show.call("G0")
$set.call(0.22, 4.0)
$e.run_steps(1)
$show.call("G1")
$set.call(0.45, 4.0)
$e.run_steps(1)
$show.call("G2")
$set.call(0.22, 4.0)
$e.run_steps(10)
$show.call("G3")
$set.call(0.22, 4.0)
$e.run_steps(60)
$show.call("G4")
$set.call(0.22, -4.0)
$e.run_steps(60)
$show.call("G5")
# R0-R2: does a fixedMove body move when something lands on it?
$r = RampLeft.new; $e.toys << $r
$k = RectangleThin48.new; $e.toys << $k
$rl = $r.limbs.to_a.first
$kl = $k.limbs.to_a.first
$al.position = Vector[2.0, 2.0]; $bl.position = Vector[13.0, 2.0]
$rl.position = Vector[8.0, 4.0]; $rl.orientation = 0.0; $rl.momentum = Vector[0.0, 0.0]; $rl.angular_momentum = 0.0
$kl.position = Vector[8.0, 6.0]; $kl.orientation = 0.0; $kl.momentum = Vector[0.0, 0.0]; $kl.angular_momentum = 0.0
puts format("R0 %+.9e %+.9e %+.9e", $rl.position.x, $rl.position.y, $rl.orientation)
$e.run_steps(60)
puts format("R1 %+.9e %+.9e %+.9e", $rl.position.x, $rl.position.y, $rl.orientation)
puts format("R2 %+.9e %+.9e", $kl.position.y, $kl.momentum.y)
# H0-H1: two fixedMove hubs overlapping - does the group system push them apart?
$kl.position = Vector[2.0, 8.0]; $kl.momentum = Vector[0.0, 0.0]
$rl.position = Vector[13.0, 8.0]
$al.position = Vector[6.0, 5.0]; $al.orientation = 0.0; $al.momentum = Vector[0.0, 0.0]; $al.angular_momentum = 0.0
$bl.position = Vector[6.85, 5.0]; $bl.orientation = 0.22; $bl.momentum = Vector[0.0, 0.0]; $bl.angular_momentum = 0.0
$e.run_steps(30)
puts format("H0 %+.9e %+.9e %+.9e %+.9e", $al.position.x, $al.position.y, $bl.position.x, $bl.position.y)
puts format("H1 %+.9e %+.9e", $al.angular_momentum, $bl.angular_momentum)
