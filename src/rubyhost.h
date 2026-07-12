#pragma once
#include <stdbool.h>

// Embedded Ruby 1.8.6 host. Registers the engine's Ruby API surface
// (src/ruby_api.inc), then mirrors the original Toybox.exe boot:
// $:.clear -> matrix/e2mmap from resources -> $core/$default_engine/
// $ui_engine/$grid_engine/$engine globals -> eval souptoys.rb bootstrap ->
// load world.rb -> $core.core_loaded (which builds the World toy per engine).
//
// scripts_root is the resource root for Souptoys#resource_load, currently the
// extracted souptoys_core.toy directory. Requires toydefs_load() to have run
// (Toy allocation pulls limb data from the defs by class name).

bool rbh_boot(const char* scripts_root);

// Evaluate Ruby source; on exception prints class/message/backtrace tagged
// with `what` and returns false.
bool rbh_eval(const char* code, const char* what);

void rbh_shutdown(void);
