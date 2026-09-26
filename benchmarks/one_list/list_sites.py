"""Read-only study for D222 (docs/proposals/one_list.md): for every List site in a corpus of Spite programs, which
layout one automatic List could give it, and why the others were refused.

It reads source text only, line by line, the way Spite is written (one statement per line, four-space blocks, a
file is a class). It is an approximation of what the compiler would prove, and says where it is optimistic or
pessimistic in the report; it changes nothing.

    python benchmarks/one_list/list_sites.py <name> <folder> [<folder> ...] [--library <folder>] [--sites] [--loops]
    python benchmarks/one_list/list_sites.py <name> --each <parent> [<parent> ...]
    python benchmarks/one_list/list_sites.py library library

Each folder is walked for *.spite files; the folders given are one program (classes resolve inside it and the
library). With --each every folder inside each parent is a program of its own and the totals are summed. With
--sites every site is printed with its verdict, with --loops every loop over a list.
"""
import os
import re
import sys
from collections import defaultdict, Counter

PLAIN_NUMBERS = {"Integer", "Long", "Short", "Byte", "Float", "Double", "UnsignedInteger", "UnsignedLong",
                 "UnsignedShort", "UnsignedByte", "Half"}
PLAIN = PLAIN_NUMBERS | {"Boolean", "Memory.Address", "Address"}
SIZE_CHANGING = {"append", "prepend", "insert", "remove_at", "remove_first", "remove_last", "clear",
                 "remove_swapping", "reserve"}
GROWING = {"append", "prepend", "insert", "reserve"}
ITEM_READS = {"get_at", "first", "last", "find_at"}
ITEM_MOVES_OUT = {"remove_first", "remove_last"}
HARMLESS = {"count", "is_empty", "contains", "join", "reverse", "deep_copy", "memory", "to_string", "class"}
TEMPLATE_PREFIXES = ("each_", "count_", "sum_", "any_", "all_", "map_")
PASSED = {"each", "map", "filter", "find", "any", "all", "count", "sum", "sort_by"}
ORDERED_EFFECT = re.compile(r"\b(console|print|file|File|clock|Clock|random|Random|socket|network|stdout|input)\b")
IDENT = r"[A-Za-z_][A-Za-z0-9_]*"
FUNC_RE = re.compile(r"^func (" + IDENT + r")\((.*)\)(?:: (.+?))? \{\s*$")
VAR_RE = re.compile(r"^(\s*)var (" + IDENT + r")(?:: ([^=]+?))?(?: = (.+))?$")


def split_arguments(text):
    parts, depth, current, quote = [], 0, "", False
    for character in text:
        if character == '"':
            quote = not quote
        if not quote and character in "([{<":
            depth += 1
        if not quote and character in ")]}>":
            depth -= 1
        if character == "," and depth == 0 and not quote:
            parts.append(current.strip())
            current = ""
        else:
            current += character
    if current.strip():
        parts.append(current.strip())
    return parts


def strip_strings(line):
    return re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)


def class_name_of(path):
    stem = os.path.splitext(os.path.basename(path))[0]
    return "".join(part.capitalize() for part in stem.split("_"))


def last_segment(type_text):
    type_text = type_text.strip().rstrip("?")
    return type_text.split(".")[-1]


class Function:
    def __init__(self, owner, name, parameters, returns, line):
        self.owner, self.name, self.returns, self.line = owner, name, returns, line
        self.parameters = []  # (name, type)
        for piece in split_arguments(parameters):
            if ":" in piece:
                parameter, kind = piece.split(":", 1)
                self.parameters.append((parameter.strip(), kind.strip()))
        self.body = []  # (line number, indent, text without strings)
        self.locals = {}  # name -> (type text or None, initializer, line index)


class ClassFile:
    def __init__(self, path, program):
        self.path, self.program = path, program
        self.name = class_name_of(path)
        self.singleton = False
        self.generic = False
        self.attributes = {}  # name -> (type text, initializer, line)
        self.functions = {}
        self.enums = set()
        self.types = set()
        self.has_drop = False
        self.uses_this_as_value = False
        self.is_shape = False
        self.parse()

    def parse(self):
        with open(self.path, encoding="utf-8", errors="replace") as handle:
            raw_lines = handle.read().split("\n")
        # a statement the formatter wrapped over several lines is read as the one line it is
        lines, numbers, pending, pending_number, depth = [], [], None, 0, 0
        for number, raw in enumerate(raw_lines, 1):
            text = strip_strings(raw.rstrip())
            if pending is not None:
                open_literal = strip_strings(pending).count("[") > strip_strings(pending).count("]")
                ends_open = pending.rstrip().endswith(("[", ",", "("))
                separator = ", " if open_literal and not ends_open and not raw.strip().startswith("]") else " "
                pending = pending.rstrip() + separator + raw.strip()
            else:
                pending, pending_number = raw.rstrip(), number
            depth = 0
            for character in strip_strings(pending):
                if character in "([":
                    depth += 1
                elif character in ")]":
                    depth -= 1
            if depth > 0 and not re.match(r"^\s*(while|if|switch|\} else)", pending):
                continue
            lines.append(pending.replace("[ ", "[").replace("( ", "(").replace(", )", ")").replace(",)", ")"))
            numbers.append(pending_number)
            pending = None
        if pending is not None:
            lines.append(pending)
            numbers.append(pending_number)
        current = None
        for number, raw in zip(numbers, lines):
            line = raw.rstrip()
            text = strip_strings(line)
            if not line.strip():
                continue
            if line == "singleton":
                self.singleton = True
            if line.startswith("generic "):
                self.generic = True
            enum = re.match(r"^enum (" + IDENT + r")", line)
            if enum:
                self.enums.add(enum.group(1))
            kind = re.match(r"^(?:type|union) (" + IDENT + r")", line)
            if kind:
                self.types.add(kind.group(1))
                if kind.group(1) == self.name and current is None:
                    self.is_shape = True
            function = FUNC_RE.match(line)
            if function:
                current = Function(self, function.group(1), function.group(2), function.group(3), number)
                self.functions.setdefault(function.group(1), current)
                if function.group(1) == "drop":
                    self.has_drop = True
                continue
            if line == "}":
                current = None
                continue
            if current is None:
                attribute = VAR_RE.match(line)
                if attribute and attribute.group(1) == "":
                    self.attributes[attribute.group(2)] = (attribute.group(3), attribute.group(4), number)
                continue
            indent = len(line) - len(line.lstrip(" "))
            current.body.append((number, indent, text.strip()))
            declared = VAR_RE.match(line)
            if declared:
                current.locals.setdefault(declared.group(2), (declared.group(3), declared.group(4),
                                                              len(current.body) - 1))
            if re.search(r"\bthis\b(?!\s*\.)", text):
                self.uses_this_as_value = True


