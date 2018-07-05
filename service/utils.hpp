#pragma once

#include "options.pb.h"
#include "zipkin/opentracing.h"

auto load_cli_options(int argc, char** argv) -> is::ArucoServiceOptions;
auto make_tracer(std::string const& name, std::string const& uri)
    -> std::shared_ptr<opentracing::Tracer>;