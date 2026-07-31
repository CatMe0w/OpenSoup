$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[6.4, 0.101]); $blk = $r.limbs.to_a.first
$e.run_steps(60)
$t = U6Bluebear.new; $e.toys << $t; $t.move(Vector[6.5, 4.0])
$ls = $t.limbs.to_a
$all = [$blk] + $ls
$in = $e.input_by_id(1)
$h = $ls[1]
$e.input_grab($h, $in, $h.position)
$e.run_steps(10)
$e.input_move($in, Vector[6.10, 4.30])
$e.run_steps(15)
$e.input_move($in, Vector[6.55, 4.05])
$e.run_steps(15)
$e.input_release($h, $in, Vector[6.55, 4.05])
$e.run_steps(40)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
$lo = $ls.map { |l| l.position.y }.min
puts format("P%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 40, $blk.position.x, $blk.position.y, $blk.orientation, $ls[1].position.x, $ls[1].position.y, $npx, $lo)
$e.run_steps(40)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
$lo = $ls.map { |l| l.position.y }.min
puts format("P%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 80, $blk.position.x, $blk.position.y, $blk.orientation, $ls[1].position.x, $ls[1].position.y, $npx, $lo)
$e.run_steps(40)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
$lo = $ls.map { |l| l.position.y }.min
puts format("P%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 120, $blk.position.x, $blk.position.y, $blk.orientation, $ls[1].position.x, $ls[1].position.y, $npx, $lo)
$e.run_steps(40)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
$lo = $ls.map { |l| l.position.y }.min
puts format("P%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 160, $blk.position.x, $blk.position.y, $blk.orientation, $ls[1].position.x, $ls[1].position.y, $npx, $lo)
$e.run_steps(40)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
$lo = $ls.map { |l| l.position.y }.min
puts format("P%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 200, $blk.position.x, $blk.position.y, $blk.orientation, $ls[1].position.x, $ls[1].position.y, $npx, $lo)
$e.run_steps(60)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
$lo = $ls.map { |l| l.position.y }.min
puts format("P%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 260, $blk.position.x, $blk.position.y, $blk.orientation, $ls[1].position.x, $ls[1].position.y, $npx, $lo)
$e.run_steps(90)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
$lo = $ls.map { |l| l.position.y }.min
puts format("P%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 350, $blk.position.x, $blk.position.y, $blk.orientation, $ls[1].position.x, $ls[1].position.y, $npx, $lo)
$e.run_steps(150)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
$lo = $ls.map { |l| l.position.y }.min
puts format("P%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 500, $blk.position.x, $blk.position.y, $blk.orientation, $ls[1].position.x, $ls[1].position.y, $npx, $lo)
$e.run_steps(300)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
$lo = $ls.map { |l| l.position.y }.min
puts format("P%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 800, $blk.position.x, $blk.position.y, $blk.orientation, $ls[1].position.x, $ls[1].position.y, $npx, $lo)
$e.run_steps(700)
$npx = $all.inject(0.0) { |s, l| s + l.momentum.x }
$lo = $ls.map { |l| l.position.y }.min
puts format("P%04d %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e %+.9e", 1500, $blk.position.x, $blk.position.y, $blk.orientation, $ls[1].position.x, $ls[1].position.y, $npx, $lo)
