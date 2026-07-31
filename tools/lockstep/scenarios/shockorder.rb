# Limb#shock_order: read off World walls and ordinary limbs, before and after a step (the per-step sort renumbers).
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$t = $e.toys.to_a
puts format("T %d %s", $t.size, $t.first.toy_id)
$wl = $t.first.limbs.to_a
puts format("W %d", $wl.size)
puts format("W0 %s", $wl[0].shock_order.inspect)
puts format("W1 %s", $wl[1].shock_order.inspect)
puts format("W2 %s", $wl[2].shock_order.inspect)
puts format("W3 %s", $wl[3].shock_order.inspect)
$r0 = RectangleThin48.new; $e.toys << $r0
$r1 = RectangleThin48.new; $e.toys << $r1
$a = $r0.limbs.to_a.first
$b = $r1.limbs.to_a.first
$a.position = Vector[4.0, 2.0]; $b.position = Vector[4.0, 6.0]
puts format("B0 %s %s", $a.shock_order.inspect, $b.shock_order.inspect)
$e.run_steps(1)
puts format("B1 %s %s", $a.shock_order.inspect, $b.shock_order.inspect)
puts format("W1a %s %s", $wl[0].shock_order.inspect, $wl[2].shock_order.inspect)
$a.position = Vector[4.0, 8.0]; $b.position = Vector[4.0, 1.0]
$e.run_steps(1)
puts format("B2 %s %s", $a.shock_order.inspect, $b.shock_order.inspect)
$a.fixed_move = true
$e.run_steps(1)
puts format("B3 %s %s", $a.shock_order.inspect, $b.shock_order.inspect)
$a.fixed_move = false
$e.run_steps(1)
puts format("B4 %s %s", $a.shock_order.inspect, $b.shock_order.inspect)
