#pragma once
#define DECLARE_FIELD(type, name, default) type name = default;

#define APPEND_FIELD(type, name, default) \
append(builder, "\t\t" #name " "); \
append(builder, name); \
append(builder, "\n");
