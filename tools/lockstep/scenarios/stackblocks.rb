$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$bs = []
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[4.0, 0.2]); $bs << $r.limbs.to_a.first
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[4.0, 0.55]); $bs << $r.limbs.to_a.first
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[4.0, 0.9]); $bs << $r.limbs.to_a.first
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[4.0, 1.25]); $bs << $r.limbs.to_a.first
$tilt = lambda { $bs.map {|l| l.orientation.abs }.max }
$mp = lambda { $bs.map {|l| l.momentum.r }.max }
$spread = lambda { $bs.map {|l| l.position.x }.max - $bs.map {|l| l.position.x }.min }
puts format("n=%4d  maxTilt=%9.6f  maxP=%9.5f  spreadX=%8.5f  ys=%s", 0, $tilt.call, $mp.call, $spread.call, $bs.map{|l| format("%.3f", l.position.y)}.join(","))
$e.run_steps(200)
puts format("n=%4d  maxTilt=%9.6f  maxP=%9.5f  spreadX=%8.5f  ys=%s", 200, $tilt.call, $mp.call, $spread.call, $bs.map{|l| format("%.3f", l.position.y)}.join(","))
$e.run_steps(200)
puts format("n=%4d  maxTilt=%9.6f  maxP=%9.5f  spreadX=%8.5f  ys=%s", 400, $tilt.call, $mp.call, $spread.call, $bs.map{|l| format("%.3f", l.position.y)}.join(","))
$e.run_steps(200)
puts format("n=%4d  maxTilt=%9.6f  maxP=%9.5f  spreadX=%8.5f  ys=%s", 600, $tilt.call, $mp.call, $spread.call, $bs.map{|l| format("%.3f", l.position.y)}.join(","))
$e.run_steps(200)
puts format("n=%4d  maxTilt=%9.6f  maxP=%9.5f  spreadX=%8.5f  ys=%s", 800, $tilt.call, $mp.call, $spread.call, $bs.map{|l| format("%.3f", l.position.y)}.join(","))
$e.run_steps(200)
puts format("n=%4d  maxTilt=%9.6f  maxP=%9.5f  spreadX=%8.5f  ys=%s", 1000, $tilt.call, $mp.call, $spread.call, $bs.map{|l| format("%.3f", l.position.y)}.join(","))
$e.run_steps(200)
puts format("n=%4d  maxTilt=%9.6f  maxP=%9.5f  spreadX=%8.5f  ys=%s", 1200, $tilt.call, $mp.call, $spread.call, $bs.map{|l| format("%.3f", l.position.y)}.join(","))
