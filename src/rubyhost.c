// Embedded Ruby 1.8.6 host: class registry, stub surface, resource host,
// and boot. Methods start as stubs; needed ones are rebound in rubyhost_*.c.
#include "rubyhost.h"
#include "assets_layout.h"
#include "physics.h"
#include "toyfile.h"
#include "toyfile_playset.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rubyhost_internal.h"

void Init_ext(void); // ext/extinit.o: static stringio + syck

// registry

#define MAX_CLASSES 40
#define MAX_TOYPACKS 32
static struct { const char* cname; VALUE cls; } g_reg[MAX_CLASSES];
static int g_nreg;
char g_assets[1024]; // the assets tree root (per-container defs + resources)
char g_root[1024];   // souptoys_core_toy resource root (framework scripts)
static VALUE g_license_properties;
static VALUE g_runtime_license_properties;
static rbh_toypack g_toypacks[MAX_TOYPACKS];
static int g_ntoypacks;

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

static VALUE soup_get_license_properties(VALUE self) {
    (void)self;
    return g_license_properties;
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

static VALUE playset_error_class(void) {
    const ID id = rb_intern("PlaysetParseError");
    return rb_const_defined(rb_cObject, id)
        ? rb_const_get(rb_cObject, id) : rb_eRuntimeError;
}

static void playset_hash_set(VALUE hash, const char* key, VALUE value) {
    rb_hash_aset(hash, ID2SYM(rb_intern(key)), value);
}

static VALUE playset_optional_float_to_ruby(
        const toyfile_optional_float* value) {
    return value->present ? rb_float_new(value->value) : Qnil;
}

static VALUE playset_optional_vec2_to_ruby(
        const toyfile_optional_vec2* value) {
    return value->present
        ? vec_new(value->value[0], value->value[1]) : Qnil;
}

static VALUE playset_header_to_ruby(const toyfile_playset* playset) {
    VALUE hash = rb_hash_new();
    playset_hash_set(hash, "format_name",
                     rb_str_new2("SOUPTOYS.COM TOY FORMAT"));
    playset_hash_set(hash, "version",
                     ULONG2NUM((unsigned long)playset->source_version));
    return hash;
}

static VALUE playset_info_to_ruby(const toyfile_playset_info* info) {
    struct {
        const char* key;
        const char* value;
    } fields[] = {
        {"version_description", info->version_description},
        {"file_description", info->file_description},
        {"author", info->author},
        {"creation_date", info->creation_date},
    };
    VALUE hash = rb_hash_new();
    for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++) {
        playset_hash_set(hash, fields[i].key, rb_str_new2(fields[i].value));
    }
    return hash;
}

static VALUE playset_world_to_ruby(const toyfile_playset* playset) {
    if (!playset->has_world) {
        return Qnil;
    }
    const toyfile_playset_world* world = &playset->world;
    struct {
        const char* key;
        const toyfile_optional_float* value;
    } fields[] = {
        {"left_wall", &world->left_wall},
        {"right_wall", &world->right_wall},
        {"floor", &world->floor},
        {"ceiling", &world->ceiling},
        {"timestep", &world->timestep},
        {"gravity", &world->gravity},
    };
    VALUE hash = rb_hash_new();
    for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++) {
        playset_hash_set(hash, fields[i].key,
                         playset_optional_float_to_ruby(fields[i].value));
    }
    playset_hash_set(hash, "drop_position",
                     playset_optional_vec2_to_ruby(&world->drop_position));
    return hash;
}

static VALUE playset_limb_to_ruby(const toyfile_playset_limb* limb) {
    VALUE hash = rb_hash_new();
    playset_hash_set(hash, "limb_id",
                     ID2SYM(rb_intern(limb->limb_id)));
    playset_hash_set(hash, "position",
                     vec_new(limb->position[0], limb->position[1]));
    playset_hash_set(hash, "orientation",
                     rb_float_new(limb->orientation));
    playset_hash_set(hash, "momentum",
                     vec_new(limb->momentum[0], limb->momentum[1]));
    playset_hash_set(hash, "angular_momentum",
                     rb_float_new(limb->angular_momentum));
    return hash;
}

