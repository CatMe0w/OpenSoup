$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$bears = []
$t = U6Bluebear.new; $e.toys << $t; $t.move(Vector[6.0, 1.5]); $bears << $t
$t = U6Bluebear.new; $e.toys << $t; $t.move(Vector[6.08, 3.2]); $bears << $t
$t = U6Bluebear.new; $e.toys << $t; $t.move(Vector[5.95, 4.9]); $bears << $t
$t = U6Bluebear.new; $e.toys << $t; $t.move(Vector[6.03, 6.6]); $bears << $t
$limbs = $bears.map {|t| t.limbs.to_a }.flatten
$peak = 0.0
$m = $limbs.map {|l| l.momentum.r }.max
$peak = $m if $m > $peak
puts format("n=%4d  max|p|=%10.4f  peak=%10.4f  minY=%7.3f", 0, $m, $peak, $limbs.map{|l| l.position.y}.min)
$e.run_steps(100)
$m = $limbs.map {|l| l.momentum.r }.max
$peak = $m if $m > $peak
puts format("n=%4d  max|p|=%10.4f  peak=%10.4f  minY=%7.3f", 100, $m, $peak, $limbs.map{|l| l.position.y}.min)
$e.run_steps(100)
$m = $limbs.map {|l| l.momentum.r }.max
$peak = $m if $m > $peak
puts format("n=%4d  max|p|=%10.4f  peak=%10.4f  minY=%7.3f", 200, $m, $peak, $limbs.map{|l| l.position.y}.min)
$e.run_steps(100)
$m = $limbs.map {|l| l.momentum.r }.max
$peak = $m if $m > $peak
puts format("n=%4d  max|p|=%10.4f  peak=%10.4f  minY=%7.3f", 300, $m, $peak, $limbs.map{|l| l.position.y}.min)
$e.run_steps(100)
$m = $limbs.map {|l| l.momentum.r }.max
$peak = $m if $m > $peak
puts format("n=%4d  max|p|=%10.4f  peak=%10.4f  minY=%7.3f", 400, $m, $peak, $limbs.map{|l| l.position.y}.min)
$e.run_steps(100)
$m = $limbs.map {|l| l.momentum.r }.max
$peak = $m if $m > $peak
puts format("n=%4d  max|p|=%10.4f  peak=%10.4f  minY=%7.3f", 500, $m, $peak, $limbs.map{|l| l.position.y}.min)
$e.run_steps(100)
$m = $limbs.map {|l| l.momentum.r }.max
$peak = $m if $m > $peak
puts format("n=%4d  max|p|=%10.4f  peak=%10.4f  minY=%7.3f", 600, $m, $peak, $limbs.map{|l| l.position.y}.min)
$e.run_steps(100)
$m = $limbs.map {|l| l.momentum.r }.max
$peak = $m if $m > $peak
puts format("n=%4d  max|p|=%10.4f  peak=%10.4f  minY=%7.3f", 700, $m, $peak, $limbs.map{|l| l.position.y}.min)
$e.run_steps(100)
$m = $limbs.map {|l| l.momentum.r }.max
$peak = $m if $m > $peak
puts format("n=%4d  max|p|=%10.4f  peak=%10.4f  minY=%7.3f", 800, $m, $peak, $limbs.map{|l| l.position.y}.min)
$e.run_steps(100)
$m = $limbs.map {|l| l.momentum.r }.max
$peak = $m if $m > $peak
puts format("n=%4d  max|p|=%10.4f  peak=%10.4f  minY=%7.3f", 900, $m, $peak, $limbs.map{|l| l.position.y}.min)
