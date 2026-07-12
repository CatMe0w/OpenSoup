// Headless Ruby boot check: framework load through World instantiation,
// no window. Exits 0 iff the whole boot + wall-position round trip works.
#include "rubyhost.h"
#include "audio.h"
#include "toydefs.h"
#include <stdio.h>

int main(int argc, char** argv) {
    const char* assets = argc > 1 ? argv[1] : "private/extracted";
    char path[1024];

    if (!audio_init(true)) {
        fprintf(stderr, "rubyboot: cannot initialize headless audio\n");
        return 1;
    }

    // Static Ogg decode + the original mixer's fixed 32-voice capacity.
    snprintf(path, sizeof path,
             "%s/toys_toybox_toy/sound/c06 goose/goosehonk.ogg", assets);
    const int sample = audio_sample_load(path);
    int channels = 0, sample_rate = 0;
    uint64_t frames = 0;
    bool audio_ok = audio_sample_info(sample, &channels, &sample_rate, &frames)
                 && channels > 0 && sample_rate > 0 && frames > 0;
    for (int i = 0; audio_ok && i < AUDIO_MAX_VOICES; i++) {
        audio_ok = audio_play(sample, 1, 0.0f, false);
    }
    if (audio_ok && audio_play(sample, 1, 0.0f, false)) {
        audio_ok = false;
    }
    audio_stop_owner(1);
    if (!audio_ok || audio_active_voices() != 0) {
        fprintf(stderr, "rubyboot: audio decode/voice-pool verification failed\n");
        audio_shutdown();
        return 1;
    }
    fprintf(stderr, "rubyboot: audio %dch %dHz, %llu frames, %d voices\n",
            channels, sample_rate, (unsigned long long)frames,
            AUDIO_MAX_VOICES);

    snprintf(path, sizeof path, "%s/toydefs.json", assets);
    if (!toydefs_load(path)) {
        fprintf(stderr, "rubyboot: cannot load %s\n", path);
        audio_shutdown();
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

    // Shape-level narrowphase drives both physical toy contacts and sensor
    // events. Two balls begin overlapped: triggers_overlapping must see the
    // other Shape, enter/exit must be edge-triggered, and the penalty contact
    // must push the bodies apart.
    if (ok) {
        char dir[1200];
        snprintf(dir, sizeof dir, "%s/toys_toybox_toy", assets);
        ok = rbh_load_toy_class("U1Bouncy", dir)
          && rbh_spawn_toy("U1Bouncy", dir, 4.0, 4.0)
          && rbh_spawn_toy("U1Bouncy", dir, 4.15, 4.0);
    }
    if (ok) {
        ok = rbh_eval(
            "balls = $default_engine.toys.select {|t| t.toy_id == 'U1Bouncy'}\n"
            "raise 'two balls missing' unless balls.size == 2\n"
            "a, b = balls\n"
            "sa = a.limbs.first.shapes.first\n"
            "sb = b.limbs.first.shapes.first\n"
            "raise 'member_of' unless sa.member_of.include?(:bouncers)\n"
            "raise 'overlap query' unless sa.triggers_overlapping(:bouncers).include?(sb)\n"
            "$trigger_enters = $trigger_exits = 0\n"
            "sa.trigger_on = :bouncers\n"
            "sa.on_trigger_enter(:bouncers, lambda {|e| $trigger_enters += 1 if e.shape2 == sb})\n"
            "sa.on_trigger_exit(:bouncers, lambda {|e| $trigger_exits += 1 if e.shape2 == sb})\n"
            "$default_engine.run_steps(1)\n"
            "raise 'trigger enter' unless $trigger_enters == 1\n"
            "b.limbs.first.position = Vector[5.0, 4.0]\n"
            "$default_engine.run_steps(1)\n"
            "raise 'trigger exit' unless $trigger_exits == 1\n"
            "la, lb = a.limbs.first, b.limbs.first\n"
            "la.position = Vector[4.0, 4.0]; lb.position = Vector[4.15, 4.0]\n"
            "la.momentum = Vector[0.0, 0.0]; lb.momentum = Vector[0.0, 0.0]\n"
            "d0 = (lb.position.x - la.position.x).abs\n"
            "$default_engine.run_steps(5)\n"
            "d1 = (lb.position.x - la.position.x).abs\n"
            "STDERR.puts format('rubyboot: toy contact separation %.3f -> %.3f', d0, d1)\n"
            "raise 'toy contact did not separate' unless d1 > d0 + 0.01\n"
            "balls.each {|t| $default_engine.toys.remove(t)}\n",
            "toy collision + trigger verification");
    }

    // Real shipped script integration: SnowballLarge watches :floor and must
    // replace itself with four SnowballSmall instances on first overlap.
    if (ok) {
        char dir[1200];
        snprintf(dir, sizeof dir, "%s/christmas07_toy", assets);
        ok = rbh_load_toy_class("SnowballSmall", dir)
          && rbh_load_toy_class("SnowballLarge", dir);
        if (ok) ok = rbh_spawn_toy("SnowballLarge", dir, 3.0, 0.03);
    }
    if (ok) {
        ok = rbh_eval(
            "large = $default_engine.toys.find {|t| t.toy_id == 'SnowballLarge'}\n"
            "raise 'large snowball missing' unless large\n"
            "before = $default_engine.toys.count\n"
            "$default_engine.run_steps(1)\n"
            "smalls = $default_engine.toys.select {|t| t.toy_id == 'SnowballSmall'}\n"
            "raise 'large snowball did not shatter' if $default_engine.toys.include?(large)\n"
            "raise \"small snowballs #{smalls.size}\" unless smalls.size == 4\n"
            "raise 'snowball count' unless $default_engine.toys.count == before + 3\n"
            "smalls.each {|t| $default_engine.toys.remove(t)}\n",
            "snowball trigger verification");
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

    // Ruby Sound bridge: def path, period throttling, looping, and owner stop.
    if (ok) {
        ok = rbh_eval(
            "goose = $default_engine.toys.find {|t| t.toy_id == 'Goose'}\n"
            "lay = goose.sounds.by_sid(:lay); pop = goose.sounds.by_sid(:pop)\n"
            "raise 'sound nodes' unless lay && pop\n"
            "raise 'sound path' unless lay.path == '../sound/c06 goose/goosehonk.ogg'\n"
            "lay.stop; pop.stop\n"
            "lay.period_length = 1.0; lay.max_sounds_per_period = 2\n"
            "raise 'period getter' unless lay.period_length == 1.0\n"
            "raise 'max getter' unless lay.max_sounds_per_period == 2\n"
            "3.times { lay.play(0.0) }\n",
            "sound throttle verification");
    }
    if (ok && audio_active_voices() != 2) {
        fprintf(stderr, "rubyboot: Sound period limit produced %d voices\n",
                audio_active_voices());
        ok = false;
    }
    if (ok) {
        ok = rbh_eval(
            "lay.period_length = 0; lay.play(0.0)\n"
            "lay.stop; lay.loop(0.0)\n",
            "sound loop verification");
    }
    if (ok && (audio_active_voices() != 1 ||
               !audio_render_frames(frames * 2 + (uint64_t)sample_rate) ||
               audio_active_voices() != 1)) {
        fprintf(stderr, "rubyboot: looping Sound did not remain active\n");
        ok = false;
    }
    if (ok) {
        ok = rbh_eval("lay.stop", "sound stop verification");
    }
    if (ok && audio_active_voices() != 0) {
        fprintf(stderr, "rubyboot: Sound#stop left active voices\n");
        ok = false;
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
    if (ok) {
        ok = rbh_eval(
            "goose.sounds.each {|sound| sound.stop }\n"
            "goose.sounds.by_sid(:lay).loop(0.0)\n",
            "sound teardown setup");
    }
    if (ok && audio_active_voices() != 1) {
        fprintf(stderr, "rubyboot: teardown loop setup failed\n");
        ok = false;
    }
    if (ok) {
        ok = rbh_eval("$default_engine.toys.remove(goose)",
                      "sound teardown verification");
    }
    if (ok && audio_active_voices() != 0) {
        fprintf(stderr, "rubyboot: toy teardown left active Sound voices\n");
        ok = false;
    }

    // Shipped polygon sensor integration: a descending Basketball crossing
    // SCHoop's sensor-only trigger_top rectangle increments the score on exit.
    if (ok) {
        char dir[1200];
        snprintf(dir, sizeof dir, "%s/toys_data_toy", assets);
        ok = rbh_load_toy_class("Basketball", dir)
          && rbh_load_toy_class("SCHoop", dir)
          && rbh_spawn_toy("SCHoop", dir, 6.0, 2.0);
        if (ok) {
            ok = rbh_eval("$default_engine.run_steps(300)",
                          "hoop settle verification");
        }
        if (ok) ok = rbh_spawn_toy("Basketball", dir, 6.0, 2.26);
    }
    if (ok) {
        ok = rbh_eval(
            "hoop = $default_engine.toys.find {|t| t.toy_id == 'SCHoop'}\n"
            "ball = $default_engine.toys.find {|t| t.toy_id == 'Basketball'}\n"
            "raise 'hoop setup' unless hoop && ball\n"
            "bl = ball.limbs.first\n"
            "bl.momentum = Vector[0.0, -1.0]\n"
            "$default_engine.run_steps(1)\n"
            "bl.position = Vector[6.0, 1.0]\n"
            "bl.momentum = Vector[0.0, -1.0]\n"
            "$default_engine.run_steps(1)\n"
            "score = hoop.instance_variable_get(:@score)\n"
            "STDERR.puts 'rubyboot: hoop score=' + score.to_s\n"
            "raise 'hoop did not score' unless score == 2\n"
            "$default_engine.toys.remove(ball)\n"
            "$default_engine.toys.remove(hoop)\n",
            "hoop trigger verification");
    }

    printf(ok ? "rubyboot: OK\n" : "rubyboot: FAILED\n");
    rbh_shutdown();
    audio_shutdown();
    return ok ? 0 : 1;
}
