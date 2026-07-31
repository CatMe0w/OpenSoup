$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[4.0, 1.0])
$l = $r.limbs.to_a.first
puts format("n=%4d  y=%8.5f  orient=%+10.6f  p=(%+.5f,%+.5f)  L=%+.6f", 0, $l.position.y, $l.orientation, $l.momentum.x, $l.momentum.y, $l.angular_momentum)
$e.run_steps(200)
puts format("n=%4d  y=%8.5f  orient=%+10.6f  p=(%+.5f,%+.5f)  L=%+.6f", 200, $l.position.y, $l.orientation, $l.momentum.x, $l.momentum.y, $l.angular_momentum)
$e.run_steps(200)
puts format("n=%4d  y=%8.5f  orient=%+10.6f  p=(%+.5f,%+.5f)  L=%+.6f", 400, $l.position.y, $l.orientation, $l.momentum.x, $l.momentum.y, $l.angular_momentum)
$e.run_steps(200)
puts format("n=%4d  y=%8.5f  orient=%+10.6f  p=(%+.5f,%+.5f)  L=%+.6f", 600, $l.position.y, $l.orientation, $l.momentum.x, $l.momentum.y, $l.angular_momentum)
$e.run_steps(200)
puts format("n=%4d  y=%8.5f  orient=%+10.6f  p=(%+.5f,%+.5f)  L=%+.6f", 800, $l.position.y, $l.orientation, $l.momentum.x, $l.momentum.y, $l.angular_momentum)
$e.run_steps(200)
puts format("n=%4d  y=%8.5f  orient=%+10.6f  p=(%+.5f,%+.5f)  L=%+.6f", 1000, $l.position.y, $l.orientation, $l.momentum.x, $l.momentum.y, $l.angular_momentum)
$e.run_steps(200)
puts format("n=%4d  y=%8.5f  orient=%+10.6f  p=(%+.5f,%+.5f)  L=%+.6f", 1200, $l.position.y, $l.orientation, $l.momentum.x, $l.momentum.y, $l.angular_momentum)
