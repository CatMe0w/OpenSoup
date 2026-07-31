# Read body-local collision geometry back via Shape#vertices (after base scale).
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-15.12, -9.49]
$e.scene_walls_changed
$r0 = RectangleThin48.new; $e.toys << $r0
$b0 = U6Bluebear.new; $e.toys << $b0
$f = lambda { |t, v| puts format("%s %+.9e %+.9e %+.9e", t, v[0].x, v[0].y, v[1]) }
$s = $r0.limbs.to_a.first.shapes.to_a[0]
$f.call("R0.0", $s.vertices[0]); $f.call("R0.1", $s.vertices[1])
$f.call("R0.2", $s.vertices[2]); $f.call("R0.3", $s.vertices[3])
$s = $r0.limbs.to_a.first.shapes.to_a[1]
$f.call("R1.0", $s.vertices[0]); $f.call("R1.1", $s.vertices[1])
$f.call("R1.2", $s.vertices[2]); $f.call("R1.3", $s.vertices[3])
$l = $b0.limbs.to_a[0]
$s = $l.shapes.to_a[0]
puts format("B0.n %d", $s.vertices.size)
$f.call("B0.0", $s.vertices[0])
$l = $b0.limbs.to_a[3]
$s = $l.shapes.to_a[0]
puts format("B3.n %d", $s.vertices.size)
$f.call("B3.0", $s.vertices[0])
