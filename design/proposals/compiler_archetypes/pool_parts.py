# A hand edit of the C the compiler writes, for design/proposals/compiler_archetypes.md (rule P): gives each named
# class a pool of its own, written the way class_pools.spite writes the pool of a class a list holds, so the objects
# of a part (or of a member of a union) come from blocks side by side instead of the C library's allocator.
#   python pool_parts.py in.c out.c PooledClass Class [Class ...]
# PooledClass is any class the compiler already pooled; its pool's text is copied and renamed for each Class.
import re
import sys

source, target, model, classes = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
text = open(source).read()
start = text.index("static %s* %s___pool_free = 0;" % (model, model))
give = text.index("static inline void %s___pool_give(%s* self) {" % (model, model))
end = text.index("}\n", give) + 2
pool = text[start:end]
added = ""
for name in classes:
    added += re.sub(r"\b%s\b" % model, name, pool.replace(model + "___", name + "___"))
    allocation = "%s* self = (%s*)SPITE_MALLOC(sizeof(%s));" % (name, name, name)
    assert allocation in text, "no allocation of " + name
    text = text.replace(allocation, "%s* self = %s___pool_take();" % (name, name))
    free_start = text.index("void %s___free(%s* self) {" % (name, name))
    free_at = text.index("SPITE_FREE(self);", free_start)
    text = text[:free_at] + "%s___pool_give(self);" % name + text[free_at + len("SPITE_FREE(self);"):]
text = text[:end] + added + text[end:]
open(target, "w").write(text)
