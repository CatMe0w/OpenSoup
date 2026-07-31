$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$b1 = Basketball.new; $e.toys << $b1; $b1.move(Vector[3.0, 0.5])
$b2 = Basketball.new; $e.toys << $b2; $b2.move(Vector[3.02, 1.6])
$k1 = U6Bluebear.new; $e.toys << $k1; $k1.move(Vector[8.0, 1.5])
$k2 = U6Bluebear.new; $e.toys << $k2; $k2.move(Vector[8.05, 3.2])
$balls = ($b1.limbs.to_a + $b2.limbs.to_a)
$bears = ($k1.limbs.to_a + $k2.limbs.to_a)
$mb = $balls.map {|l| l.momentum.r }.max
$mk = $bears.map {|l| l.momentum.r }.max
puts format("n=%4d  2balls max|p|=%9.5f   2bears max|p|=%9.5f", 0, $mb, $mk)
$e.run_steps(200)
$mb = $balls.map {|l| l.momentum.r }.max
$mk = $bears.map {|l| l.momentum.r }.max
puts format("n=%4d  2balls max|p|=%9.5f   2bears max|p|=%9.5f", 200, $mb, $mk)
$e.run_steps(200)
$mb = $balls.map {|l| l.momentum.r }.max
$mk = $bears.map {|l| l.momentum.r }.max
puts format("n=%4d  2balls max|p|=%9.5f   2bears max|p|=%9.5f", 400, $mb, $mk)
$e.run_steps(200)
$mb = $balls.map {|l| l.momentum.r }.max
$mk = $bears.map {|l| l.momentum.r }.max
puts format("n=%4d  2balls max|p|=%9.5f   2bears max|p|=%9.5f", 600, $mb, $mk)
$e.run_steps(200)
$mb = $balls.map {|l| l.momentum.r }.max
$mk = $bears.map {|l| l.momentum.r }.max
puts format("n=%4d  2balls max|p|=%9.5f   2bears max|p|=%9.5f", 800, $mb, $mk)
$e.run_steps(200)
$mb = $balls.map {|l| l.momentum.r }.max
$mk = $bears.map {|l| l.momentum.r }.max
puts format("n=%4d  2balls max|p|=%9.5f   2bears max|p|=%9.5f", 1000, $mb, $mk)
$e.run_steps(200)
$mb = $balls.map {|l| l.momentum.r }.max
$mk = $bears.map {|l| l.momentum.r }.max
puts format("n=%4d  2balls max|p|=%9.5f   2bears max|p|=%9.5f", 1200, $mb, $mk)
