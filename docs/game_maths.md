# Game maths

The standard library has the maths a game engine is built on: `Vector2`, `Vector3` and `Vector4`, `Matrix3` and
`Matrix4`, and `Quaternion`, with the boxes, planes, frustums and rays that culling and picking need,
`Color` with the web's colour formats, and the curves, easings, noise and half-precision floats that animation and
rendering need. They are plain classes in `library/` holding numbers and
nothing else, so a list of them fits a [`Vector<T>`](collections.md) column and a binary writer, and a program that
uses none of them carries none of their code. They are built on the number classes' own
maths (`square_root()`, `sine()`, `arc_cosine()`, see [values_and_types.md](values_and_types.md#maths-functions))
and every name is spelled out in full, except the type names themselves.

## Vectors

A vector is made with its parts and read through them: `x`, `y`, `z` and `w`, the axis names
([style.md](style.md#names)). The operators are its functions ([functions_and_operators.md](functions_and_operators.md#every-operator-is-a-function)),
each answering a new vector: `+` is `sum`, `-` `subtract`, `*` and `/` work part by part, unary `-` negates and
`==` compares every part. Scaling by a number is `scaled(factor)`, since one name is one function and `*` already
takes a vector.

A vector holds any number class: `Vector2`, `Vector3` and `Vector4` are generic over a class that fits
[`Number`](values_and_types.md#every-number-fits-number), taken from the parts it is made with, so
`Vector3(1.0, 2.0, 2.0)` is a `Vector3<Float>`, `Vector2(5, 1)` a `Vector2<Integer>`, and a `Vector3<Double>` keeps
`Double`'s precision. There is no `Vector3i` or `Vector3d`: the class is written out where a type is named,
`func move(by: Vector3<Float>)`, and each class is its own packed copy of the code.

```gdscript title=game_vectors_basics/game_vectors_basics.spite entry
var console = Console()

func GameVectorsBasics() {
    var position = Vector3(1.0, 2.0, 2.0)
    var velocity = Vector3(0.5, 0.0, -1.0)
    var moved = velocity.scaled(2.0)
    var next = position + moved
    console.print("{next} {position.length()} {position.normalized()}")
    var up = Vector3(0.0, 1.0, 0.0)
    console.print("{position.dot(up)} {position.cross(up)} {position.distance_to(next)}")
    console.print("{position.linear_interpolate(next, 0.5)} {next.minimum(position)} {velocity.absolute()}")
}
```
```output
(2, 2, 0) 3 (0.33333334, 0.6666667, 0.6666667)
2 (-2, 0, 1) 2.236068
(1.5, 2, 1) (1, 2, 0) (0.5, 0, 1)
```

Every member answers in the vector's own class. On whole numbers that truncates exactly as integer division does:
`Vector2(5, 1).length()` is `5` and `Vector2(1, 1).length()` is `1`, so grid code compares `length_squared()`,
which is exact, and `normalized()` snaps toward a grid direction; a part shorter than the whole length becomes `0`,
so `Vector3(3, 4, 12).normalized()` is `(0, 0, 0)`:

```gdscript title=game_grid_vectors/game_grid_vectors.spite entry
var console = Console()

func GameGridVectors() {
    var step = Vector2(5, 1)
    var diagonal = Vector2(1, 1)
    var direction = step.normalized()
    var step_length = step.length()
    var diagonal_length = diagonal.length()
    var diagonal_squared = diagonal.length_squared()
    console.print(direction, step_length, diagonal_length, diagonal_squared)
    var precise = Vector3<Double>(1.0, 2.0, 2.0)
    var unit = precise.normalized()
    console.print(unit)
}
```
```output
(1, 0) 5 1 2
(0.3333333333333333, 0.6666666666666666, 0.6666666666666666)
```

A constructor call is never an argument and neither is a call, so a chain of vector maths
is written a step per line, each step named: `var moved = velocity.scaled(2.0)` then `var next = position +
moved`. Operators chain freely, `first + second - third`, because an operator is not an argument.

## Matrices

`Matrix4<Float>()` is the identity. Its sixteen parts are `column_0_row_0` to `column_3_row_3`, **column-major**: the
four parts of a column sit next to each other in memory, as Vulkan, OpenGL and glTF expect, and a point is a
column the matrix multiplies from the left. `a * b` is `a` applied after `b`: with `var both = a * b`, `both.transform_point(point)` is
`a.transform_point(b.transform_point(point))`. A matrix is set to a transform in place, the usual
way: `set_translation(offset)`, `set_scale(factor)`, `set_rotation(rotation)`, `set_transform(translation,
rotation, scale)` (scale first, then rotation, then translation), `set_look_at(eye, target, up)`,
`set_perspective(field_of_view, aspect, near, far)` and `set_orthographic(left, right, bottom, top, near, far)`.
The projections are **Vulkan's clip space**: the camera looks down -z, y points down the screen, and depth runs
from 0 at `near` to 1 at `far`.

```gdscript title=game_matrices_basics/game_matrices_basics.spite entry
var console = Console()

func GameMatricesBasics() {
    var model = Matrix4<Float>()
    var place = Vector3(1.0, 2.0, 3.0)
    var turn = Quaternion<Float>()
    var size = Vector3(2.0, 2.0, 2.0)
    model.set_transform(place, turn, size)
    var corner = Vector3(1.0, 1.0, 1.0)
    console.print("{model.transform_point(corner)} {model.transform_direction(corner)} {model.determinant()}")
    var inverse = model.inverse()
    crash inverse
    var back = inverse.transform_point(corner)
    console.print("{back} {model.translation_part()} {model.scale_part()}")
    var projection = Matrix4<Float>()
    projection.set_orthographic(-2.0, 2.0, -1.0, 1.0, 1.0, 3.0)
    var seen = Vector3(2.0, 1.0, -1.0)
    var projected = projection.project_point(seen)
    console.print(projected)
}
```
```output
(3, 4, 5) (2, 2, 2) 8
(0, -0.5, -1) (1, 2, 3) (2, 2, 2)
(1, -1, 0)
```

`inverse()` answers `Matrix4?`: a matrix that flattens space has none, and that is a value to handle, not a crash.
`translation_part()`, `scale_part()` and `rotation_part()` take a transform apart again,
`transform(vector)` multiplies a `Vector4`, `project_point(point)` divides by the `w` it makes, and
`normal_matrix()` is the `Matrix3` that turns normals under a scaled model. `Matrix3` has the same shape for three
dimensions: `multiply`, `transform`, `transposed`, `determinant`, `inverse` and `set_rotation`.

A matrix and a quaternion are generic over their number class, as a vector is, and nothing is inferred from an
empty constructor, so the class is always written: `Matrix4<Float>()` for what a GPU reads, `Matrix4<Double>()`
where a world is too large for `Float` (a millimetre a thousand kilometres from the origin is lost in a `Float` and
kept in a `Double`). The two never mix: a `Matrix4<Double>` takes a `Vector3<Double>` and a `Quaternion<Double>`.
Only `Float` and `Double` are offered, since a rotation of whole numbers is not a rotation: `Quaternion<Integer>()` is
a compile error.

```gdscript title=double_precision/double_precision.spite entry
var console = Console()

func DoublePrecision() {
    var world = Matrix4<Double>()
    var far_away = Vector3<Double>(1000000.0, 0.0, 0.0)
    world.translation = far_away
    var millimetre = Vector3<Double>(0.001, 0.0, 0.0)
    var placed = world.transform_point(millimetre)
    var kept = placed.x - 1000000.0
    console.print("kept {kept > 0.0009 and kept < 0.0011}")
}
```
```output
kept true
```

## Rotations

A `Quaternion<Float>()` is no rotation. `set_axis_angle(axis, angle)` turns by `angle` radians about `axis`;
`set_euler(angles, order)` turns about x, y and z by the parts of `angles`, applied in the order named: `'xyz'`
turns about x first, then y, then z, as Blender's XYZ Euler does; the other orders are `'xzy'`, `'yxz'`, `'yzx'`,
`'zxy'` and `'zyx'`. `rotate(vector)` turns a vector, `a * b` is the rotation `b` then `a`,
`spherical_interpolate(target, amount)` and `normalized_interpolate(target, amount)` blend two rotations along the
shorter way, and `to_matrix()` is the same rotation as a `Matrix4`.

```gdscript title=game_rotations_basics/game_rotations_basics.spite entry
var console = Console()

func GameRotationsBasics() {
    var turn = Quaternion<Float>()
    var axis = Vector3(0.0, 0.0, 1.0)
    var quarter_turn = Float.pi * 0.5
    turn.set_axis_angle(axis, quarter_turn)
    var along = Vector3(1.0, 0.0, 0.0)
    var turned = turn.rotate(along)
    console.print("{turned.x.absolute() < 0.001} {turned.y > 0.999}")
    var still = Quaternion<Float>()
    var halfway = still.spherical_interpolate(turn, 0.5)
    var diagonal = halfway.rotate(along)
    console.print("{diagonal.x > 0.707} {diagonal.y > 0.707} {still}")
}
```
```output
true true
true true (0, 0, 0, 1)
```

## Boxes, frustums and rays

`AxisAlignedBox(lowest, highest)` is a box along the axes: `contains(point)`, `intersects(other)`,
`united(other)`, `including(point)`, `center()`, `size()`, and `transformed(matrix)`, the box that holds the
turned and moved one. A `Plane()` is set with `set_point_normal(point, normal)` or `set_parts(x, y, z,
distance)`, and `signed_distance_to(point)` says how far in front of it a point is. A `Frustum()` is set from a
camera's view-projection matrix and culls: `contains_point`, `intersects_sphere(center, radius)` and
`intersects_box(box)`. A `Ray(origin, direction)` picks: `hit_plane(plane)`, `hit_box(box)` and
`hit_triangle(first, second, third)` answer how far along the ray the hit is, as a `Float?` that is `null` when
there is none: a miss is an answer, not a failure.

```gdscript title=game_picking_basics/game_picking_basics.spite entry
var console = Console()

func GamePickingBasics() {
    var low = Vector3(-1.0, -1.0, -1.0)
    var high = Vector3(1.0, 1.0, 1.0)
    var box = AxisAlignedBox(low, high)
    var origin = Vector3(0.0, 0.0, 5.0)
    var forward = Vector3(0.0, 0.0, -1.0)
    var ray = Ray(origin, forward)
    var hit = ray.hit_box(box)
    if hit {
        console.print("hit {hit} away, at {ray.point_at(hit)}")
    }
    var backward = Vector3(0.0, 0.0, 1.0)
    var away = Ray(origin, backward)
    var missed = away.hit_box(box)
    if missed {
        console.print("hit behind")
    } else {
        console.print("nothing behind")
    }
}
```
```output
hit 4 away, at (0, 0, 1)
nothing behind
```

## Colours

`Color(red, green, blue, alpha)` holds four `Float`s from 0 to 1. `ColorText()` reads the colours the web writes
(`#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa`, `rgb()`, `rgba()`, `hsl()` and `hsla()` with commas or spaces and an
alpha after a `/`, and the 148 CSS colour names) and answers `null` for text that is none of them.
A colour writes itself back as `to_hex()`, `to_rgb_text()` and `to_hsl_text()`, and
`to_linear()` and `to_standard_rgb()` convert between the sRGB a picker or a texture holds and the linear light a
shader adds up.

```gdscript title=game_colors_basics/game_colors_basics.spite entry
var console = Console()
var color_text = ColorText()

func GameColorsBasics() {
    var tomato = color_text.read("tomato")
    crash tomato
    console.print("{tomato} {tomato.to_rgb_text()} {tomato.to_hsl_text()}")
    var faded = color_text.read("hsl(210, 50%, 40%, 0.25)")
    crash faded
    console.print("{faded} {faded.to_rgb_text()}")
    var wrong = color_text.read("#12")
    if wrong {
        console.print("read")
    } else {
        console.print("not a colour")
    }
    var linear = tomato.to_linear()
    var back = linear.to_standard_rgb()
    console.print("{linear} back to {back}")
}
```
```output
#ff6347 rgb(255, 99, 71) hsl(9.1, 100%, 63.9%)
#33669940 rgba(51, 102, 153, 0.25)
not a colour
#ff2010 back to #ff6347
```

## Curves, easing and noise

`CubicBezier(start, first_handle, second_handle, end)` is a curve of four `Vector2` points: `point_at(amount)`
walks it, and `y_at_x(x)` answers its height where it passes `x`, which is how an animation curve in Blender
(an F-curve) is read. `Easing()` is a singleton of the usual easing curves, each taking an amount from 0 to 1:
`smooth_step`, `smoother_step`, `in_quadratic`, `out_quadratic`, `in_out_quadratic`, the same for `cubic` and
`sine`, `in_exponential`, `out_exponential`, `in_back`, `out_back`, `out_elastic` and `out_bounce`.
`Noise(seed)` answers smooth noise from -1 to 1 (`value_2d`, `value_3d`, `gradient_2d`, `gradient_3d`), the
same for the same seed on every machine, and `interleaved_gradient(pixel_x, pixel_y)`, the 0 to 1 dither a
shader uses.

```gdscript title=game_curves_basics/game_curves_basics.spite entry
var console = Console()
var easing = Easing()

func GameCurvesBasics() {
    var start = Vector2(0.0, 0.0)
    var first_handle = Vector2(0.5, 0.0)
    var second_handle = Vector2(0.5, 1.0)
    var end = Vector2(1.0, 1.0)
    var curve = CubicBezier(start, first_handle, second_handle, end)
    console.print("{curve.y_at_x(0.5)} {easing.smooth_step(0.25)} {easing.in_out_cubic(0.5)}")
    var noise = Noise(7)
    var sample = noise.gradient_2d(0.5, 0.5)
    console.print("{sample >= -1.5 and sample <= 1.5} {noise.gradient_2d(3.0, 4.0)}")
}
```
```output
0.5 0.15625 0.5
true 0
```

## Half precision

A GPU often takes a 16-bit float. `value.to_half_precision()` turns a `Float` into its 16 bits, as an
`UnsignedShort`, rounding to the nearest and to even on a tie, as the hardware does; `bits.half_precision_to_float()`
turns them back. Past 65504 is infinity, below about 6e-8 is zero, and not-a-number stays not-a-number.

```gdscript title=half_precision_basics/half_precision_basics.spite entry
var console = Console()

func HalfPrecisionBasics() {
    var tenth: Float = 0.1
    var packed = tenth.to_half_precision()
    var unpacked = packed.half_precision_to_float()
    var huge: Float = 100000.0
    var one: Float = 1.0
    console.print("{packed} {unpacked} {huge.to_half_precision()} {one.bits()}")
}
```
```output
11878 0.099975586 31744 1065353216
```

## What they cost

Every function that answers a vector, a matrix or a quaternion answers a new one, and these are classes: a class
instance is a reference-counted object ([memory.md](memory.md)), passed by reference. What
it costs depends on where the answer goes. When it stays in the function that asked for it (read, handed to
functions that keep nothing, given a new value, or returned) the compiler keeps it in that function's frame and
the function that made it writes it straight there
([optimizations.md](optimizations.md#objects-that-never-leave-their-function-live-in-the-frame)), so
`position = position + velocity.scaled(delta)` in a loop allocates nothing, and neither do `a + b + c` or
`var offset = first - second` read only by `offset.length()`. An answer that is stored (in an attribute, a list, a `Vector<T>` column) or printed
is an object on the heap, one allocation, as any object is.

[Its benchmark](../benchmarks/game_maths/) measures it against the same passes written in C with plain structs: a
million `position + velocity.scaled(delta)` steps, 200 000 `Matrix4` products and a million `transform_point`s take
7.2 ms in all, against 2.3 ms in plain C and 1.8 ms in C tuned by hand. The vector steps and the transforms live in
the frame; a product assigned back over its own operand, `accumulated = accumulated * step_matrix`, is made on the
heap each time, and that is most of the difference.

---

Next: [Foreign libraries and operating systems](foreign_libraries.md), calling C libraries and reaching the operating system.
