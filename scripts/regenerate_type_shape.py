"""Regenerates bootstrap/source/analysis/spite_type.spite and type_shape.spite from the member list below."""
members = ["ScalarType", "StringType", "ClassRefType", "NullableType", "VoidType", "EnumType",
           "ListType", "DictionaryType", "UnionType", "ArgumentsType"]
root = "bootstrap/source/analysis/"


def switch(hit, hit_value, miss_value):
    lines = ["        Analysis.Types.%s: return %s\n" % (m, hit_value if m == hit else miss_value) for m in members]
    return "    switch spite_type {\n" + "".join(lines) + "    }\n"


open(root + "spite_type.spite", "w", newline="\n").write(
    "union SpiteType {\n" + "".join("    Analysis.Types.%s\n" % m for m in members) + "}\n")
out = ""
for name, member in [("as_scalar", "ScalarType"), ("as_class_ref", "ClassRefType"), ("as_nullable", "NullableType"),
                     ("as_enum", "EnumType"), ("as_list", "ListType"), ("as_dictionary", "DictionaryType"),
                     ("as_union", "UnionType")]:
    out += "func %s(spite_type: SpiteType): Analysis.Types.%s? {\n" % (name, member)
    out += switch(member, "spite_type", "null") + "}\n\n"
for name, member in [("as_string", "StringType"), ("is_void", "VoidType"),
                     ("is_arguments", "ArgumentsType")]:
    out += "func %s(spite_type: SpiteType): Bool {\n" % name + switch(member, "true", "false") + "}\n\n"
open(root + "type_shape.spite", "w", newline="\n").write(out.rstrip("\n") + "\n")
open(root + "types/arguments_type.spite", "w", newline="\n").write("")
print("regenerated")
