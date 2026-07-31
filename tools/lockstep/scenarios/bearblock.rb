$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[6.4, 0.101]); $blk = $r.limbs.to_a.first
$e.run_steps(60)
$t = U6Bluebear.new; $e.toys << $t; $t.move(Vector[6.5, 5.0])
$ls = $t.limbs.to_a
$all = [$blk] + $ls
$e.run_steps(60)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 60, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(5)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 65, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(5)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 70, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(5)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 75, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(5)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 80, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(5)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 85, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(5)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 90, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(5)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 95, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(5)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 100, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(5)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 105, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(5)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 110, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(5)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 115, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(5)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 120, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(10)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 130, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(10)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 140, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(10)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 150, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(25)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 175, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(25)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 200, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(50)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 250, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(50)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 300, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(100)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 400, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(200)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 600, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(300)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 900, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
$e.run_steps(600)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
puts format("D%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 1500, $blk.position.x, $blk.position.y, $blk.orientation, $ls[0].position.x, $ls[0].position.y, $npx)
