$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$r1 = RectangleThin48.new; $e.toys << $r1; $r1.move(Vector[4.0, 0.101]); $b = $r1.limbs.to_a.first
$r2 = RectangleThin48.new; $e.toys << $r2; $r2.move(Vector[4.0, 0.40]); $u = $r2.limbs.to_a.first
$u.orientation = 0.05
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 0, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
$e.run_steps(10)
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 10, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
$e.run_steps(10)
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 20, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
$e.run_steps(10)
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 30, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
$e.run_steps(10)
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 40, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
$e.run_steps(10)
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 50, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
$e.run_steps(10)
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 60, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
$e.run_steps(10)
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 70, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
$e.run_steps(10)
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 80, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
$e.run_steps(10)
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 90, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
$e.run_steps(10)
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 100, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
$e.run_steps(10)
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 110, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
$e.run_steps(10)
puts format("n=%3d low(x=%+.4f y=%.4f o=%+.4f)  up(x=%+.4f y=%.4f o=%+.4f)  px=(%+.3f,%+.3f)", 120, $b.position.x, $b.position.y, $b.orientation, $u.position.x, $u.position.y, $u.orientation, $b.momentum.x, $u.momentum.x)