class Program:
    def __init__(self, name, folders, library):
        self.name = name
        self.classes = {}
        self.own = []
        for folder in folders:
            for path in spite_files(folder):
                cls = ClassFile(path, self)
                self.own.append(cls)
                self.classes.setdefault(cls.name, cls)
        self.library = []
        for path in spite_files(library):
            cls = ClassFile(path, self)
            self.library.append(cls)
            self.classes.setdefault(cls.name, cls)
        self.all_classes = self.own + self.library
        self.functions_by_name = defaultdict(list)
        for cls in self.all_classes:
            for function in cls.functions.values():
                self.functions_by_name[function.name].append(function)
        self.enums = set()
        self.types = set()
        for cls in self.all_classes:
            self.enums |= cls.enums
            self.types |= cls.types
        self._fits = {}

    def is_plain(self, type_text):
        segment = type_text.strip().rstrip("?")
        return segment in PLAIN or last_segment(segment) in self.enums

    def fits_vector(self, type_text):
        """D204's rule, read from source: a class whose attributes are all plain, text or singletons."""
        segment = last_segment(type_text)
        if segment in self._fits:
            return self._fits[segment]
        self._fits[segment] = (False, "unknown class")
        cls = self.classes.get(segment)
        if cls is None or type_text.startswith("$"):
            result = (False, "not a class of the program (generic, type or union)")
        elif cls.is_shape:
            result = (False, "a union or a type, whose class is known only at run time")
        elif cls.singleton or cls.generic:
            result = (False, "singleton or generic class")
        elif cls.has_drop:
            result = (False, "has drop()")
        elif cls.uses_this_as_value:
            result = (False, "uses 'this' as a value")
        else:
            result = (True, "")
            for name, (declared, initializer, _) in cls.attributes.items():
                kind = declared or self.infer(initializer or "", cls, None)
                if kind is None:
                    result = (False, "attribute '%s' of unknown type" % name)
                    break
                base = kind.strip().rstrip("?")
                if self.is_plain(base) or base == "String":
                    continue
                other = self.classes.get(last_segment(base))
                if other is not None and other.singleton:
                    continue
                result = (False, "attribute '%s' holds %s" % (name, base))
                break
        self._fits[segment] = result
        return result

    def infer(self, expression, cls, function):
        expression = expression.strip()
        if not expression:
            return None
        if re.fullmatch(r"-?\d+", expression):
            return "Integer"
        if re.fullmatch(r"-?\d+\.\d+", expression):
            return "Float"
        if expression.startswith('"'):
            return "String"
        if expression in ("true", "false"):
            return "Boolean"
        if expression.startswith("["):
            inner = split_arguments(expression[1:-1]) if expression.endswith("]") else []
            if inner:
                element = self.infer(inner[0], cls, function)
                if element:
                    return "List<%s>" % element
            return "List<?>"
        constructed = re.match(r"^((?:" + IDENT + r"\.)*" + IDENT + r"(?:<.*>)?)\(", expression)
        if constructed:
            name = constructed.group(1)
            if name.startswith("List<") or name.startswith("Dictionary<") or name.startswith("Vector<") \
                    or name.startswith("Items<"):
                return name
            if last_segment(name.split("<")[0]) in self.classes:
                return name
        called = re.match(r"^(?:(" + IDENT + r")\.)?(" + IDENT + r")\(.*\)$", expression)
        if called:
            candidates = [f.returns for f in self.functions_by_name.get(called.group(2), []) if f.returns]
            if candidates and len(set(candidates)) == 1:
                return candidates[0]
        if function is not None and re.fullmatch(IDENT, expression):
            if expression in function.locals and function.locals[expression][0]:
                return function.locals[expression][0]
            for parameter, kind in function.parameters:
                if parameter == expression:
                    return kind
        if cls is not None and re.fullmatch(IDENT, expression) and expression in cls.attributes:
            declared, initializer, _ = cls.attributes[expression]
            return declared or None
        if re.search(r"[-+*/%]", expression) and not re.search(r"[a-z]\(", expression):
            return None
        return None


def spite_files(folder):
    for root, directories, files in os.walk(folder):
        directories[:] = [d for d in directories if not d.startswith(".")]
        for name in sorted(files):
            if name.endswith(".spite"):
                yield os.path.join(root, name)


def element_of(list_type):
    match = re.match(r"^(?:List|Vector|Items)<(.+)>\??$", list_type.strip())
    return match.group(1).strip() if match else None


def container_of(list_type):
    return list_type.strip().split("<")[0]


class Node:
    """One name that holds a list: an attribute of a class, or a local or parameter of a function."""

    def __init__(self, kind, cls, function, name, element):
        self.kind, self.cls, self.function, self.name, self.element = kind, cls, function, name, element

    def key(self):
        return (self.kind, self.cls.path, self.function.name if self.function else "", self.name)


