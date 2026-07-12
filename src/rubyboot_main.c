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

    // Milestone: World toy exists per engine and the walls follow the screen
    // through the whole framework chain: screen_size_changed ->
    // fit_canvas_to_screen/fit_scene_to_canvas -> scene_walls_changed ->
    // World#walls_changed -> Limb#position=.
    if (ok) {
        rbh_screen_size(1280.0, 800.0);
        ok = rbh_eval(
            "w = $default_engine.toys.by_sid(:world)\n"
            "raise 'world toy missing' unless w\n"
            "raise 'not a World' unless w.is_a?(World)\n"
            "raise 'not sticky' unless w.is_sticky?\n"
            "names = [:left_wall, :right_wall, :floor, :ceiling]\n"
            "ls = names.map {|s| w.limbs.by_sid(s) or raise \"no #{s}\"}\n"
            "STDERR.puts 'rubyboot: world=' + w.toy_instance_id.to_s +\n"
            "  ' walls=' + ls.map {|l| format('%g,%g', l.position.x, l.position.y)}.join(' ')\n"
            "raise 'right wall' unless (ls[1].position.x - 12.8).abs < 1e-6\n"
            "raise 'ceiling' unless (ls[3].position.y - 8.0).abs < 1e-6\n"
            "raise 'scale not refit' unless ($default_engine.scale - 100.0).abs < 1e-6\n",
            "world verification");
    }

    // Physics timers: scheduled ticks delivered through dispatch_timers,
    // registered via the framework's SoupNode#on_timer.
    if (ok) {
        ok = rbh_eval(
            "$fired = []\n"
            "w = $default_engine.toys.by_sid(:world)\n"
            "w.on_timer(lambda {|t| $fired << t}, 50)\n"
            "$default_engine.run_steps(100)\n"
            "$default_engine.dispatch_timers(100)\n"
            "raise \"timer fired #{$fired.inspect}\" unless $fired == [50, 100]\n",
            "timer verification");
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

    // Input grab: the framework-facing mouse spring. Grab the bear's head,
    // drag the target across the scene, and the limb must follow.
    if (ok) {
        ok = rbh_eval(
            "t = $default_engine.toys.by_index(1)\n"
            "head = t.limbs.by_sid(:Head)\n"
            "input = $default_engine.input_by_id(:default)\n"
            "$default_engine.input_grab(head, input, head.position)\n"
            "raise 'input.limb' unless input.limb == head\n"
            "$default_engine.input_move(input, Vector[6.0, 6.0])\n"
            "$default_engine.run_steps(300)\n"
            "d = (head.position - Vector[6.0, 6.0]).r\n"
            "STDERR.puts format('rubyboot: grabbed head at %.2f,%.2f (%.2f from target)',\n"
            "  head.position.x, head.position.y, d)\n"
            "raise 'grab did not pull' unless d < 1.0\n"
            "$default_engine.input_release(head, input, head.position)\n"
            "raise 'release' unless input.limb.nil?\n",
            "input grab verification");
    }

    // First scripted toy: the goose. Spin its body past the lay angle and
    // the script must shoot a GooseEgg (new toy realized mid-simulation);
    // shrink the egg's lifetime and it must remove itself (full teardown).
    if (ok) {
        char dir[1200];
        snprintf(dir, sizeof dir, "%s/toys_toybox_toy", assets);
        ok = rbh_load_toy_class("GooseEgg", dir)
          && rbh_spawn_toy("Goose", dir, 9.0, 3.0);
    }
    if (ok) {
        ok = rbh_eval(
            "goose = $default_engine.toys.find {|t| t.toy_id == 'Goose'}\n"
            "raise 'goose missing' unless goose\n"
            "raise 'goose class' unless goose.is_a?(Goose)\n"
            "raise 'lay sound' unless goose.sounds.by_sid(:lay)\n"
            "before = $default_engine.toys.count\n"
            "body = goose.limbs.by_sid(:goosebody)\n"
            "20.times do\n"  // rock the body across the lay angle
            "  body.orientation = 0.0\n"
            "  $default_engine.run_steps(1); $default_engine.dispatch_timers(1)\n"
            "  body.orientation = -Math::PI / 2.0\n"
            "  $default_engine.run_steps(1); $default_engine.dispatch_timers(1)\n"
            "  break if $default_engine.toys.count > before\n"
            "end\n"
            "raise 'no egg laid' unless $default_engine.toys.count == before + 1\n"
            "egg = $default_engine.toys.by_index($default_engine.toys.count - 1)\n"
            "raise 'egg class' unless egg.is_a?(GooseEgg)\n"
            "m = egg.limbs.by_sid(:egg).momentum\n"
            "STDERR.puts format('rubyboot: egg laid, momentum %.2f,%.2f', m.x, m.y)\n"
            "raise 'egg momentum' unless m.x < -5.0\n"
            "egg.lifetime = 3\n"
            "$default_engine.dispatch_timers(5)\n"
            "raise 'egg not removed' unless $default_engine.toys.count == before\n"
            "raise 'egg still realized' unless egg.limbs.by_sid(:egg).engine.nil?\n",
            "goose verification");
    }

    // Rotational grab: the goose body is move=0/rotate=1 and hangs on a
    // rotational joint - pulling the grab anchor sideways must TWIST it
    // past the lay angle, which lays an egg through the timer script.
    if (ok) {
        ok = rbh_eval(
            "goose = $default_engine.toys.find {|t| t.toy_id == 'Goose'}\n"
            "body = goose.limbs.by_sid(:goosebody)\n"
            "80.times { $default_engine.run_steps(10); $default_engine.dispatch_timers(10) }\n"
            "raise format('goose not upright (%.2f)', body.orientation) unless body.orientation.abs < 0.3\n"
            "before = $default_engine.toys.count\n"
            "input = $default_engine.input_by_id(:default)\n"
            "$default_engine.input_grab(body, input, body.position + Vector[0.0, 0.3])\n"
            "$default_engine.input_move(input, body.position + Vector[0.6, -0.1])\n"
            "30.times { $default_engine.run_steps(10); $default_engine.dispatch_timers(10) }\n"
            "STDERR.puts format('rubyboot: goose twisted to %.2f rad', body.orientation)\n"
            "raise 'goose did not rotate' unless body.orientation < -0.6\n"
            "raise 'no egg from rocking' unless $default_engine.toys.count > before\n"
            "$default_engine.input_release(body, input, body.position)\n",
            "rotational grab verification");
    }

    printf(ok ? "rubyboot: OK\n" : "rubyboot: FAILED\n");
    rbh_shutdown();
    return ok ? 0 : 1;
}
