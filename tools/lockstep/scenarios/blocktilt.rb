$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$r = RectangleThin48.new; $e.toys << $r; $r.move(Vector[6.4, 0.101]); $blk = $r.limbs.to_a.first
$e.run_steps(120)
$bx = $blk.position.x
$by = $blk.position.y
$t = U6Bluebear.new; $e.toys << $t; $t.move(Vector[6.4, 4.0])
$ls = $t.limbs.to_a
$n = $ls.length
$px = $ls.map { |l| l.position.x }
$py = $ls.map { |l| l.position.y }
$po = $ls.map { |l| l.orientation }
$blk.position = Vector[$bx, $by]; $blk.orientation = 0.0
$blk.momentum = Vector[0.0, 0.0]; $blk.angular_momentum = 0.0
$n.times { |i| $ls[i].position = Vector[$px[i] + -0.45, $py[i] - 0.6] }
$n.times { |i| $ls[i].orientation = $po[i] }
$n.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
$n.times { |i| $ls[i].angular_momentum = 0.0 }
$e.run_steps(1500)
$lo = $ls.map { |l| l.position.y }.min
puts format("T00 dx -0.45  blkx %+.9e  blky %+.9e  blko %+.9e  low %+.9e", $blk.position.x, $blk.position.y, $blk.orientation, $lo)
$blk.position = Vector[$bx, $by]; $blk.orientation = 0.0
$blk.momentum = Vector[0.0, 0.0]; $blk.angular_momentum = 0.0
$n.times { |i| $ls[i].position = Vector[$px[i] + -0.3, $py[i] - 0.6] }
$n.times { |i| $ls[i].orientation = $po[i] }
$n.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
$n.times { |i| $ls[i].angular_momentum = 0.0 }
$e.run_steps(1500)
$lo = $ls.map { |l| l.position.y }.min
puts format("T01 dx -0.30  blkx %+.9e  blky %+.9e  blko %+.9e  low %+.9e", $blk.position.x, $blk.position.y, $blk.orientation, $lo)
$blk.position = Vector[$bx, $by]; $blk.orientation = 0.0
$blk.momentum = Vector[0.0, 0.0]; $blk.angular_momentum = 0.0
$n.times { |i| $ls[i].position = Vector[$px[i] + -0.2, $py[i] - 0.6] }
$n.times { |i| $ls[i].orientation = $po[i] }
$n.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
$n.times { |i| $ls[i].angular_momentum = 0.0 }
$e.run_steps(1500)
$lo = $ls.map { |l| l.position.y }.min
puts format("T02 dx -0.20  blkx %+.9e  blky %+.9e  blko %+.9e  low %+.9e", $blk.position.x, $blk.position.y, $blk.orientation, $lo)
$blk.position = Vector[$bx, $by]; $blk.orientation = 0.0
$blk.momentum = Vector[0.0, 0.0]; $blk.angular_momentum = 0.0
$n.times { |i| $ls[i].position = Vector[$px[i] + -0.1, $py[i] - 0.6] }
$n.times { |i| $ls[i].orientation = $po[i] }
$n.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
$n.times { |i| $ls[i].angular_momentum = 0.0 }
$e.run_steps(1500)
$lo = $ls.map { |l| l.position.y }.min
puts format("T03 dx -0.10  blkx %+.9e  blky %+.9e  blko %+.9e  low %+.9e", $blk.position.x, $blk.position.y, $blk.orientation, $lo)
$blk.position = Vector[$bx, $by]; $blk.orientation = 0.0
$blk.momentum = Vector[0.0, 0.0]; $blk.angular_momentum = 0.0
$n.times { |i| $ls[i].position = Vector[$px[i] + 0.0, $py[i] - 0.6] }
$n.times { |i| $ls[i].orientation = $po[i] }
$n.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
$n.times { |i| $ls[i].angular_momentum = 0.0 }
$e.run_steps(1500)
$lo = $ls.map { |l| l.position.y }.min
puts format("T04 dx +0.00  blkx %+.9e  blky %+.9e  blko %+.9e  low %+.9e", $blk.position.x, $blk.position.y, $blk.orientation, $lo)
$blk.position = Vector[$bx, $by]; $blk.orientation = 0.0
$blk.momentum = Vector[0.0, 0.0]; $blk.angular_momentum = 0.0
$n.times { |i| $ls[i].position = Vector[$px[i] + 0.1, $py[i] - 0.6] }
$n.times { |i| $ls[i].orientation = $po[i] }
$n.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
$n.times { |i| $ls[i].angular_momentum = 0.0 }
$e.run_steps(1500)
$lo = $ls.map { |l| l.position.y }.min
puts format("T05 dx +0.10  blkx %+.9e  blky %+.9e  blko %+.9e  low %+.9e", $blk.position.x, $blk.position.y, $blk.orientation, $lo)
$blk.position = Vector[$bx, $by]; $blk.orientation = 0.0
$blk.momentum = Vector[0.0, 0.0]; $blk.angular_momentum = 0.0
$n.times { |i| $ls[i].position = Vector[$px[i] + 0.2, $py[i] - 0.6] }
$n.times { |i| $ls[i].orientation = $po[i] }
$n.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
$n.times { |i| $ls[i].angular_momentum = 0.0 }
$e.run_steps(1500)
$lo = $ls.map { |l| l.position.y }.min
puts format("T06 dx +0.20  blkx %+.9e  blky %+.9e  blko %+.9e  low %+.9e", $blk.position.x, $blk.position.y, $blk.orientation, $lo)
$blk.position = Vector[$bx, $by]; $blk.orientation = 0.0
$blk.momentum = Vector[0.0, 0.0]; $blk.angular_momentum = 0.0
$n.times { |i| $ls[i].position = Vector[$px[i] + 0.3, $py[i] - 0.6] }
$n.times { |i| $ls[i].orientation = $po[i] }
$n.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
$n.times { |i| $ls[i].angular_momentum = 0.0 }
$e.run_steps(1500)
$lo = $ls.map { |l| l.position.y }.min
puts format("T07 dx +0.30  blkx %+.9e  blky %+.9e  blko %+.9e  low %+.9e", $blk.position.x, $blk.position.y, $blk.orientation, $lo)
$blk.position = Vector[$bx, $by]; $blk.orientation = 0.0
$blk.momentum = Vector[0.0, 0.0]; $blk.angular_momentum = 0.0
$n.times { |i| $ls[i].position = Vector[$px[i] + 0.45, $py[i] - 0.6] }
$n.times { |i| $ls[i].orientation = $po[i] }
$n.times { |i| $ls[i].momentum = Vector[0.0, 0.0] }
$n.times { |i| $ls[i].angular_momentum = 0.0 }
$e.run_steps(1500)
$lo = $ls.map { |l| l.position.y }.min
puts format("T08 dx +0.45  blkx %+.9e  blky %+.9e  blko %+.9e  low %+.9e", $blk.position.x, $blk.position.y, $blk.orientation, $lo)