class Site:
    def __init__(self, node, line, how):
        self.node, self.line, self.how = node, line, how
        self.refusals = defaultdict(set)  # layout -> reasons
        self.resized = False
        self.grows_in_loop = False
        self.shrinks = False
        self.kept_past_resize = False
        self.item_kept = False
        self.loops = []


class Study:
    def __init__(self, program):
        self.program = program
        self.nodes = {}
        self.sites = []
        self.loops = []
        self.visiting = set()

    # ---------- finding sites ----------
    def run(self):
        for cls in self.program.own:
            if cls.name in ("List", "Vector", "Items", "InlineMemory", "TypedMemory"):
                continue
            for name, (declared, initializer, line) in cls.attributes.items():
                kind = declared or self.program.infer(initializer or "", cls, None)
                if kind and element_of(kind):
                    node = self.node("attribute", cls, None, name, element_of(kind))
                    self.sites.append(Site(node, line, "attribute"))
                    self.sites[-1].container = container_of(kind)
            for function in cls.functions.values():
                for name, (declared, initializer, index) in function.locals.items():
                    if (initializer or "").strip() == "[" and index + 1 < len(function.body):
                        initializer = "[" + function.body[index + 1][2] + "]"
                    kind = declared or self.program.infer(initializer or "", cls, function)
                    if kind and element_of(kind):
                        how = "literal" if (initializer or "").startswith("[") else (
                            "made" if (initializer or "").startswith(("List<", "Vector<", "Items<")) else "received")
                        node = self.node("local", cls, function, name, element_of(kind))
                        self.sites.append(Site(node, function.body[index][0], how))
                        self.sites[-1].container = container_of(kind)
        for site in self.sites:
            self.judge(site)

    def node(self, kind, cls, function, name, element):
        created = Node(kind, cls, function, name, element)
        return self.nodes.setdefault(created.key(), created)

    # ---------- uses ----------
    def uses(self, node):
        """(function, body index, text) of every line that names the node."""
        found = []
        pattern = re.compile(r"(?<![\w.$])" + re.escape(node.name) + r"\b")
        if node.kind == "attribute":
            for function in node.cls.functions.values():
                if node.name in function.locals or any(p == node.name for p, _ in function.parameters):
                    continue
                for index, (_, _, text) in enumerate(function.body):
                    if pattern.search(text):
                        found.append((function, index, text))
            outside = re.compile(r"(" + IDENT + r")\." + re.escape(node.name) + r"\b")
            for cls in self.program.all_classes:
                if cls is node.cls:
                    continue
                for function in cls.functions.values():
                    for index, (_, _, text) in enumerate(function.body):
                        for match in outside.finditer(text):
                            receiver = self.receiver_class(function, match.group(1))
                            if receiver is None or receiver is node.cls:
                                found.append((function, index, text))
                                break
        else:
            for index, (_, _, text) in enumerate(node.function.body):
                if pattern.search(text):
                    found.append((node.function, index, text))
        return found

    # ---------- judging ----------
    def judge(self, site):
        element = site.node.element
        plain = self.program.is_plain(element)
        text_items = element.rstrip("?") == "String"
        fits, why_not = (True, "") if (plain or text_items) else self.program.fits_vector(element)
        site.plain, site.text_items, site.fits = plain, text_items, fits
        site.numeric = element.rstrip("?") in PLAIN_NUMBERS
        if not (plain or text_items) and not fits:
            site.refusals["inline"].add("item class does not fit inline: " + why_not)
        if not (plain or text_items) and self.reflected(last_segment(element)):
            site.refusals["inline"].add("the item class's instances are read by reflection (.instances)")
        self.visiting = set()
        self.walk(site, site.node, 0)
        # stack: a local whose size is fixed by straight-line code and which never leaves the function
        if site.node.kind != "local":
            site.refusals["stack"].add("an attribute lives as long as its object")
        if site.how == "received":
            site.refusals["stack"].add("made elsewhere and received here")
        if site.shrinks or site.grows_in_loop:
            site.refusals["stack"].add("its size changes in a loop or by removal")
        # simd: plain numbers, touched by a loop the C compiler can see through
        if not site.numeric:
            site.refusals["simd"].add("items are not numbers")
        # parallel: judged per loop, recorded on the site
        if not (site.fits and not site.refusals["inline"]) and not plain:
            site.refusals["parallel"].add("items are not proven distinct (not inline)")

    def reflected(self, class_name):
        pattern = re.compile(r"\b" + re.escape(class_name) + r"\.instances\b|\.class\.instances\b")
        for cls in self.program.own:
            for function in cls.functions.values():
                for _, _, text in function.body:
                    if pattern.search(text):
                        return True
        return False

    def walk(self, site, node, depth):
        if node.key() in self.visiting or depth > 6:
            return
        self.visiting.add(node.key())
        for function, index, text in self.uses(node):
            self.use(site, node, function, index, text, depth)

    def use(self, site, node, function, index, text, depth):
        name = re.escape(node.name)
        prefix = r"(?:(?<![\w.$])|(?<=\.))"
        body = function.body
        indent = body[index][1]
        in_loop = self.loop_depth(function, index) > 0
        method = re.search(prefix + name + r"\.(" + IDENT + r")\(", text)
        handled = False
        for match in re.finditer(prefix + name + r"\.(" + IDENT + r")(\(?)", text):
            member, called = match.group(1), match.group(2)
            handled = True
            arguments = self.arguments_after(text, match.end() - 1) if called else []
            if member in SIZE_CHANGING:
                site.resized = True
                if member in GROWING and (in_loop or function is not site.node.function):
                    site.grows_in_loop = True
                if member not in GROWING:
                    site.shrinks = True
                if member in GROWING and arguments:
                    self.check_fresh(site, function, index, arguments[-1], depth)
                if member in ITEM_MOVES_OUT:
                    pass  # the list was the only holder, so the removed item is a copy nobody else sees
                continue
            if member == "set_at" and arguments:
                self.check_fresh(site, function, index, arguments[-1], depth)
                continue
            if member in ITEM_READS or member.startswith("find_by_"):
                self.item_read(site, function, index, text, match.start(), depth)
                continue
            if member.startswith("filter_"):
                if not re.search(re.escape(match.group(0)) + r"[^)]*\)\.(each_|count_|sum_|any_|all_|map_|filter_|parallel_each_)", text):
                    site.refusals["inline"].add("a filter_ result shares the items with another list")
                continue
            if member.startswith("parallel_each_") or member.startswith(TEMPLATE_PREFIXES):
                self.template_loop(site, node, function, index, member, text)
                continue
            if member in PASSED and arguments:
                self.passed_function(site, node, function, index, member, arguments, depth)
                continue
            if member.startswith("sort_by_") or member == "copy":
                site.refusals["inline"].add("'%s' makes a second list of the same items" % member)
                continue
            if member in HARMLESS or member.startswith("memory"):
                continue
            if not called:
                continue
            site.refusals["inline"].add("unknown member '%s'" % member)
        # indexing
        for match in re.finditer(prefix + name + r"\[", text):
            handled = True
            closing = self.matching(text, match.end() - 1)
            after = text[closing + 1:].lstrip()
            before = text[:match.start()].rstrip()
            if after.startswith("=") and not after.startswith("=="):
                arguments = [after[1:].strip()]
                self.check_fresh(site, function, index, arguments[0], depth)
                continue
            self.item_read(site, function, index, text, match.start(), depth)
            self.maybe_loop(site, node, function, index)
        # the list itself passed on
        bare = re.compile(prefix + name + r"(?![\w.\[(])")
        for match in bare.finditer(text):
            handled = True
            self.list_flows(site, node, function, index, text, match.start(), match.end(), depth)

    def attribute_element(self, cls, name):
        declared, initializer, _ = cls.attributes[name]
        kind = declared or self.program.infer(initializer or "", cls, None) or ""
        element = element_of(kind)
        return last_segment(element) if element else None

    def receiver_class(self, function, receiver):
        kind = None
        if receiver == "this":
            return function.owner
        if receiver in function.locals:
            declared, initializer, _ = function.locals[receiver]
            kind = declared or self.program.infer(initializer or "", function.owner, function)
        for parameter, parameter_kind in function.parameters:
            if parameter == receiver:
                kind = parameter_kind
        if kind is None and receiver in function.owner.attributes:
            declared, initializer, _ = function.owner.attributes[receiver]
            kind = declared or self.program.infer(initializer or "", function.owner, None)
        if kind is None and receiver[:1].isupper():
            return self.program.classes.get(receiver)
        if kind is None:
            return None
        return self.program.classes.get(last_segment(kind.split("<")[0]))

    def arguments_after(self, text, open_index):
        closing = self.matching(text, open_index)
        return split_arguments(text[open_index + 1:closing])

    @staticmethod
    def matching(text, open_index):
        pairs = {"(": ")", "[": "]", "{": "}"}
        opener = text[open_index]
        depth = 0
        for position in range(open_index, len(text)):
            if text[position] == opener:
                depth += 1
            elif text[position] == pairs.get(opener):
                depth -= 1
                if depth == 0:
                    return position
        return len(text) - 1

    @staticmethod
    def loop_depth(function, index):
        depth, indent = 0, function.body[index][1]
        for position in range(index - 1, -1, -1):
            number, line_indent, text = function.body[position]
            if line_indent < indent:
                if text.startswith("while "):
                    depth += 1
                indent = line_indent
        return depth

    # ---------- appended values ----------
    def check_fresh(self, site, function, index, value, depth, caller_chain=0):
        if site.plain or site.text_items:
            return
        value = value.strip()
        reason = self.freshness(function, index, value, depth, caller_chain)
        if reason:
            site.refusals["inline"].add(reason)
            site.refusals["parallel"].add("items are not proven distinct: " + reason)

    def freshness(self, function, index, value, depth, caller_chain):
        if re.match(r"^(?:" + IDENT + r"\.)*[A-Z]" + r"[\w]*(?:<.*>)?\(", value):
            return ""  # a construction, used for nothing else
        if value.endswith(".copy()") or value.endswith(".deep_copy()"):
            return ""
        if not re.fullmatch(IDENT, value):
            return "appends a value read from elsewhere (%s)" % ("an item of another list" if "[" in value else "an expression")
        if value in function.locals:
            declared, initializer, declared_at = function.locals[value]
            for earlier in range(index - 1, -1, -1):
                redeclared = VAR_RE.match(function.body[earlier][2])
                if redeclared and redeclared.group(2) == value:
                    declared_at, initializer = earlier, redeclared.group(4)
                    break
            initializer = (initializer or "").strip()
            if not (re.match(r"^(?:" + IDENT + r"\.)*[A-Z][\w]*(?:<.*>)?\(", initializer)
                    or initializer.endswith(".copy()")):
                return "appends a local that holds a value made elsewhere"
            if self.loop_depth(function, index) > self.loop_depth(function, declared_at):
                return "appends one object on every pass of a loop"
            pattern = re.compile(r"(?<![\w.$])" + re.escape(value) + r"\b")
            block_indent = function.body[declared_at][1]
            for later in range(index + 1, len(function.body)):
                if function.body[later][1] < block_indent:
                    break
                if function.body[later][2].startswith("var " + value + " "):
                    break
                if pattern.search(function.body[later][2]):
                    return "the appended object is used after the append"
            for between in range(declared_at + 1, index):
                text = function.body[between][2]
                for match in pattern.finditer(text):
                    rest = text[match.end():]
                    if not rest.startswith("."):
                        return "the appended object is passed or kept before the append"
                    member = re.match(r"\.(" + IDENT + r")(\(?)", rest)
                    if member and not member.group(2):
                        after = rest[member.end():].lstrip()
                        if not (after.startswith("=") or after == "" or after[0] in "+-*/<>=!,)]"):
                            return "the appended object is passed or kept before the append"
            return ""
        for position, (parameter, _) in enumerate(function.parameters):
            if parameter == value:
                if caller_chain >= 3:
                    return "appends a parameter whose callers are too far to follow"
                return self.callers_fresh(function, position, depth, caller_chain + 1)
        if value in function.owner.attributes:
            return "appends an attribute of its object, which keeps it"
        return "appends a value made elsewhere"

    def callers_fresh(self, function, position, depth, caller_chain):
        callers = self.call_sites(function)
        if not callers:
            return "appends a parameter of a function nobody calls by name (reached through a value or a walk)"
        for caller, index, arguments in callers:
            if position >= len(arguments):
                return "appends a parameter filled by a walk or a default"
            reason = self.freshness(caller, index, arguments[position], depth, caller_chain)
            if reason:
                return "a caller of '%s': %s" % (function.name, reason)
        return ""

    def call_sites(self, function):
        found = []
        if function.name == function.owner.name:
            pattern = re.compile(r"(?<![\w.$])" + re.escape(function.name) + r"\(")
        else:
            pattern = re.compile(r"(?:(?<![\w.$])|(?<=\.))" + re.escape(function.name) + r"\(")
        for cls in self.program.all_classes:
            for other in cls.functions.values():
                for index, (_, _, text) in enumerate(other.body):
                    for match in pattern.finditer(text):
                        before = text[:match.start()]
                        if not before.endswith(".") and cls is not function.owner and function.name != function.owner.name:
                            continue
                        arguments = self.arguments_after(text, match.end() - 1)
                        if len(arguments) == len(function.parameters):
                            found.append((other, index, arguments))
        return found

    # ---------- reads of an item ----------
    def item_read(self, site, function, index, text, start, depth):
        if site.plain:
            return
        expression_end = self.expression_end(text, start)
        after = text[expression_end:]
        before = text[:start].rstrip()
        if after.startswith("."):
            member = re.match(r"\.(" + IDENT + r")(\(?)", after)
            if member and not member.group(2):
                element_class = self.program.classes.get(last_segment(site.node.element))
                if element_class and member.group(1) in element_class.functions:
                    site.refusals["inline"].add("an item's function is taken as a value")
            if member and member.group(1) == "copy":
                return
            return
        if before.endswith("crash") or before.endswith("assert") or before.endswith("if") or \
                before.endswith("while") or before.endswith("not"):
            if not after.strip() or after.strip() in ("{",) or after.strip().startswith(("and", "or", "{")):
                return
        declared = re.match(r"^var (" + IDENT + r")(?:: [^=]+)? = $", text[:start])
        if declared and not after.strip():
            self.item_local(site, function, index, declared.group(1), depth)
            return
        if site.text_items:
            return
        if before.startswith("return"):
            site.refusals["inline"].add("an item is returned")
        elif re.search(r"\.(append|prepend|insert|set_at)\($", before) or re.search(r"\.(append|prepend|insert|set_at)\(.*,\s*$", before):
            site.refusals["inline"].add("an item is put in another list")
        elif re.search(r"(^|[^=!<>])=\s*$", before):
            site.refusals["inline"].add("an item is kept in an attribute or a second name")
        elif re.search(r"(==|!=)\s*$", before) or after.lstrip().startswith(("==", "!=")):
            site.refusals["inline"].add("an item is compared by identity")
        elif re.search(r"[(,]\s*$", before):
            site.refusals["inline"].add("an item is passed as an argument")
        else:
            site.refusals["inline"].add("an item is used as a value")
        site.item_kept = True

    def expression_end(self, text, start):
        position = start
        while position < len(text) and (text[position].isalnum() or text[position] in "_."):
            position += 1
        while position < len(text) and text[position] in "[(":
            position = self.matching(text, position) + 1
        return position

    def item_local(self, site, function, index, name, depth):
        """A local bound to an item: inline is invisible only if it is used through its members."""
        if site.text_items:
            return
        pattern = re.compile(r"(?<![\w.$])" + re.escape(name) + r"\b")
        element_class = self.program.classes.get(last_segment(site.node.element))
        resized_at = None
        for later in range(index + 1, len(function.body)):
            text = function.body[later][2]
            if re.search(r"(?<![\w.$])" + re.escape(site.node.name) + r"\.(" + "|".join(SIZE_CHANGING) + r")\(", text):
                resized_at = later
            for match in pattern.finditer(text):
                if resized_at is not None:
                    site.kept_past_resize = True
                rest = text[match.end():]
                before = text[:match.start()].rstrip()
                if rest.startswith("."):
                    member = re.match(r"\.(" + IDENT + r")(\(?)", rest)
                    if member and not member.group(2) and element_class and member.group(1) in element_class.functions:
                        site.refusals["inline"].add("an item's function is taken as a value")
                    continue
                if before.endswith(("crash", "assert", "if", "not", "while")) and not rest.strip().startswith(("==", "!=")):
                    continue
                if re.match(r"^" + re.escape(name) + r" = ", text):
                    continue
                if before.startswith("return"):
                    site.refusals["inline"].add("an item is returned")
                elif re.search(r"\.(append|prepend|insert|set_at)\(", before):
                    site.refusals["inline"].add("an item is put in another list")
                elif re.search(r"(^|[^=!<>])=\s*$", before):
                    site.refusals["inline"].add("an item is kept in an attribute or a second name")
                elif re.search(r"(==|!=)\s*$", before) or rest.lstrip().startswith(("==", "!=")):
                    site.refusals["inline"].add("an item is compared by identity")
                elif re.search(r"[(,]\s*$", before):
                    site.refusals["inline"].add("an item is passed as an argument")
                elif "{" in before and ":" in before:
                    site.refusals["inline"].add("an item is put in an object literal")
                else:
                    site.refusals["inline"].add("an item is used as a value")
                site.item_kept = True
        if site.kept_past_resize:
            site.refusals["borrow"].add("an item is read after its list changes size")

    # ---------- the list itself flowing ----------
    def list_flows(self, site, node, function, index, text, start, end, depth):
        before = text[:start].rstrip()
        after = text[end:].lstrip()
        if re.match(r"^var " + re.escape(node.name) + r"\b", text) and node.kind == "local":
            return  # its own declaration
        if node.kind == "attribute" and re.match(r"^var ", text) and text.startswith("var " + node.name):
            return
        if before.startswith("return") and not after:
            self.flows_to_callers(site, function, depth)
            return
        declared = re.match(r"^var (" + IDENT + r")(?:: [^=]+)? =$", before)
        if declared and not after:
            alias = self.node("local", function.owner, function, declared.group(1), node.element)
            self.walk(site, alias, depth + 1)
            return
        assigned = re.match(r"^((?:" + IDENT + r"\.)*" + IDENT + r") =$", before)
        if assigned and not after:
            target = assigned.group(1)
            owner_attribute = target.split(".")[-1]
            if "." not in target and owner_attribute in function.owner.attributes:
                classes = [function.owner]
            else:
                receiver = self.receiver_class(function, target.split(".")[-2]) if "." in target else None
                if receiver is not None and owner_attribute in receiver.attributes:
                    classes = [receiver]
                else:
                    classes = [c for c in self.program.all_classes if owner_attribute in c.attributes
                               and self.attribute_element(c, owner_attribute) == last_segment(node.element)]
            if not classes:
                site.refusals["inline"].add("the list is kept somewhere the study cannot follow")
                site.refusals["stack"].add("the list is kept beyond its function")
                return
            site.refusals["stack"].add("the list is kept in an attribute")
            for cls in classes:
                self.walk(site, self.node("attribute", cls, None, owner_attribute, node.element), depth + 1)
            return
        if re.search(r"^" + re.escape(node.name) + r" =", text):
            return
        call = self.enclosing_call(text, start)
        if call:
            callee_name, position, receiver = call
            if callee_name in ("append", "prepend", "insert", "set_at"):
                site.refusals["inline"].add("the list is put in another list")
                site.refusals["stack"].add("the list is put in another list")
                return
            if callee_name in ("Parallel", "Concurrent"):
                site.refusals["stack"].add("the list is handed to a task")
                return
            targets = [f for f in self.program.functions_by_name.get(callee_name, [])
                       if position < len(f.parameters)]
            if receiver is None:
                same = [f for f in targets if f.owner is function.owner]
                targets = same or [f for f in targets if f.name == f.owner.name]
            if not targets:
                site.refusals["inline"].add("the list is passed to '%s', which the study cannot resolve" % callee_name)
                site.refusals["stack"].add("the list is passed to code the study cannot follow")
                return
            for target in targets:
                if target.owner in self.program.library and target.owner.name in ("Console", "String"):
                    continue
                parameter, kind = target.parameters[position]
                if not element_of(kind):
                    if kind.startswith("$") or kind in ("Anything",):
                        site.refusals["inline"].add("the list is passed as a generic value to '%s'" % callee_name)
                        site.refusals["stack"].add("the list is passed as a generic value")
                    continue
                self.walk(site, self.node("local", target.owner, target, parameter, node.element), depth + 1)
            return
        if "{" in before or "\"" in text and "{" + node.name in text:
            return
        if re.search(r"(==|!=|<|>)\s*$", before) or after.startswith(("==", "!=")):
            return
        if before.endswith(("crash", "assert", "if", "not")):
            return
        if before.endswith(":") or re.search(r"\{[^}]*:\s*$", before):
            site.refusals["inline"].add("the list is put in an object literal")
            site.refusals["stack"].add("the list is put in an object literal")
            return

    def enclosing_call(self, text, position):
        depth = 0
        for index in range(position - 1, -1, -1):
            character = text[index]
            if character in ")]}":
                depth += 1
            elif character in "([{":
                if depth == 0:
                    if character != "(":
                        return None
                    head = re.search(r"((?:" + IDENT + r"\.)*)(" + IDENT + r")(?:<[^()]*>)?$", text[:index])
                    if not head:
                        return None
                    inner = text[index + 1:position]
                    argument_position = len(split_arguments(inner + "x")) - 1
                    receiver = head.group(1) or None
                    return head.group(2), argument_position, receiver
                depth -= 1
        return None

    def flows_to_callers(self, site, function, depth):
        site.refusals["stack"].add("the list is returned")
        callers = self.call_sites(function)
        if not callers:
            return
        for caller, index, _ in callers:
            text = caller.body[index][2]
            declared = re.match(r"^var (" + IDENT + r")(?:: [^=]+)? = ", text)
            if declared:
                self.walk(site, self.node("local", caller.owner, caller, declared.group(1), site.node.element), depth + 1)
            elif re.match(r"^return ", text):
                self.flows_to_callers(site, caller, depth + 1) if depth < 6 else None
            elif re.match(r"^(" + IDENT + r") = ", text):
                target = re.match(r"^(" + IDENT + r") = ", text).group(1)
                if target in caller.owner.attributes:
                    self.walk(site, self.node("attribute", caller.owner, None, target, site.node.element), depth + 1)
                elif target in caller.locals:
                    self.walk(site, self.node("local", caller.owner, caller, target, site.node.element), depth + 1)

    # ---------- loops ----------
    def maybe_loop(self, site, node, function, index):
        """A while whose body indexes the list: record it once, with the shape of its body."""
        position = index
        indent = function.body[index][1]
        while position >= 0:
            number, line_indent, text = function.body[position]
            if line_indent < indent and text.startswith("while "):
                break
            if line_indent < indent:
                indent = line_indent
            position -= 1
        if position < 0:
            return
        key = (function.owner.path, function.body[position][0])
        if any(loop["key"] == key for loop in self.loops):
            return
        loop_indent = function.body[position][1]
        body = []
        for later in range(position + 1, len(function.body)):
            if function.body[later][1] <= loop_indent:
                break
            body.append(function.body[later][2])
        calls = [b for b in body if re.search(IDENT + r"\(", b) and not re.search(r"\.(count|is_empty)\(\)", b)]
        calls = [b for b in calls if not re.fullmatch(r".*\.(count|is_empty|get_at|abs|minimum|maximum)\(.*", b)]
        branches = [b for b in body if b.startswith(("if ", "} else"))]
        exits = [b for b in body if b.startswith(("return", "break", "crash", "assert"))]
        counter = re.match(r"^while (" + IDENT + r") ", function.body[position][2])
        counter = counter.group(1) if counter else ""
        reduction = any(re.search(r"^(" + IDENT + r") = \1 [+*] ", b) and not b.startswith(counter + " = ")
                        for b in body)
        self.loops.append({
            "key": key, "site": site, "kind": "while", "function": function, "lines": len(body),
            "calls": len(calls), "branches": len(branches), "exits": len(exits), "reduction": reduction,
            "effects": any(ORDERED_EFFECT.search(b) for b in body), "grows": any(
                re.search(r"\.(append|prepend|insert)\(", b) for b in body)})

    def template_loop(self, site, node, function, index, member, text):
        element_class = self.program.classes.get(last_segment(site.node.element))
        target = member.split("_", 1)[1] if "_" in member else ""
        if member.startswith("parallel_each_"):
            target = member[len("parallel_each_"):]
        lines, effects, shared, has_loop = 0, False, False, False
        if element_class and target in element_class.functions:
            reached = self.reach(element_class, element_class.functions[target], set())
            for function_reached in reached:
                lines += len(function_reached.body)
                for _, _, body_text in function_reached.body:
                    if body_text.startswith("while "):
                        has_loop = True
                    if ORDERED_EFFECT.search(body_text):
                        effects = True
                    if re.search(r"(?<![\w.$])(" + "|".join(
                            re.escape(a) for a, (d, i, _) in element_class.attributes.items()
                            if not self.attribute_plain(element_class, a)) + r")\b", body_text) and \
                            any(not self.attribute_plain(element_class, a) for a in element_class.attributes):
                        shared = True
        self.loops.append({
            "key": (function.owner.path, function.body[index][0], member), "site": site, "kind": "template",
            "member": member, "function": function, "lines": lines, "calls": 0, "branches": 0, "exits": 0,
            "reduction": member.startswith("sum_"), "effects": effects, "shared": shared, "grows": False,
            "has_loop": has_loop,
            "parallel_already": member.startswith("parallel_each_")})

    def attribute_plain(self, cls, name):
        declared, initializer, _ = cls.attributes[name]
        kind = declared or self.program.infer(initializer or "", cls, None)
        if not kind:
            return False
        base = kind.rstrip("?")
        other = self.program.classes.get(last_segment(base))
        return self.program.is_plain(base) or base == "String" or (other is not None and other.singleton)

    def reach(self, cls, function, seen):
        if function in seen or len(seen) > 40:
            return seen
        seen.add(function)
        for _, _, text in function.body:
            for called in re.findall(r"(?<![\w.$])(" + IDENT + r")\(", text):
                if called in cls.functions:
                    self.reach(cls, cls.functions[called], seen)
        return seen

    def passed_function(self, site, node, function, index, member, arguments, depth):
        if site.plain:
            if site.numeric:
                self.loops.append({
                    "key": (function.owner.path, function.body[index][0], member), "site": site, "kind": "passed",
                    "member": member, "function": function, "lines": 1, "calls": 1, "branches": 0, "exits": 0,
                    "reduction": member == "sum", "effects": False, "grows": False})
            return
        if site.text_items:
            return
        site.refusals["inline"].add("items are passed to a function ('%s(f)')" % member)