static VALUE playset_toy_to_ruby(const toyfile_playset_toy* toy) {
    VALUE hash = rb_hash_new();
    playset_hash_set(hash, "toy_instance_id",
                     LONG2NUM((long)toy->toy_instance_id));
    playset_hash_set(hash, "toy_id",
                     ID2SYM(rb_intern(toy->toy_id)));
    playset_hash_set(hash, "extra", rb_str_new2(toy->extra));

    VALUE array = rb_ary_new2((long)toy->limb_count);
    for (size_t i = 0; i < toy->limb_count; i++) {
        rb_ary_push(array, playset_limb_to_ruby(&toy->limbs[i]));
    }
    playset_hash_set(hash, "limbs", array);
    return hash;
}

static VALUE playset_to_ruby(const toyfile_playset* playset) {
    VALUE result = rb_hash_new();
    playset_hash_set(result, "header", playset_header_to_ruby(playset));
    playset_hash_set(result, "info", playset_info_to_ruby(&playset->info));
    playset_hash_set(result, "world", playset_world_to_ruby(playset));

    VALUE array = rb_ary_new2((long)playset->toy_count);
    for (size_t i = 0; i < playset->toy_count; i++) {
        rb_ary_push(array, playset_toy_to_ruby(&playset->toys[i]));
    }
    playset_hash_set(result, "toy_instances", array);
    return result;
}

static VALUE playset_to_ruby_ensure(VALUE playset_value) {
    return playset_to_ruby((const toyfile_playset*)playset_value);
}

static VALUE playset_free_ensure(VALUE playset_value) {
    toyfile_playset_free((toyfile_playset*)playset_value);
    return Qnil;
}

static VALUE soup_read_playset(VALUE self, VALUE path) {
    (void)self;
    VALUE path_string = rb_obj_as_string(path);
    toyfile* file = NULL;
    toyfile_status status = toyfile_open_path(
        StringValueCStr(path_string), &file);
    if (status != TOYFILE_OK) {
        char error[320];
        (void)snprintf(error, sizeof error, "%s", toyfile_error(file));
        toyfile_close(file);
        rb_raise(playset_error_class(), "%s", error);
    }

    toyfile_playset* playset = NULL;
    char error[320];
    status = toyfile_playset_decode(file, &playset, error, sizeof error);
    toyfile_close(file);
    if (status != TOYFILE_OK) {
        rb_raise(playset_error_class(), "%s",
                 error[0] ? error : "could not decode playset");
    }
    return rb_ensure(playset_to_ruby_ensure, (VALUE)playset,
                     playset_free_ensure, (VALUE)playset);
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
    rb_define_method(c, "get_license_properties",
                     soup_get_license_properties, 0);
    rb_define_method(c, "load_paths", soup_load_paths, 0);
    rb_define_method(c, "load_paths=", soup_load_paths_set, 1);
    rb_define_method(c, "exe_path", soup_exe_path, 0);
    rb_define_method(c, "console_open?", soup_console_open_p, 0);
    rb_define_method(c, "read_playset", soup_read_playset, 1);
}

