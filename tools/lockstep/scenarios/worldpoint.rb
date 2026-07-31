# Limb#to_world / to_local: result is on the float grid, not double.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$r0 = RectangleThin48.new; $e.toys << $r0
$a = $r0.limbs.to_a.first
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.0
puts format("W1 %+.9e %+.9e", $a.to_world(Vector[0.48, 0.108]).x, $a.to_world(Vector[0.48, 0.108]).y)
$a.position = Vector[6.0, 4.5]
puts format("W2 %+.9e %+.9e", $a.to_world(Vector[0.48, 0.108]).x, $a.to_world(Vector[0.48, 0.108]).y)
$a.position = Vector[8.0, 4.5]; $a.orientation = 0.7
puts format("W3 %+.9e %+.9e", $a.to_world(Vector[0.48, 0.108]).x, $a.to_world(Vector[0.48, 0.108]).y)
$a.orientation = -2.3
puts format("W4 %+.9e %+.9e", $a.to_world(Vector[-0.31, 0.77]).x, $a.to_world(Vector[-0.31, 0.77]).y)
$a.orientation = 0.0
puts format("W5 %+.9e %+.9e", $a.to_local(Vector[8.48, 4.608]).x, $a.to_local(Vector[8.48, 4.608]).y)
$a.orientation = 1.1
puts format("W6 %+.9e %+.9e", $a.to_local(Vector[8.48, 4.608]).x, $a.to_local(Vector[8.48, 4.608]).y)
