// Embedded Ruby 1.8.6 host, the script side of the engine.
// Class surface comes from the reverse-engineered Toybox binding surface:
// every method is first bound to a logging stub so framework load-time aliases
// (`alias :internal_render :render` etc.) always resolve; the subset the boot
// path needs is then rebound to real implementations backed by toydefs/physics.
//
// The implementations live in the rubyhost_*.c files (node model, realization,
// engine, limb/shape, sound); this file owns the class registry, the stub API
// surface, the Souptoys resource host, and boot.
#include "rubyhost.h"
#include "assets_layout.h"
#include "physics.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rubyhost_internal.h"

void Init_ext(void); // ext/extinit.o: static stringio + syck

// registry

#define MAX_CLASSES 40
static struct { const char* cname; VALUE cls; } g_reg[MAX_CLASSES];
static int g_nreg;
char g_assets[1024]; // the assets tree root (per-container defs + resources)
char g_root[1024];   // souptoys_core_toy resource root (framework scripts)

VALUE cls_find(const char* name) {
    for (int i = 0; i < g_nreg; i++) {
        if (strcmp(g_reg[i].cname, name) == 0) {
            return g_reg[i].cls;
        }
    }
    rb_raise(rb_eRuntimeError, "rubyhost: unknown engine class %s", name);
}

// stubs

static int g_stub_log_budget = 200;

static VALUE rba_stub(int argc, VALUE* argv, VALUE self) {
    (void)argc;
    (void)argv;
    if (g_stub_log_budget > 0) {
        g_stub_log_budget--;
        fprintf(stderr, "[rubyhost] stub %s#%s\n", rb_obj_classname(self),
                rb_id2name(rb_frame_last_func()));
    }
    return Qnil;
}

// Souptoys

static bool resource_path(const char* key, char* out, size_t cap) {
    if (key[0] == '/' || strstr(key, "..")) {
        return false;
    }
    return (size_t)snprintf(out, cap, "%s/%s", g_root, key) < cap;
}

static VALUE soup_resource_exists(VALUE self, VALUE key) {
    (void)self;
    char p[1400];
    if (!resource_path(StringValueCStr(key), p, sizeof p)) {
        return Qfalse;
    }
    FILE* f = fopen(p, "rb");
    if (f) {
        fclose(f);
        return Qtrue;
    }
    return Qfalse;
}

