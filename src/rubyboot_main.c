// Headless Ruby boot check: framework load through World instantiation,
// no window. Exits 0 iff the whole boot + wall-position round trip works.
#include "rubyhost.h"
#include "assets.h"
#include "audio.h"
#include "physics.h"
#include "toydefs.h"
#include "toybox_button.h"
#include "toybox_scroll.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fake_sprite_next;
static int fake_sprite_limit;
static int fake_sprite_created;
static int fake_sprite_removed;

static int fake_sprite_add(const char* image, int body, float com_x,
                           float com_y, int group, void* user) {
    (void)image; (void)body; (void)com_x; (void)com_y; (void)group; (void)user;
    if (fake_sprite_created >= fake_sprite_limit) return -1;
    fake_sprite_created++;
    return fake_sprite_next++;
}

static void fake_sprite_remove(int sprite, void* user) {
    (void)sprite; (void)user;
    fake_sprite_removed++;
}

int main(int argc, char** argv) {
    const char* assets = argc > 1 ? argv[1] : "private/extracted";
    char path[1024];

    // Original IconGrid uniform-mode trajectory: mass=1, anchor k=300/c=25,
    // air resistance=0.099, integrated by the engine's fixed 0.01s RK4.
    // These checkpoints are the closed-form response to an 80px target step,
    // independent of the implementation in toybox_scroll.c.
    toybox_scroll_model scroll = {0};
    toybox_scroll_model_set_target(&scroll, 80.0f);
    toybox_scroll_model_advance(&scroll, 100.0);
    const float scroll_100ms = scroll.position;
    toybox_scroll_model_advance(&scroll, 100.0);
    const float scroll_200ms = scroll.position;
    toybox_scroll_model_advance(&scroll, 60.0);
    const float scroll_260ms = scroll.position;
    if (fabsf(scroll_100ms - 49.31147f) > 0.02f ||
        fabsf(scroll_200ms - 80.06019f) > 0.02f ||
        fabsf(scroll_260ms - 82.93851f) > 0.02f) {
        fprintf(stderr, "rubyboot: Toybox scroll response wrong %.5f %.5f %.5f\n",
                scroll_100ms, scroll_200ms, scroll_260ms);
        return 1;
    }
    fprintf(stderr, "rubyboot: Toybox scroll %.2f -> %.2f -> %.2f px\n",
            scroll_100ms, scroll_200ms, scroll_260ms);

    // ScrollbarGUIComponent maps captured thumb displacement over the usable
    // track directly into the normalized scroll model, clamped at both ends.
    toybox_scroll_model drag = {.target = 200.0f};
    toybox_scroll_model_drag(&drag, 200.0f, 50.0f, 200.0f, 1000.0f);
    if (fabsf(drag.target - 450.0f) > 0.001f) {
        fprintf(stderr, "rubyboot: Toybox thumb drag mapped to %.3f\n",
                drag.target);
        return 1;
    }
    toybox_scroll_model_drag(&drag, 200.0f, -100.0f, 200.0f, 1000.0f);
    if (drag.target != 0.0f) {
        fprintf(stderr, "rubyboot: Toybox thumb did not clamp at top\n");
        return 1;
    }
    toybox_scroll_model_drag(&drag, 200.0f, 1000.0f, 200.0f, 1000.0f);
    if (drag.target != 1000.0f) {
        fprintf(stderr, "rubyboot: Toybox thumb did not clamp at bottom\n");
        return 1;
    }
    fprintf(stderr, "rubyboot: Toybox thumb drag 200 -> 450 px, clamps 0..1000\n");

    // ButtonGUIComponent uses all six frames as one linear press track. A
    // quick momentary click must finish the forward leg, return to frame zero,
    // and only then dispatch; toggles travel once to their opposite endpoint.
    toybox_button_model momentary;
    toybox_button_init(&momentary, 0.14f, false, false);
    toybox_button_down(&momentary, 10.0f, 10.0f);
    bool button_command = toybox_button_advance(&momentary, 35.0, NULL);
    if (button_command || toybox_button_frame(&momentary, 6) != 1
        || !toybox_button_up(&momentary, 10.0f, 10.0f)
        || toybox_button_advance(&momentary, 110.0, NULL)
        || toybox_button_frame(&momentary, 6) != 5
        || !toybox_button_advance(&momentary, 145.0, NULL)
        || toybox_button_frame(&momentary, 6) != 0) {
        fprintf(stderr, "rubyboot: Toybox momentary button trajectory wrong\n");
        return 1;
    }

    toybox_button_model toggle;
    bool desired_on = false;
    toybox_button_init(&toggle, 0.14f, true, false);
    toybox_button_down(&toggle, 20.0f, 20.0f);
    toybox_button_advance(&toggle, 16.0, NULL); // settle at its current endpoint
    if (!toybox_button_up(&toggle, 20.0f, 20.0f)
        || toybox_button_advance(&toggle, 70.0, &desired_on)
        || !toybox_button_advance(&toggle, 75.0, &desired_on)
        || !desired_on || toybox_button_frame(&toggle, 6) != 5) {
        fprintf(stderr, "rubyboot: Toybox toggle button trajectory wrong\n");
        return 1;
    }
    toybox_button_sync(&toggle, true);

    toybox_button_model cancelled;
    toybox_button_init(&cancelled, 0.14f, false, false);
    toybox_button_down(&cancelled, 0.0f, 0.0f);
    toybox_button_advance(&cancelled, 35.0, NULL);
    if (toybox_button_up(&cancelled, 2.0f, 0.0f)
        || toybox_button_advance(&cancelled, 40.0, NULL)
        || toybox_button_frame(&cancelled, 6) != 0) {
        fprintf(stderr, "rubyboot: Toybox button click slop wrong\n");
        return 1;
    }
    fprintf(stderr,
            "rubyboot: Toybox buttons 140ms linear forward/reverse, 1.5px slop\n");

    if (!audio_init(true)) {
        fprintf(stderr, "rubyboot: cannot initialize headless audio\n");
        return 1;
    }
    if (!audio_set_muted(true) || !audio_muted()
        || !audio_set_muted(false) || audio_muted()) {
        fprintf(stderr, "rubyboot: audio mute toggle failed\n");
        audio_shutdown();
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
    if (toydefs_pack_count() < 7 || toydefs_icon_count() < 80) {
        fprintf(stderr, "rubyboot: Toybox catalog missing (%d packs, %d icons)\n",
                toydefs_pack_count(), toydefs_icon_count());
        audio_shutdown();
        return 1;
    }
    fprintf(stderr, "rubyboot: Toybox catalog %d packs, %d manifest icons\n",
            toydefs_pack_count(), toydefs_icon_count());
    int sports_icons = 0;
    bool essbee_in_astrobots = false;
    for (int i = 0; i < toydefs_icon_count(); i++) {
        const toyicon_t* icon = toydefs_icon_at(i);
        if (!icon) {
            continue;
        }
        if (icon->pack_order > 0.99f && icon->pack_order < 1.01f) {
            sports_icons++;
        }
        if (strcmp(icon->class_name, "REssBee") == 0 &&
            icon->pack_order > 6.99f && icon->pack_order < 7.01f) {
            essbee_in_astrobots = true;
        }
    }
    if (sports_icons != 10 || !essbee_in_astrobots) {
        fprintf(stderr,
                "rubyboot: Toybox pack mapping wrong (Sports=%d, Essbee=%s)\n",
                sports_icons, essbee_in_astrobots ? "Astrobots" : "other");
        audio_shutdown();
        return 1;
    }
    int decoded_icons = 0;
    int decoded_icon_frames = 0;
    for (int i = 0; i < toydefs_icon_count(); i++) {
        const toyicon_t* icon = toydefs_icon_at(i);
        if (!icon || icon->pack_order < 1.0f || icon->pack_order > 8.0f) {
            continue;
        }
        const int frames = icon->num_frames < 6 ? icon->num_frames : 6;
        for (int frame = 0; frame < frames; frame++) {
            snprintf(path, sizeof path, "%s/%s%04d.tga", assets, icon->image,
                     frame);
            as_image image = {0};
            if (!as_load_tga(path, &image)) {
                fprintf(stderr, "rubyboot: cannot decode Toybox icon %s\n", path);
                audio_shutdown();
                return 1;
            }
            decoded_icon_frames++;
            as_image_free(&image);
        }
        decoded_icons++;
    }
    fprintf(stderr, "rubyboot: decoded %d visible Toybox icons (%d frames)\n",
            decoded_icons, decoded_icon_frames);

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

    // Fixed physics degrees of freedom are still user-editable through the
    // original shape handles. The centre handle translates this fully fixed
    // ramp; an end handle rotates it while keeping its position anchored.
    if (ok) {
        char dir[1200];
        snprintf(dir, sizeof dir, "%s/toys_data_toy", assets);
        ok = rbh_spawn_toy("SCTurnableRamp", dir, 7.0, 4.0);
    }
    if (ok) {
        ok = rbh_eval(
            "ramp = $default_engine.toys.find {|t| t.toy_id == 'SCTurnableRamp'}\n"
            "raise 'turnable ramp missing' unless ramp\n"
            "limb = ramp.limbs.by_index(0)\n"
            "input = $default_engine.input_by_id(:default)\n"
            "p0 = limb.position\n"
            "$default_engine.input_grab(limb, input, p0)\n"
            "$default_engine.input_move(input, p0 + Vector[1.0, 1.0])\n"
            "$default_engine.run_steps(300)\n"
            "move_error = (limb.position - (p0 + Vector[1.0, 1.0])).r\n"
            "raise 'fixedMove handle did not translate' unless move_error < 0.3\n"
            "$default_engine.input_release(limb, input, limb.position)\n"
            "moved = limb.position\n"
            "$default_engine.run_steps(30)\n"
            "raise 'fixedMove did not relock' unless (limb.position - moved).r < 0.01\n"
            "theta0 = limb.orientation\n"
            "handle = limb.position + Vector[0.72, 0.0]\n"
            "$default_engine.input_grab(limb, input, handle)\n"
            "$default_engine.input_move(input, handle + Vector[0.0, 0.72])\n"
            "$default_engine.run_steps(150)\n"
            "raise 'grabRotate handle did not rotate' unless (limb.orientation - theta0).abs > 0.1\n"
            "raise 'rotate handle translated fixed limb' unless (limb.position - moved).r < 0.05\n"
            "$default_engine.input_release(limb, input, limb.position)\n"
            "$default_engine.toys.remove(ramp)\n",
            "fixed grab handle verification");
    }

    // The reported real-world case: the cannon base uses the limb default
    // (move only), while the small barrel-tip circle overrides it to rotate
    // only. Both must work even though fixedMove keeps the cannon anchored
    // against ordinary physics.
    if (ok) {
        char dir[1200];
        snprintf(dir, sizeof dir, "%s/christmas07_toy", assets);
        ok = rbh_spawn_toy("SnowballCannon", dir, 8.0, 3.0);
    }
    if (ok) {
        ok = rbh_eval(
            "cannon = $default_engine.toys.find {|t| t.toy_id == 'SnowballCannon'}\n"
            "raise 'snowball cannon missing' unless cannon\n"
            "limb = cannon.limbs.by_index(0)\n"
            "input = $default_engine.input_by_id(:default)\n"
            "p0 = limb.position\n"
            "$default_engine.input_grab(limb, input, p0)\n"
            "$default_engine.input_move(input, p0 + Vector[0.8, 0.5])\n"
            "$default_engine.run_steps(250)\n"
            "raise 'cannon base did not move' unless (limb.position - p0).r > 0.5\n"
            "$default_engine.input_release(limb, input, limb.position)\n"
            "base = limb.position\n"
            "theta0 = limb.orientation\n"
            "tip = base + Vector[0.342, 0.0]\n"
            "$default_engine.input_grab(limb, input, tip)\n"
            "$default_engine.input_move(input, tip + Vector[0.0, 0.5])\n"
            "$default_engine.run_steps(120)\n"
            "raise 'cannon tip did not rotate' unless (limb.orientation - theta0).abs > 0.1\n"
            "raise 'cannon tip translated base' unless (limb.position - base).r < 0.05\n"
            "$default_engine.input_release(limb, input, limb.position)\n"
            "$default_engine.toys.remove(cannon)\n",
            "cannon grab handle verification");
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
            "raise 'egg still realized' unless egg.limbs.by_sid(:egg).engine.nil?\n"
            "legs = goose.limbs.by_sid(:gooselegs)\n"
            "input = $default_engine.input_by_id(:default)\n"
            "legs0 = legs.position\n"
            "$default_engine.input_grab(legs, input, legs0)\n"
            "$default_engine.input_move(input, legs0 + Vector[0.8, 0.5])\n"
            "$default_engine.run_steps(250)\n"
            "raise 'goose anchored legs did not move' unless (legs.position - legs0).r > 0.5\n"
            "$default_engine.input_release(legs, input, legs.position)\n",
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

    // Capacity regression: twenty six-limb bears exceed both former global
    // ceilings at once (64 bodies and 128 joints). No simulation step is
    // needed here; successful all-or-nothing realization is the contract.
    if (ok) {
        const int bodies_before = phys_active_body_count();
        ok = rbh_eval(
            "$capacity_bears = []\n"
            "20.times do |i|\n"
            "  t = U6Bluebear.new\n"
            "  t.move(Vector[2.0 + (i % 10) * 0.1, 5.0 + (i / 10) * 0.1])\n"
            "  $default_engine.toys << t\n"
            "  $capacity_bears << t\n"
            "end\n",
            "dynamic physics capacity verification");
        const int added = phys_active_body_count() - bodies_before;
        if (ok && added != 120) {
            fprintf(stderr, "rubyboot: dynamic body growth added %d, expected 120\n",
                    added);
            ok = false;
        }
        if (ok) {
            ok = rbh_eval(
                "$capacity_bears.each {|t| $default_engine.toys.remove(t)}\n"
                "$capacity_bears = nil\n",
                "dynamic physics capacity teardown");
        }
        if (ok && phys_active_body_count() != bodies_before) {
            fprintf(stderr, "rubyboot: capacity teardown leaked bodies\n");
            ok = false;
        }
        if (ok) {
            fprintf(stderr,
                    "rubyboot: dynamic capacity 120 bodies / 220 joints\n");
        }
    }

    // A picked scene sprite resolves through its node parents to the whole
    // owning Toy. Dropping it into the Toybox removes every native resource;
    // sticky toys use the same marker as the original clear/recycle guards.
    if (ok) {
        const int bodies_before = phys_active_body_count();
        bool recycle_ok = true;
        fake_sprite_next = 1200;
        fake_sprite_limit = 32;
        fake_sprite_created = fake_sprite_removed = 0;
        rbh_set_sprite_hook(fake_sprite_add, fake_sprite_remove, NULL);
        char dir[1200];
        snprintf(dir, sizeof dir, "%s/toys_toybox_toy", assets);

        // Pick the last sprite of a six-limb bear, not its first/root visual:
        // every visual must still resolve to and tear down the complete toy.
        if (!rbh_spawn_toy("U6Bluebear", dir, 5.0, 5.0)
            || fake_sprite_created < 2
            || !rbh_recycle_sprite(fake_sprite_next - 1)) {
            fprintf(stderr, "rubyboot: ordinary toy was not recyclable\n");
            recycle_ok = false;
        }

        const int sticky_sprite = fake_sprite_next;
        if (!rbh_spawn_toy("U1Bouncy", dir, 5.0, 5.0)
            || !rbh_eval(
                "t = $default_engine.toys.find {|toy| toy.toy_id == 'U1Bouncy'}\n"
                "raise 'recycle test ball missing' unless t\n"
                "t.is_sticky = true\n",
                "sticky recycle setup")
            || rbh_recycle_sprite(sticky_sprite)) {
            fprintf(stderr, "rubyboot: sticky toy was recyclable\n");
            recycle_ok = false;
        }

        const bool cleanup_ok = rbh_eval(
            "$default_engine.toys.select {|t| t.toy_id == 'U1Bouncy'}.each do |t|\n"
            "  $default_engine.toys.remove(t)\n"
            "end\n"
            "bears = $default_engine.toys.select {|t| t.toy_id == 'U6Bluebear'}\n"
            "$default_engine.toys.remove(bears.pop) while bears.size > 1\n",
            "recycle verification cleanup");
        rbh_set_sprite_hook(NULL, NULL, NULL);
        if (!cleanup_ok || phys_active_body_count() != bodies_before
            || fake_sprite_created == 0
            || fake_sprite_removed != fake_sprite_created) {
            fprintf(stderr,
                    "rubyboot: recycle teardown bodies=%d sprites=%d/%d\n",
                    phys_active_body_count() - bodies_before,
                    fake_sprite_removed, fake_sprite_created);
            recycle_ok = false;
        }
        if (!recycle_ok) {
            ok = false;
        } else {
            fprintf(stderr, "rubyboot: sprite-to-toy recycle + sticky guard\n");
        }
    }

    // A visual allocation failure after a prefix of a complex toy must make
    // rbh_spawn_toy fail and tear the prefix back down. Starting above the
    // old 1024-id map limit also exercises dynamic scene-id bookkeeping.
    if (ok) {
        const int bodies_before = phys_active_body_count();
        fake_sprite_next = 1500;
        fake_sprite_limit = 2;
        fake_sprite_created = fake_sprite_removed = 0;
        rbh_set_sprite_hook(fake_sprite_add, fake_sprite_remove, NULL);
        char dir[1200];
        snprintf(dir, sizeof dir, "%s/toys_toybox_toy", assets);
        if (rbh_spawn_toy("U6Bluebear", dir, 6.0, 5.0)) {
            fprintf(stderr, "rubyboot: partial visual realization succeeded\n");
            ok = false;
        }
        rbh_set_sprite_hook(NULL, NULL, NULL);
        if (phys_active_body_count() != bodies_before
            || fake_sprite_created != 2 || fake_sprite_removed != 2) {
            fprintf(stderr,
                    "rubyboot: realization rollback bodies=%d sprites=%d/%d\n",
                    phys_active_body_count() - bodies_before,
                    fake_sprite_removed, fake_sprite_created);
            ok = false;
        }
        if (ok) {
            ok = rbh_eval(
                "bears = $default_engine.toys.select {|t| t.toy_id == 'U6Bluebear'}\n"
                "raise 'failed bear remained in engine' unless bears.size == 1\n",
                "transactional realization verification");
        }
        if (ok) {
            fprintf(stderr, "rubyboot: partial realization rolled back\n");
        }
    }

    // The Toybox clear command removes complete non-sticky toys through the
    // Ruby container teardown path while preserving the sticky World.
    if (ok && !rbh_clear_scene()) {
        fprintf(stderr, "rubyboot: Toybox clear command failed\n");
        ok = false;
    }
    if (ok) {
        ok = rbh_eval(
            "toys = $default_engine.toys.to_a\n"
            "raise 'clear left ordinary toys' unless toys.all? {|t| t.is_sticky?}\n"
            "raise 'clear removed world' unless toys.find {|t| t.sid == :world}\n"
            "raise 'clear scale' unless ($default_engine.scale - 100.0).abs < 1e-6\n",
            "Toybox clear verification");
    }
    if (ok) {
        fprintf(stderr, "rubyboot: Toybox clear preserved sticky World\n");
    }

    printf(ok ? "rubyboot: OK\n" : "rubyboot: FAILED\n");
    rbh_shutdown();
    audio_shutdown();
    return ok ? 0 : 1;
}
