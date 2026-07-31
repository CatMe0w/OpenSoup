$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$t = U6Bluebear.new
$e.toys << $t
$t.move(Vector[6.0, 7.0])
$b = $t.limbs.to_a.find {|l| l.sid.to_s == "Body" }
puts format("BEAR n=  0  y=%8.4f py=%9.4f", $b.position.y, $b.momentum.y)
$e.run_steps(10)
puts format("BEAR n= 10  y=%8.4f py=%9.4f", $b.position.y, $b.momentum.y)
$e.run_steps(40)
puts format("BEAR n= 50  y=%8.4f py=%9.4f", $b.position.y, $b.momentum.y)
$e.run_steps(50)
puts format("BEAR n=100  y=%8.4f py=%9.4f", $b.position.y, $b.momentum.y)
$e.run_steps(200)
puts format("BEAR n=300  y=%8.4f py=%9.4f", $b.position.y, $b.momentum.y)
