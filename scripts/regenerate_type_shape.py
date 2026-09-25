"""Regenerates bootstrap/source/analysis/spite_type.spite and type_shape.spite from the member list below."""
members = ["ScalarType", "StringType", "ClassRefType", "NullableType", "VoidType", "EnumType",
           "ListType", "DictionaryType", "UnionType", "ArgumentsType", "SymbolType"]
root = "bootstrap/source/analysis/"


def narrowed(member):
    return ("    if spite_type == Analysis.Types.%s {\n        return spite_type\n    }\n    return null\n" % member)


def tested(member):
    return "    return spite_type == Analysis.Types.%s\n" % member


open(root + "spite_type.spite", "w", newline="\n").write(
    "union SpiteType {\n" + "".join("    Analysis.Types.%s\n" % m for m in members) + "}\n")
out = ""
for name, member in [("as_scalar", "ScalarType"), ("as_class_ref", "ClassRefType"), ("as_nullable", "NullableType"),
                     ("as_enum", "EnumType"), ("as_list", "ListType"), ("as_dictionary", "DictionaryType"),
                     ("as_union", "UnionType")]:
    out += "func %s(spite_type: SpiteType): Analysis.Types.%s? {\n" % (name, member)
    out += narrowed(member) + "}\n\n"
for name, member in [("as_string", "StringType"), ("is_void", "VoidType"),
                     ("is_arguments", "ArgumentsType"), ("is_symbol", "SymbolType")]:
    out += "func %s(spite_type: SpiteType): Boolean {\n" % name + tested(member) + "}\n\n"
open(root + "type_shape.spite", "w", newline="\n").write(out.rstrip("\n") + "\n")
open(root + "types/arguments_type.spite", "w", newline="\n").write("")
open(root + "types/symbol_type.spite", "w", newline="\n").write("")
print("regenerated")
