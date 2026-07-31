# Does a fixedMove hoop stay still when 40 net limbs pull on it via joints?
# F0 after spawn, F1 after 300 steps, F2-F3 a net limb for reference.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$h = SCHoop.new; $e.toys << $h
$hl = $h.limbs.to_a.first
$nl = $h.limbs.to_a[20]
$hl.position = Vector[6.0, 5.0]; $hl.orientation = 0.0
puts format("F0 %+.9e %+.9e %+.9e", $hl.position.x, $hl.position.y, $hl.orientation)
$e.run_steps(300)
puts format("F1 %+.9e %+.9e %+.9e", $hl.position.x, $hl.position.y, $hl.orientation)
puts format("F2 %+.9e %+.9e", $nl.position.x, $nl.position.y)
$e.run_steps(300)
puts format("F3 %+.9e %+.9e %+.9e", $hl.position.x, $hl.position.y, $hl.orientation)
puts format("F4 %+.9e %+.9e", $nl.position.x, $nl.position.y)
