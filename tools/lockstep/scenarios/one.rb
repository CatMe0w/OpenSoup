$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$t = U6Bluebear.new; $e.toys << $t; $t.move(Vector[6.0, 1.5])
$limbs = $t.limbs.to_a
$m = $limbs.map {|l| l.momentum.r }.max
$w = $limbs.map {|l| l.angular_momentum.abs }.max
puts format("n=%4d  max|p|=%9.5f  max|L|=%9.5f", 0, $m, $w)
$e.run_steps(150)
$m = $limbs.map {|l| l.momentum.r }.max
$w = $limbs.map {|l| l.angular_momentum.abs }.max
puts format("n=%4d  max|p|=%9.5f  max|L|=%9.5f", 150, $m, $w)
$e.run_steps(150)
$m = $limbs.map {|l| l.momentum.r }.max
$w = $limbs.map {|l| l.angular_momentum.abs }.max
puts format("n=%4d  max|p|=%9.5f  max|L|=%9.5f", 300, $m, $w)
$e.run_steps(150)
$m = $limbs.map {|l| l.momentum.r }.max
$w = $limbs.map {|l| l.angular_momentum.abs }.max
puts format("n=%4d  max|p|=%9.5f  max|L|=%9.5f", 450, $m, $w)
$e.run_steps(150)
$m = $limbs.map {|l| l.momentum.r }.max
$w = $limbs.map {|l| l.angular_momentum.abs }.max
puts format("n=%4d  max|p|=%9.5f  max|L|=%9.5f", 600, $m, $w)
$e.run_steps(150)
$m = $limbs.map {|l| l.momentum.r }.max
$w = $limbs.map {|l| l.angular_momentum.abs }.max
puts format("n=%4d  max|p|=%9.5f  max|L|=%9.5f", 750, $m, $w)
$e.run_steps(150)
$m = $limbs.map {|l| l.momentum.r }.max
$w = $limbs.map {|l| l.angular_momentum.abs }.max
puts format("n=%4d  max|p|=%9.5f  max|L|=%9.5f", 900, $m, $w)