static void build_license_properties(void) {
    g_license_properties = rb_hash_new();
    rb_global_variable(&g_license_properties);
    for (int i = 0; i < toydefs_license_property_count(); i++) {
        const toyprop_t* p = toydefs_license_property_at(i);
        if (!p || !p->key) {
            continue;
        }
        VALUE value;
        switch (p->kind) {
            case TOYPROP_STRING:
                value = rb_str_new2(p->string ? p->string : "");
                break;
            case TOYPROP_INTEGER:
                value = LONG2NUM((long)p->number);
                break;
            case TOYPROP_FLOAT:
                value = rb_float_new(p->number);
                break;
            default:
                continue;
        }
        rb_hash_aset(g_license_properties, rb_str_new2(p->key), value);
    }
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

// rb_funcall behind rb_protect: exceptions must not longjmp through C frames.
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

// Per-frame heartbeat: fixed 0.01s steps with an accumulator, driven through
// the framework's run_steps. Catch-up is capped by both step count and wall
// clock: overrun scene time is dropped, so a heavy scene runs slow instead
// of freezing.
#define PHYS_CATCHUP_BUDGET_MS 8.0
#define PHYS_CATCHUP_MAX_STEPS 25

static double ms_since(clock_t start) {
    return (double)(clock() - start) * 1000.0 / (double)CLOCKS_PER_SEC;
}

void rbh_frame(double dt_ms) {
    static double acc_ms;
    acc_ms += dt_ms;
    int steps = (int)(acc_ms / (PHYS_DT * 1000.0));
    if (steps <= 0) {
        return;
    }
    acc_ms -= steps * (PHYS_DT * 1000.0);
    if (steps > PHYS_CATCHUP_MAX_STEPS) {
        steps = PHYS_CATCHUP_MAX_STEPS; // clamp long stalls
    }
    VALUE eng = rb_gv_get("$default_engine");
    if (NIL_P(eng)) {
        return;
    }
    const clock_t start = clock();
    int done = 0;
    while (done < steps) {
        if (!fcall_protected(eng, "run_steps", 1, INT2FIX(1), Qnil, Qnil,
                             "run_steps")) {
            break;
        }
        done++;
        if (ms_since(start) >= PHYS_CATCHUP_BUDGET_MS) {
            break;
        }
    }
    // Timers see steps that actually ran, not the frame delta.
    if (done > 0) {
        fcall_protected(eng, "dispatch_timers", 1, INT2FIX(done), Qnil, Qnil,
                        "dispatch_timers");
    }
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

// Bootstrap: load souptoys.rb from the resource container.
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
    if (!assets_root) {
        fprintf(stderr, "[rubyhost] assets root is missing\n");
        return false;
    }
    const int assets_len = snprintf(g_assets, sizeof g_assets, "%s",
                                    assets_root);
    const int root_len = container_resource_root(g_root, sizeof g_root,
                                                 assets_root,
                                                 "souptoys_core_toy");
    if (assets_len < 0 || (size_t)assets_len >= sizeof g_assets
        || root_len < 0 || (size_t)root_len >= sizeof g_root) {
        fprintf(stderr, "[rubyhost] assets root is too long\n");
        return false;
    }

    ruby_init();
    ruby_script("souptoys_embedded");
    // Ruby 1.8 traps INT/TERM as SignalExceptions; restore SIG_DFL so the
    // app stays killable.
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
    build_license_properties();

    VALUE load_path = rb_gv_get("$:");
    rb_ary_clear(load_path);
    rb_ary_push(load_path, rb_str_new2(g_root));

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

    // Headless IconToy retains add_toypack calls without constructing the UI.
    if (!rbh_eval("$opensoup_icon_toy = IconToy.new(nil, nil, nil)",
                  "IconToy catalog")) {
        return false;
    }

    // Natively instantiated toy classes; just World for now.
    if (!rbh_eval("load 'world.rb'", "world.rb")) {
        return false;
    }

    if (!rbh_eval("$core.core_loaded", "core_loaded")) {
        return false;
    }
    return true;
}

// Toy-class dirs must be absolute; Kernel#load ignores relative paths not
// on $LOAD_PATH, and we clear $LOAD_PATH at boot.
static const char* abs_dir(const char* dir, char* buf) {
    if (!dir) {
        return ".";
    }
#ifdef _WIN32
    if (!_fullpath(buf, dir, 1024)) {
        return dir;
    }
    // _fullpath returns backslashes; normalize to '/'.
    for (char* p = buf; *p; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }
    return buf;
#else
    return realpath(dir, buf) ? buf : dir;
#endif
}

// Bound as globals: a quote in a path would break an interpolated literal.
static void bind_toy_class(const char* class_name, const char* class_dir) {
    char abs[1024];
    rb_gv_set("$opensoup_toy_class", rb_str_new2(class_name));
    rb_gv_set("$opensoup_toy_dir", rb_str_new2(abs_dir(class_dir, abs)));
}

bool rbh_load_toy_class(const char* class_name, const char* class_dir) {
    bind_toy_class(class_name, class_dir);
    return rbh_eval("ToyClassResolver.load_toy_class($opensoup_toy_class,"
                    " Pathname.new($opensoup_toy_dir))\n",
                    class_name);
}

static int cmp_toypack(const void* a, const void* b) {
    const rbh_toypack* pa = a;
    const rbh_toypack* pb = b;
    if (pa->order < pb->order) {
        return -1;
    }
    if (pa->order > pb->order) {
        return 1;
    }
    return strcmp(pa->id, pb->id);
}

bool rbh_catalog_finalize(void) {
    // Snapshots add_toypack only; a future pass should also run add_icon
    // to honour LicensePolicy and seasonal rewrites.
    static const char* snapshot =
        "$opensoup_license_properties = $engine.get_license_properties;"
        "$opensoup_toypacks = IconToy.toypacks.map { |id, p|"
        " [id.to_s, p[:license].to_s, p[:sprite_path].to_s, p[:order].to_f]"
        " }";
    if (!rbh_eval(snapshot, "Toybox catalog")) {
        return false;
    }
    g_runtime_license_properties = rb_gv_get("$opensoup_license_properties");
    VALUE rows = rb_gv_get("$opensoup_toypacks");
    if (TYPE(rows) != T_ARRAY) {
        return false;
    }

    for (int i = 0; i < g_ntoypacks; i++) {
        free((char*)g_toypacks[i].id);
        free((char*)g_toypacks[i].license);
        free((char*)g_toypacks[i].sprite_path);
    }
    g_ntoypacks = 0;
    const long count = RARRAY(rows)->len;
    for (long i = 0; i < count && g_ntoypacks < MAX_TOYPACKS; i++) {
        VALUE row = rb_ary_entry(rows, i);
        if (TYPE(row) != T_ARRAY || RARRAY(row)->len != 4) {
            continue;
        }
        VALUE id = rb_ary_entry(row, 0);
        VALUE license = rb_ary_entry(row, 1);
        VALUE sprite_path = rb_ary_entry(row, 2);
        VALUE order = rb_ary_entry(row, 3);
        if (TYPE(id) != T_STRING || TYPE(license) != T_STRING ||
            TYPE(sprite_path) != T_STRING || TYPE(order) != T_FLOAT) {
            continue;
        }
        rbh_toypack* pack = &g_toypacks[g_ntoypacks++];
        pack->id = strdup(StringValueCStr(id));
        pack->license = strdup(StringValueCStr(license));
        pack->sprite_path = strdup(StringValueCStr(sprite_path));
        pack->order = (float)NUM2DBL(order);
    }
    qsort(g_toypacks, (size_t)g_ntoypacks, sizeof g_toypacks[0],
          cmp_toypack);
    return true;
}

int rbh_toypack_count(void) {
    return g_ntoypacks;
}

const rbh_toypack* rbh_toypack_at(int index) {
    return index >= 0 && index < g_ntoypacks ? &g_toypacks[index] : NULL;
}

const char* rbh_toy_pack(const char* class_name) {
    if (!class_name || TYPE(g_runtime_license_properties) != T_HASH) {
        return NULL;
    }
    char key[512];
    snprintf(key, sizeof key, "%s.toypack", class_name);
    VALUE value = rb_hash_aref(g_runtime_license_properties,
                               rb_str_new2(key));
    return TYPE(value) == T_STRING ? StringValueCStr(value) : NULL;
}

// Decode the playset natively; let the framework assemble it. Our IconToy
// is headless, so the engine is addressed directly.
bool rbh_open_playset(const char* path) {
    if (!path || !path[0]) {
        return false;
    }
    // Bound as a global; see bind_toy_class.
    rb_gv_set("$opensoup_playset_path", rb_str_new2(path));
    return rbh_eval("$default_engine.open_playset("
                    "$engine.read_playset($opensoup_playset_path))\n",
                    "open_playset");
}

bool rbh_spawn_toy(const char* class_name, const char* class_dir,
                   double x_m, double y_m) {
    bind_toy_class(class_name, class_dir);
    char code[512];
    snprintf(code, sizeof code,
             "t = ToyClassResolver.load_toy_class($opensoup_toy_class,"
             " Pathname.new($opensoup_toy_dir)).new\n"
             "t.move(Vector[%.6f, %.6f])\n"
             "$default_engine.toys << t\n",
             x_m, y_m);
    return rbh_eval(code, class_name);
}

void rbh_shutdown(void) {
    ruby_finalize();
    rbh_sprite_map_free();
}
