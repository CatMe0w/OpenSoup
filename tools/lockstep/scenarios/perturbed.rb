$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$bs = []
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[4.0, 0.3]); $l = $r.limbs.to_a.first; $l.orientation = 0.0; $bs << $l
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[4.06, 0.75]); $l = $r.limbs.to_a.first; $l.orientation = 0.05; $bs << $l
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[3.95, 1.2]); $l = $r.limbs.to_a.first; $l.orientation = -0.04; $bs << $l
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[4.03, 1.65]); $l = $r.limbs.to_a.first; $l.orientation = 0.02; $bs << $l
$tilt = lambda { $bs.map {|l| l.orientation }.map{|o| o.abs}.max }
$mp = lambda { $bs.map {|l| l.momentum.r }.max }
puts format("n=%4d maxTilt=%9.6f maxP=%9.5f xs=%s", 0, $tilt.call, $mp.call, $bs.map{|l| format("%.3f", l.position.x)}.join(","))
puts format("        ys=%s  os=%s", $bs.map{|l| format("%.3f", l.position.y)}.join(","), $bs.map{|l| format("%+.3f", l.orientation)}.join(","))
$e.run_steps(200)
puts format("n=%4d maxTilt=%9.6f maxP=%9.5f xs=%s", 200, $tilt.call, $mp.call, $bs.map{|l| format("%.3f", l.position.x)}.join(","))
puts format("        ys=%s  os=%s", $bs.map{|l| format("%.3f", l.position.y)}.join(","), $bs.map{|l| format("%+.3f", l.orientation)}.join(","))
$e.run_steps(200)
puts format("n=%4d maxTilt=%9.6f maxP=%9.5f xs=%s", 400, $tilt.call, $mp.call, $bs.map{|l| format("%.3f", l.position.x)}.join(","))
puts format("        ys=%s  os=%s", $bs.map{|l| format("%.3f", l.position.y)}.join(","), $bs.map{|l| format("%+.3f", l.orientation)}.join(","))
$e.run_steps(200)
puts format("n=%4d maxTilt=%9.6f maxP=%9.5f xs=%s", 600, $tilt.call, $mp.call, $bs.map{|l| format("%.3f", l.position.x)}.join(","))
puts format("        ys=%s  os=%s", $bs.map{|l| format("%.3f", l.position.y)}.join(","), $bs.map{|l| format("%+.3f", l.orientation)}.join(","))
$e.run_steps(200)
puts format("n=%4d maxTilt=%9.6f maxP=%9.5f xs=%s", 800, $tilt.call, $mp.call, $bs.map{|l| format("%.3f", l.position.x)}.join(","))
puts format("        ys=%s  os=%s", $bs.map{|l| format("%.3f", l.position.y)}.join(","), $bs.map{|l| format("%+.3f", l.orientation)}.join(","))
$e.run_steps(200)
puts format("n=%4d maxTilt=%9.6f maxP=%9.5f xs=%s", 1000, $tilt.call, $mp.call, $bs.map{|l| format("%.3f", l.position.x)}.join(","))
puts format("        ys=%s  os=%s", $bs.map{|l| format("%.3f", l.position.y)}.join(","), $bs.map{|l| format("%+.3f", l.orientation)}.join(","))
$e.run_steps(200)
puts format("n=%4d maxTilt=%9.6f maxP=%9.5f xs=%s", 1200, $tilt.call, $mp.call, $bs.map{|l| format("%.3f", l.position.x)}.join(","))
puts format("        ys=%s  os=%s", $bs.map{|l| format("%.3f", l.position.y)}.join(","), $bs.map{|l| format("%+.3f", l.orientation)}.join(","))