static VALUE soup_resource_load(VALUE self, VALUE key) {
    (void)self;
    char p[1400];
    FILE* f = NULL;
    if (resource_path(StringValueCStr(key), p, sizeof p)) {
        f = fopen(p, "rb");
    }
    if (!f) {
        rb_raise(rb_eRuntimeError, "no such resource: %s",
                 StringValueCStr(key));
    }
    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    VALUE s = rb_str_new(NULL, len);
    if (fread(RSTRING(s)->ptr, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        rb_raise(rb_eRuntimeError, "short read: %s", StringValueCStr(key));
    }
    fclose(f);
    return s;
}

static VALUE soup_set_license_policy(VALUE self, VALUE v) {
    sn_get(self)->license_policy = v;
    return v;
}

static VALUE soup_get_license_policy(VALUE self) {
    return sn_get(self)->license_policy;
}

static VALUE soup_load_paths(VALUE self) {
    VALUE v = sn_get(self)->load_paths;
    return NIL_P(v) ? rb_ary_new() : v;
}

static VALUE soup_load_paths_set(VALUE self, VALUE v) {
    sn_get(self)->load_paths = v;
    return v;
}

static VALUE soup_exe_path(VALUE self) {
    (void)self;
    return rb_str_new2(g_root);
}

static VALUE soup_console_open_p(VALUE self) {
    (void)self;
    return Qtrue; // dev: keeps the framework's gated Object#puts audible
}

// registration

static VALUE lookup_super(const char* name) {
    if (strcmp(name, "Object") == 0) {
        return rb_cObject;
    }
    if (strcmp(name, "Exception") == 0) {
        return rb_eException;
    }
    return cls_find(name);
}

static void define_api(void) {
    // pass 1: classes (in .inc order, supers first) + default allocator
#define RBA_CLASS(name, super) \
    { \
        VALUE c = rb_define_class(#name, lookup_super(#super)); \
        if (strcmp(#super, "Exception") != 0) { \
            rb_define_alloc_func(c, alloc_generic); \
        } \
        g_reg[g_nreg].cname = #name; \
        g_reg[g_nreg].cls = c; \
        g_nreg++; \
    }
#define RBA_METHOD(cls, name, argc)
#define RBA_SMETHOD(cls, name, argc)
#define RBA_PMETHOD(cls, name, argc)
#include "ruby_api.inc"
#undef RBA_CLASS
#undef RBA_METHOD
#undef RBA_SMETHOD
#undef RBA_PMETHOD

    // pass 2: every API method as a variadic stub
#define RBA_CLASS(name, super)
#define RBA_METHOD(cls, name, argc) \
    rb_define_method(cls_find(#cls), name, rba_stub, -1);
#define RBA_SMETHOD(cls, name, argc) \
    rb_define_singleton_method(cls_find(#cls), name, rba_stub, -1);
#define RBA_PMETHOD(cls, name, argc) \
    rb_define_private_method(cls_find(#cls), name, rba_stub, -1);
#include "ruby_api.inc"
#undef RBA_CLASS
#undef RBA_METHOD
#undef RBA_SMETHOD
#undef RBA_PMETHOD
}

static void register_soup(void) {
    VALUE c = cls_find("Souptoys");
    rb_define_method(c, "resource_exists", soup_resource_exists, 1);
    rb_define_method(c, "resource_load", soup_resource_load, 1);
    rb_define_method(c, "set_license_policy", soup_set_license_policy, 1);
    rb_define_method(c, "get_license_policy", soup_get_license_policy, 0);
    rb_define_method(c, "load_paths", soup_load_paths, 0);
    rb_define_method(c, "load_paths=", soup_load_paths_set, 1);
    rb_define_method(c, "exe_path", soup_exe_path, 0);
    rb_define_method(c, "console_open?", soup_console_open_p, 0);
}

// boot

static void report_exception(const char* what) {
    VALUE err = rb_gv_get("$!");
    if (NIL_P(err)) {
        fprintf(stderr, "[rubyhost] %s failed (no $!)\n", what);
        return;
    }
    VALUE msg = rb_funcall(err, rb_intern("message"), 0);
    fprintf(stderr, "[rubyhost] %s failed: %s: %s\n", what,
            rb_obj_classname(err), StringValueCStr(msg));
    VALUE bt = rb_funcall(err, rb_intern("backtrace"), 0);
    if (TYPE(bt) == T_ARRAY) {
        const long n = RARRAY(bt)->len < 12 ? RARRAY(bt)->len : 12;
        for (long i = 0; i < n; i++) {
            VALUE line = rb_ary_entry(bt, i);
            fprintf(stderr, "    %s\n", StringValueCStr(line));
        }
    }
    ruby_errinfo = Qnil;
}

bool rbh_eval(const char* code, const char* what) {
    int state = 0;
    rb_eval_string_protect(code, &state);
    if (state == 0) {
        return true;
    }
    report_exception(what);
    return false;
}

// rb_funcall behind rb_protect: script exceptions (e.g. inside timer procs)
// must not longjmp through the C frame loop.
struct fcall {
    VALUE recv;
    ID id;
    int argc;
    VALUE argv[3];
};

static VALUE fcall_thunk(VALUE arg) {
    struct fcall* f = (struct fcall*)arg;
    return rb_funcall2(f->recv, f->id, f->argc, f->argv);
}

bool fcall_protected(VALUE recv, const char* name, int argc,
                     VALUE a0, VALUE a1, VALUE a2, const char* what) {
    struct fcall f = { recv, rb_intern(name), argc, { a0, a1, a2 } };
    int state = 0;
    rb_protect(fcall_thunk, (VALUE)&f, &state);
    if (state == 0) {
        return true;
    }
    report_exception(what);
    return false;
}

// Per-frame heartbeat: fixed 0.01s steps with an accumulator (clamped like
// the native loop was), driven through the FRAMEWORK's run_steps wrapper so
// engine_execute and pre-timer observers behave as in the original.
void rbh_frame(double dt_ms) {
    static double acc_ms;
    acc_ms += dt_ms;
    int steps = (int)(acc_ms / (PHYS_DT * 1000.0));
    if (steps <= 0) {
        return;
    }
    acc_ms -= steps * (PHYS_DT * 1000.0);
    if (steps > 25) {
        steps = 25; // clamp long stalls
    }
    VALUE eng = rb_gv_get("$default_engine");
    if (NIL_P(eng)) {
        return;
    }
    fcall_protected(eng, "run_steps", 1, INT2FIX(steps), Qnil, Qnil,
                    "run_steps");
    fcall_protected(eng, "dispatch_timers", 1, INT2FIX(steps), Qnil, Qnil,
                    "dispatch_timers");
}

static bool eval_resource(const char* key) {
    char p[1400];
    if (!resource_path(key, p, sizeof p)) {
        return false;
    }
    FILE* f = fopen(p, "rb");
    if (!f) {
        fprintf(stderr, "[rubyhost] missing resource %s\n", p);
        return false;
    }
    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc((size_t)len + 1);
    const bool ok = fread(buf, 1, (size_t)len, f) == (size_t)len;
    fclose(f);
    buf[len] = 0;
    const bool ran = ok && rbh_eval(buf, key);
    free(buf);
    return ran;
}

// The exact bootstrap snippet Toybox.exe evals to pull souptoys.rb out of the
// resource container (the proto version of the require hook the framework
// then installs for everything else).
static const char* BOOTSTRAP =
    "path = 'souptoys.rb';"
    "begin;"
    "   require path;"
    "rescue LoadError => load_error;"
    "   path << '.rb' if File.extname(path) == '';"
    "   if $engine.resource_exists path;"
    "       eval $engine.resource_load(path);"
    "   else;"
    "       raise load_error;"
    "   end;"
    "end;";

bool rbh_boot(const char* assets_root) {
    snprintf(g_assets, sizeof g_assets, "%s", assets_root);
    container_resource_root(g_root, sizeof g_root, assets_root,
                            "souptoys_core_toy");

    ruby_init();
    ruby_script("souptoys_embedded");
    // Ruby 1.8 traps INT/TERM and turns them into SignalExceptions, which
    // our rb_protect frames would swallow, and the app becomes unkillable.
    // The host owns process lifetime, not the interpreter.
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    Init_ext(); // static stringio + syck (yaml needs both)
    define_api();
    rbh_register_nodes();
    rbh_register_limb();
    rbh_register_engine();
    rbh_register_sound();
    register_soup();

    // globals, in the original's creation order
    VALUE core = rb_obj_alloc(cls_find("RCore"));
    rb_gv_set("$core", core);
    static const char* engs[] = { "$default_engine", "$ui_engine",
                                  "$grid_engine" };
    for (int i = 0; i < 3; i++) {
        VALUE e = rb_obj_alloc(cls_find("REngine"));
        rb_gv_set(engs[i], e);
        core_add_engine(core, e);
    }
    rb_gv_set("$engine", rb_obj_alloc(cls_find("Souptoys")));

    char lp[sizeof g_root + 32];
    snprintf(lp, sizeof lp, "$:.clear(); $: << '%s'", g_root);
    if (!rbh_eval(lp, "load path reset")) {
        return false;
    }

    int state = 0;
    rb_eval_string_protect("require 'matrix'", &state);
    if (state) {
        ruby_errinfo = Qnil;
        if (!eval_resource("e2mmap.rb") ||
            !rbh_eval("$\" << 'e2mmap.rb'", "provide e2mmap") ||
            !eval_resource("matrix.rb") ||
            !rbh_eval("$\" << 'matrix.rb'", "provide matrix")) {
            return false;
        }
    }

    if (!rbh_eval(BOOTSTRAP, "souptoys.rb bootstrap")) {
        return false;
    }

    // toy classes for the defs we instantiate natively; just World for now
    // (full toy import lands with the object-model bridge)
    if (!rbh_eval("load 'world.rb'", "world.rb")) {
        return false;
    }

    if (!rbh_eval("$core.core_loaded", "core_loaded")) {
        return false;
    }
    return true;
}

// Kernel#load only honours $LOAD_PATH (cleared at boot) for relative paths,
// so toy-class dirs must reach Ruby as ABSOLUTE paths or the resolver
// silently falls back to a script-less Toy subclass.
static const char* abs_dir(const char* dir, char* buf) {
    if (!dir) {
        return ".";
    }
    return realpath(dir, buf) ? buf : dir;
}

bool rbh_load_toy_class(const char* class_name, const char* class_dir) {
    char abs[1024], code[2048];
    snprintf(code, sizeof code,
             "ToyClassResolver.load_toy_class('%s', Pathname.new('%s'))\n",
             class_name, abs_dir(class_dir, abs));
    return rbh_eval(code, class_name);
}

bool rbh_spawn_toy(const char* class_name, const char* class_dir,
                   double x_m, double y_m) {
    char abs[1024], code[2048];
    snprintf(code, sizeof code,
             "t = ToyClassResolver.load_toy_class('%s', Pathname.new('%s')).new\n"
             "t.move(Vector[%.6f, %.6f])\n"
             "$default_engine.toys << t\n",
             class_name, abs_dir(class_dir, abs), x_m, y_m);
    return rbh_eval(code, class_name);
}

void rbh_shutdown(void) {
    ruby_finalize();
    rbh_sprite_map_free();
}
