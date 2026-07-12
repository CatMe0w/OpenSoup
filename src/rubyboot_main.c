// Headless Ruby boot check: framework load through World instantiation,
// no window. Exits 0 iff the whole boot + wall-position round trip works.
#include "rubyhost.h"
#include "toydefs.h"
#include <stdio.h>

int main(int argc, char** argv) {
    const char* assets = argc > 1 ? argv[1] : "private/extracted";
    char path[1024];

    snprintf(path, sizeof path, "%s/toydefs.json", assets);
    if (!toydefs_load(path)) {
        fprintf(stderr, "rubyboot: cannot load %s\n", path);
        return 1;
    }

    snprintf(path, sizeof path, "%s/souptoys_core_toy", assets);
    bool ok = rbh_boot(path);

    // Milestone: World toy exists per engine, walls follow the scene rect
    // (REngine#core_loaded ran World.new; setting the scene fires
    // scene_walls_changed -> World#walls_changed -> Limb#position=).
    if (ok) {
        ok = rbh_eval(
            "w = $default_engine.toys.by_sid(:world)\n"
            "raise 'world toy missing' unless w\n"
            "raise 'not a World' unless w.is_a?(World)\n"
            "raise 'not sticky' unless w.is_sticky?\n"
            "$default_engine.scene_bottom_left = Vector[0.0, 0.0]\n"
            "$default_engine.scene_top_right = Vector[-12.8, -8.0]\n"
            "names = [:left_wall, :right_wall, :floor, :ceiling]\n"
            "ls = names.map {|s| w.limbs.by_sid(s) or raise \"no #{s}\"}\n"
            "STDERR.puts 'rubyboot: world=' + w.toy_instance_id.to_s +\n"
            "  ' walls=' + ls.map {|l| format('%g,%g', l.position.x, l.position.y)}.join(' ')\n"
            "raise 'right wall' unless (ls[1].position.x - 12.8).abs < 1e-6\n"
            "raise 'ceiling' unless (ls[3].position.y - 8.0).abs < 1e-6\n"
            "raise 'scale not refit' unless ($default_engine.scale - 100.0).abs < 1e-6\n",
            "world verification");
    }

    // Ruby-path toy instantiation: bluebear (no script -> resolver creates a
    // bare Toy subclass), realized into physics on toys <<, then settles
    // under gravity without the joints blowing up.
    if (ok) {
        char dir[1200];
        snprintf(dir, sizeof dir, "%s/toys_toybox_toy", assets);
        ok = rbh_spawn_toy("U6Bluebear", dir, 6.0, 5.0);
    }
    if (ok) {
        ok = rbh_eval(
            "t = $default_engine.toys.by_index(1)\n"
            "raise 'bear missing' unless t and t.toy_id == 'U6Bluebear'\n"
            "raise 'limbs' unless t.limbs.count == 6\n"
            "raise 'joints' unless t.joints.count == 11\n"
            "head = t.limbs.by_sid(:Head)\n"
            "raise 'no Head limb' unless head\n"
            "y0 = head.position.y\n"
            "$default_engine.run_steps(300)\n"
            "y1 = head.position.y\n"
            "STDERR.puts format('rubyboot: bear head %.3f -> %.3f after 3s', y0, y1)\n"
            "raise 'did not fall' unless y1 < y0\n"
            "t.limbs.each do |l|\n"
            "  p = l.position\n"
            "  raise 'limb escaped' unless p.x.finite? and p.y.finite?\n"
            "  raise 'below floor' if p.y < -1.0\n"
            "end\n",
            "bluebear verification");
    }

    printf(ok ? "rubyboot: OK\n" : "rubyboot: FAILED\n");
    rbh_shutdown();
    return ok ? 0 : 1;
}
