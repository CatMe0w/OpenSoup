$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[4.0, 3.0]); $b = $r.limbs.to_a.first
$b.gravity_override = 0.0
$b.air_resistance_linear = 0.0
$b.air_resistance_angular = 0.0
$in = $e.input_by_id(1)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.3
$b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.input_grab($b, $in, Vector[4.0, 3.0])
$e.run_steps(1)
puts format("H1a %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_move($in, Vector[4.6, 3.4])
$e.run_steps(10)
puts format("H1b %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.run_steps(50)
puts format("H1c %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_release($b, $in, Vector[4.6, 3.4])
$e.run_steps(20)
puts format("H1d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = -0.3
$b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.input_grab($b, $in, Vector[4.0, 3.0])
$e.run_steps(1)
puts format("H2a %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_move($in, Vector[3.4, 3.4])
$e.run_steps(10)
puts format("H2b %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.run_steps(50)
puts format("H2c %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_release($b, $in, Vector[3.4, 3.4])
$e.run_steps(20)
puts format("H2d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.3
$b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.input_grab($b, $in, Vector[4.2866, 3.0887])
$e.run_steps(1)
puts format("H3a %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_move($in, Vector[4.6, 3.4])
$e.run_steps(10)
puts format("H3b %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.run_steps(50)
puts format("H3c %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_release($b, $in, Vector[4.6, 3.4])
$e.run_steps(20)
puts format("H3d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = -0.3
$b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.input_grab($b, $in, Vector[4.2866, 2.9113])
$e.run_steps(1)
puts format("H4a %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_move($in, Vector[3.4, 3.4])
$e.run_steps(10)
puts format("H4b %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.run_steps(50)
puts format("H4c %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_release($b, $in, Vector[3.4, 3.4])
$e.run_steps(20)
puts format("H4d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.3
$b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.input_grab($b, $in, Vector[4.4299, 3.133])
$e.run_steps(1)
puts format("H5a %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_move($in, Vector[4.9, 3.6])
$e.run_steps(10)
puts format("H5b %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.run_steps(50)
puts format("H5c %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_release($b, $in, Vector[4.9, 3.6])
$e.run_steps(20)
puts format("H5d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = -0.3
$b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.input_grab($b, $in, Vector[4.4299, 2.867])
$e.run_steps(1)
puts format("H6a %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_move($in, Vector[3.5, 3.6])
$e.run_steps(10)
puts format("H6b %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.run_steps(50)
puts format("H6c %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_release($b, $in, Vector[3.5, 3.6])
$e.run_steps(20)
puts format("H6d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$b.position = Vector[4.0, 3.0]; $b.orientation = 0.0
$b.momentum = Vector[0.0, 0.0]; $b.angular_momentum = 0.0
$e.input_grab($b, $in, Vector[4.2, 3.0])
$e.run_steps(1)
puts format("H7a %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_move($in, Vector[4.8, 3.5])
$e.run_steps(10)
puts format("H7b %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.run_steps(50)
puts format("H7c %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
$e.input_release($b, $in, Vector[4.8, 3.5])
$e.run_steps(20)
puts format("H7d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", $b.position.x, $b.position.y, $b.orientation, $b.momentum.x, $b.momentum.y, $b.angular_momentum)
