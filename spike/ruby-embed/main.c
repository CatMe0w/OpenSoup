#include <stdio.h>
#include "ruby.h"

static VALUE engine_ping(VALUE self, VALUE n) {
    (void)self;
    return INT2NUM(NUM2INT(n) * 2);
}

int main(void) {
    ruby_init();
    ruby_script("opensoup");

    // register a stub engine class the way Toybox did
    VALUE cls = rb_define_class("REngine", rb_cObject);
    rb_define_method(cls, "ping", engine_ping, 1);

    int state = 0;
    rb_eval_string_protect(
        "e = REngine.new\n"
        "raise 'ping broken' unless e.ping(21) == 42\n"
        "puts \"REngine#ping(21) => #{e.ping(21)} on #{RUBY_VERSION}p#{RUBY_PATCHLEVEL}\"\n",
        &state);
    if (state) {
        fprintf(stderr, "unexpected ruby exception\n");
        return 1;
    }

    // exception must be contained by rb_*_protect, not longjmp out of main
    rb_eval_string_protect("raise 'deliberate'", &state);
    printf("deliberate raise contained: state=%d\n", state);

    ruby_finalize();
    return state ? 0 : 1;
}