def summarise(program, study, show_sites):
    everything = study.sites
    picked = [s for s in everything if s.container in ("Vector", "Items")]
    if picked:
        print("== %s: %d hand-picked Vector/Items sites; as one List, the study would prove inline for:" % (
            program.name, len(picked)))
        for site in picked:
            flat = site.plain or site.text_items
            inline = flat or (site.fits and not site.refusals["inline"])
            print("   %s:%d %s<%s> -> %s%s" % (
                os.path.relpath(site.node.cls.path), site.line, site.container, site.node.element,
                "flat values" if flat else ("inline" if inline else "references"),
                "" if inline else "  [" + "; ".join(sorted(site.refusals["inline"])[:4]) + "]"))
    sites = [s for s in everything if s.container == "List"]
    study = type("View", (), {"sites": sites, "loops": [l for l in study.loops if l["site"].container == "List"]})
    total = len(sites)
    if total == 0:
        print("%s: no List sites" % program.name)
        return {}
    kinds = Counter()
    layouts = Counter()
    reasons = defaultdict(Counter)
    for site in sites:
        if site.plain:
            kinds["plain values"] += 1
        elif site.text_items:
            kinds["text"] += 1
        elif site.fits:
            kinds["class that fits inline"] += 1
        else:
            kinds["class that does not fit"] += 1
        inline = not site.refusals["inline"]
        class_inline = inline and not site.plain and not site.text_items
        stack = not site.refusals["stack"]
        simd = site.numeric
        if class_inline:
            layouts["inline objects (new)"] += 1
        if site.plain or site.text_items:
            layouts["values already flat"] += 1
        if stack:
            layouts["frame array"] += 1
        if simd:
            layouts["numbers (SIMD candidates)"] += 1
        for layout, why in site.refusals.items():
            for reason in why:
                reasons[layout][reason] += 1
        if show_sites:
            verdict = []
            verdict.append("inline" if class_inline else ("flat values" if (site.plain or site.text_items) else "references"))
            if stack:
                verdict.append("frame")
            print("  %s:%d %s %s List<%s> -> %s%s" % (
                os.path.relpath(site.node.cls.path), site.line, site.node.kind, site.node.name, site.node.element,
                ", ".join(verdict),
                "" if class_inline or site.plain or site.text_items else "  [" + "; ".join(sorted(site.refusals["inline"])[:3]) + "]"))
    classes = sum(1 for s in sites if not s.plain and not s.text_items)
    fitting = sum(1 for s in sites if not s.plain and not s.text_items and s.fits)
    print("== %s: %d List sites (%d attributes, %d locals)" % (
        program.name, total, sum(1 for s in sites if s.node.kind == "attribute"),
        sum(1 for s in sites if s.node.kind == "local")))
    for kind, count in kinds.most_common():
        print("   items: %-28s %5d  %5.1f%%" % (kind, count, 100.0 * count / total))
    for layout, count in layouts.most_common():
        print("   layout: %-27s %5d  %5.1f%%" % (layout, count, 100.0 * count / total))
    if classes:
        print("   of %d lists of objects: %d fit inline by class, %d proven inline (%.1f%%)" % (
            classes, fitting, layouts["inline objects (new)"], 100.0 * layouts["inline objects (new)"] / classes))
    object_sites = [s for s in sites if not s.plain and not s.text_items and s.fits]
    for layout in ("inline", "stack"):
        pool = object_sites if layout == "inline" else sites
        counter = Counter()
        for site in pool:
            for reason in site.refusals[layout]:
                counter[reason] += 1
        if counter:
            print("   why not %s (%s):" % (layout, "lists of objects that fit" if layout == "inline" else "all sites"))
            for reason, count in counter.most_common(8):
                print("     %5d  %s" % (count, reason))
    kept = sum(1 for s in object_sites if s.kept_past_resize)
    if object_sites:
        print("   of the lists of objects that fit: %d read an item after the list changes size (D204 would refuse)" % kept)
    loops = study.loops
    if loops:
        loop_kinds = Counter(l["kind"] for l in loops)
        print("   loops over lists: %d (%s)" % (len(loops), ", ".join("%s %d" % kv for kv in loop_kinds.items())))
        numeric_loops = [l for l in loops if l["site"].numeric]
        simple = [l for l in numeric_loops if l["kind"] == "while" and l["calls"] == 0 and l["exits"] == 0]
        float_reductions = [l for l in simple if l["reduction"] and last_segment(l["site"].node.element) in ("Float", "Double")]
        print("   loops over numbers: %d; with no call and no early exit (vectorisable shape): %d; of those float reductions (not vectorised under strict maths): %d" % (
            len(numeric_loops), len(simple), len(float_reductions)))
        member_loops = [l for l in loops if l["kind"] == "template" and l.get("member", "").startswith(("each_", "parallel_each_"))]
        if member_loops:
            distinct = [l for l in member_loops if not l["site"].refusals["parallel"]]
            clean = [l for l in distinct if not l["effects"] and not l.get("shared")]
            heavy = [l for l in clean if l["lines"] >= 8 or l.get("has_loop")]
            print("   each_/parallel_each_ loops: %d; items proven distinct: %d; no ordered effect and only own values: %d; heavy body (a loop, or 8+ lines): %d; already parallel_each_: %d" % (
                len(member_loops), len(distinct), len(clean), len(heavy),
                sum(1 for l in member_loops if l.get("parallel_already"))))
    if "--loops" in sys.argv:
        for l in loops:
            print("   loop %s:%s %s List<%s> %s lines=%d calls=%d exits=%d effects=%s red=%s" % (
                os.path.relpath(l["function"].owner.path), l["key"][1], l["kind"], l["site"].node.element,
                l.get("member", ""), l["lines"], l["calls"], l["exits"], l["effects"], l["reduction"]))
    return {"sites": total, "kinds": kinds, "layouts": layouts, "classes": classes, "fitting": fitting}


def main():
    arguments = sys.argv[1:]
    show = "--sites" in arguments
    arguments = [a for a in arguments if a not in ("--sites", "--loops")]
    library = "library"
    if "--library" in arguments:
        position = arguments.index("--library")
        library = arguments[position + 1]
        del arguments[position:position + 2]
    each = "--each" in arguments
    arguments = [a for a in arguments if a != "--each"]
    name, folders = arguments[0], arguments[1:]
    if folders == ["library"]:
        program = Program(name, [], library)
        program.own = program.library
        studies = [(program, Study(program))]
    elif each:
        studies = []
        for parent in folders:
            for child in sorted(os.listdir(parent)):
                path = os.path.join(parent, child)
                if os.path.isdir(path) and any(True for _ in spite_files(path)):
                    program = Program(child, [path], library)
                    studies.append((program, Study(program)))
    else:
        program = Program(name, folders, library)
        studies = [(program, Study(program))]
    combined = Study(studies[0][0])
    for program, study in studies:
        study.run()
        combined.sites += study.sites
        combined.loops += study.loops
    combined.program.name = "%s (%d programs)" % (name, len(studies)) if len(studies) > 1 else name
    summarise(combined.program, combined, show)


if __name__ == "__main__":
    main()
