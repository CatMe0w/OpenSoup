$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$a = U6Bluebear.new; $e.toys << $a; $a.move(Vector[6.4, 1.0])
$e.run_steps(300)
$b = U6Bluebear.new; $e.toys << $b; $b.move(Vector[6.4, 4.5])
$limbs = ($a.limbs.to_a + $b.limbs.to_a)
$comx = lambda { $limbs.inject(0.0){|s,l| s + l.position.x*l.mass} / $limbs.inject(0.0){|s,l| s+l.mass} }
$netp = lambda { $limbs.inject(0.0){|s,l| s + l.momentum.x} }
puts format("n=%4d  comX=%8.4f  netPx=%9.4f  maxP=%8.4f", 0, $comx.call, $netp.call, $limbs.map{|l| l.momentum.r}.max)
$e.run_steps(150)
puts format("n=%4d  comX=%8.4f  netPx=%9.4f  maxP=%8.4f", 150, $comx.call, $netp.call, $limbs.map{|l| l.momentum.r}.max)
$e.run_steps(150)
puts format("n=%4d  comX=%8.4f  netPx=%9.4f  maxP=%8.4f", 300, $comx.call, $netp.call, $limbs.map{|l| l.momentum.r}.max)
$e.run_steps(150)
puts format("n=%4d  comX=%8.4f  netPx=%9.4f  maxP=%8.4f", 450, $comx.call, $netp.call, $limbs.map{|l| l.momentum.r}.max)
$e.run_steps(150)
puts format("n=%4d  comX=%8.4f  netPx=%9.4f  maxP=%8.4f", 600, $comx.call, $netp.call, $limbs.map{|l| l.momentum.r}.max)
$e.run_steps(150)
puts format("n=%4d  comX=%8.4f  netPx=%9.4f  maxP=%8.4f", 750, $comx.call, $netp.call, $limbs.map{|l| l.momentum.r}.max)
$e.run_steps(150)
puts format("n=%4d  comX=%8.4f  netPx=%9.4f  maxP=%8.4f", 900, $comx.call, $netp.call, $limbs.map{|l| l.momentum.r}.max)
$e.run_steps(150)
puts format("n=%4d  comX=%8.4f  netPx=%9.4f  maxP=%8.4f", 1050, $comx.call, $netp.call, $limbs.map{|l| l.momentum.r}.max)
$e.run_steps(150)
puts format("n=%4d  comX=%8.4f  netPx=%9.4f  maxP=%8.4f", 1200, $comx.call, $netp.call, $limbs.map{|l| l.momentum.r}.max)
