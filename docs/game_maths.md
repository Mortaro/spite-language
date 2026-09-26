# Game maths

The standard library has the maths a game engine is built on: `Vector2`, `Vector3` and `Vector4`, `Matrix3` and
`Matrix4`, and `Quaternion`, with the boxes, planes, frustums and rays that culling and picking need
([D213](decisions.md)). They are plain classes in `library/` holding `Float`s and
nothing else, so a list of them fits a [`Vector<T>`](collections.md) column and a binary writer, and a program that
uses none of them carries none of their code ([D177](decisions.md)). They are built on the number classes' own
maths -- `square_root()`, `sine()`, `arc_cosine()` ([values_and_types.md](values_and_types.md#maths-functions)) --
and every name is spelled out in full, except the type names Mortaro gave. The names are proposed by Claude,
unconfirmed ([D214](decisions.md)).

## Vectors

A vector is made with its parts and read through them: `x_value`, `y_value`, `z_value` and `w_value` (a name is
never one letter). The operators are its functions ([functions_and_operators.md](functions_and_operators.md#every-operator-is-a-function)),
each answering a new vector: `+` is `sum`, `-` `subtract`, `*` and `/` work part by part, unary `-` negates and
`==` compares every part. Scaling by a number is `scaled(factor)`, since one name is one function and `*` already
takes a vector.

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

A constructor call is never an argument ([D202](decisions.md)) and neither is a call, so a chain of vector maths
is written a step per line, each step named: `var moved = velocity.scaled(2.0)` then `var next = position +
moved`. Operators chain freely, `first + second - third`, because an operator is not an argument.

## Matrices

`Matrix4()` is the identity. Its sixteen parts are `column_0_row_0` to `column_3_row_3`, **column-major**: the
four parts of a column sit next to each other in memory, as Vulkan, OpenGL and glTF expect, and a point is a
column the matrix multiplies from the left. `a * b` is `a` applied after `b`: `(a * b).transform_point(point)` is
`a.transform_point(b.transform_point(point))`. A matrix is set to a transform in place, the way SlopEngine already
writes it: `set_translation(offset)`, `set_scale(factor)`, `set_rotation(rotation)`, `set_transform(translation,
rotation, scale)` (scale first, then rotation, then translation), `set_look_at(eye, target, up)`,
`set_perspective(field_of_view, aspect, near, far)` and `set_orthographic(left, right, bottom, top, near, far)`.
The projections are **Vulkan's clip space**: the camera looks down -z, y points down the screen, and depth runs
from 0 at `near` to 1 at `far`.

```gdscript title=game_matrices_basics/game_matrices_basics.spite entry
var console = Console()

func GameMatricesBasics() {
    var model = Matrix4()
    var place = Vector3(1.0, 2.0, 3.0)
    var turn = Quaternion()
    var size = Vector3(2.0, 2.0, 2.0)
    model.set_transform(place, turn, size)
    var corner = Vector3(1.0, 1.0, 1.0)
    console.print("{model.transform_point(corner)} {model.transform_direction(corner)} {model.determinant()}")
    var inverse = model.inverse()
    crash inverse
    var back = inverse.transform_point(corner)
    console.print("{back} {model.translation_part()} {model.scale_part()}")
    var projection = Matrix4()
    projection.set_orthographic(-2.0, 2.0, -1.0, 1.0, 1.0, 3.0)
    var seen = Vector3(2.0, 1.0, -1.0)
    console.print("{projection.project_point(seen)}")
}
```
```output
(3, 4, 5) (2, 2, 2) 8
(0, -0.5, -1) (1, 2, 3) (2, 2, 2)
(1, -1, 0)
```

`inverse()` answers `Matrix4?`: a matrix that flattens space has none, and that is a value to handle, not a crash
([D199](decisions.md)). `translation_part()`, `scale_part()` and `rotation_part()` take a transform apart again,
`transform(vector)` multiplies a `Vector4`, `project_point(point)` divides by the `w` it makes, and
`normal_matrix()` is the `Matrix3` that turns normals under a scaled model. `Matrix3` has the same shape for three
dimensions: `multiply`, `transform`, `transposed`, `determinant`, `inverse` and `set_rotation`.

## Rotations

A `Quaternion()` is no rotation. `set_axis_angle(axis, angle)` turns by `angle` radians about `axis`;
`set_euler(angles, order)` turns about x, y and z by the parts of `angles`, applied in the order named: `'xyz'`
turns about x first, then y, then z, as Blender's XYZ Euler does; the other orders are `'xzy'`, `'yxz'`, `'yzx'`,
`'zxy'` and `'zyx'`. `rotate(vector)` turns a vector, `a * b` is the rotation `b` then `a`,
`spherical_interpolate(target, amount)` and `normalized_interpolate(target, amount)` blend two rotations along the
shorter way, and `to_matrix()` is the same rotation as a `Matrix4`.

```gdscript title=game_rotations_basics/game_rotations_basics.spite entry
var console = Console()

func GameRotationsBasics() {
    var turn = Quaternion()
    var axis = Vector3(0.0, 0.0, 1.0)
    var quarter_turn = Float.pi() * 0.5
    turn.set_axis_angle(axis, quarter_turn)
    var along = Vector3(1.0, 0.0, 0.0)
    var turned = turn.rotate(along)
    console.print("{turned.x_value.absolute() < 0.001} {turned.y_value > 0.999}")
    var still = Quaternion()
    var halfway = still.spherical_interpolate(turn, 0.5)
    var diagonal = halfway.rotate(along)
    console.print("{diagonal.x_value > 0.707} {diagonal.y_value > 0.707} {still}")
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
there is none -- a miss is an answer, not a failure ([D199](decisions.md)).

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

## What they cost

Every function that answers a vector, a matrix or a quaternion makes a new one, and **a new one is an allocation**:
these are classes, and a class instance is a reference-counted object on the heap ([memory.md](memory.md)). So
`position + velocity.scaled(delta)` allocates twice and frees the old `position` once. `benchmarks/game_maths`
measures it: a million such steps take about 26 ms with `clang -O2` on Mortaro's machine, two allocations each, and
200 000 `Matrix4` products take about 6 ms, one allocation each; setting a matrix in place (`set_perspective`,
`set_transform`, ...) allocates nothing. What would remove the allocations is a question for Mortaro
(`mortaros_missing_decisions.md`), not built: placing a result that never outlives its statement or its loop pass
in the frame, as [D108](decisions.md)'s placement already does for buffers; or letting these classes be kept
inline like numbers, which [D149](decisions.md)'s reference semantics forbid today.

## Rules in full

The normative rules for this part of the language, in full. A `D` number is a row of the
[decision log](decisions.md).

### Game maths  **[implemented]**

D213 (decided by Mortaro): the standard library fills every maths gap a game needs, as SlopEngine asks for it.
Everything below is proposed by Claude, unconfirmed: the names ([D214](decisions.md): Mortaro picks names later),
the layout and the conventions.

- **Plain classes of `Float`s** in `library/` (`vector2.spite`, `vector3.spite`, `vector4.spite`, `matrix3.spite`,
  `matrix4.spite`, `quaternion.spite`), with no attribute but their parts, so each fits `Vector<T>` (D204) and a
  binary writer (D208). Tree-shaken: a program that names none of them has none of their code, and a program that
  uses `Vector3` carries only the functions it calls (D177). Nothing runs at start-up and nothing is registered.
- **Parts.** Vectors and quaternions: `x_value`, `y_value`, `z_value`, `w_value`, since a name is never one letter
  ([style.md](style.md); SlopEngine's `Vector3` already spells them so). Matrices: `column_C_row_R`,
  column-major, `Matrix4()` and `Matrix3()` the identity. A vector is made with all its parts
  (`Vector3(1.0, 2.0, 3.0)`); a quaternion and a matrix are made from their defaults and set.
- **Vectors** (`Vector2`, `Vector3`, `Vector4`): `sum`, `subtract`, `multiply`, `divide` (part by part, and so the
  operators `+ - * /`), `negate` (unary `-`), `equals` (`==`, every part exactly), `scaled(factor)`,
  `dot(other)`, `cross(other)` (`Vector3` only), `length()`, `length_squared()`, `normalized()` (the zero vector
  stays zero rather than becoming not-a-number), `distance_to(other)`, `distance_squared_to(other)`,
  `linear_interpolate(target, amount)` (`amount` 0 is this vector, 1 is `target`, not clamped), `minimum(other)`,
  `maximum(other)` and `absolute()` part by part (the number classes' rules, so a not-a-number part is ignored by
  `minimum` and `maximum`), and `to_string()` as `(1, 2, 3)`.
- **`Matrix4`**: `set_identity()`, `multiply(other)` (`*`: `a * b` applies `b` first), `equals(other)`,
  `transform(vector: Vector4)`, `transform_point(point)` (w taken as 1, no divide), `project_point(point)` (divided
  by the w it makes), `transform_direction(direction)` (w taken as 0), `transposed()`, `determinant()`,
  `inverse(): Matrix4?` (`null` when the determinant is exactly 0), `set_translation`, `set_scale`, `set_rotation`,
  `set_transform(translation, rotation, scale)` (translation times rotation times scale), `translation_part()`,
  `scale_part()` (the lengths of the first three columns, the x one negative when the matrix mirrors),
  `rotation_part()` (after dividing the scale out; no rotation when a scale part is 0), `set_look_at(eye, target,
  up)` (a right-handed view: the camera at `eye` looks down its -z toward `target`), `set_perspective(field_of_view,
  aspect, near, far)` (vertical `field_of_view` in radians) and `set_orthographic(left, right, bottom, top, near,
  far)`, both into Vulkan's clip space (y down, depth 0 to 1), `normal_matrix(): Matrix3?` (the inverse transpose of
  the upper 3x3, `null` when it has no inverse), and `to_string()` row by row, `[1 0 0 0; ...]`.
- **`Matrix3`**: `set_identity()`, `multiply`, `equals`, `transform(vector: Vector3)`, `transposed()`,
  `determinant()`, `inverse(): Matrix3?`, `set_rotation(rotation)`, `to_string()`.
- **`Quaternion`** (x, y, z, then w; `Quaternion()` is no rotation): `set_identity()`, `set_parts(x, y, z, w)`,
  `set_axis_angle(axis, angle)` (the axis is normalized first), `set_euler(angles, order:
  Quaternion.RotationOrder)` (the six orders; `'xyz'` applies x first, as Blender's XYZ), `multiply(other)` (`*`:
  `a * b` rotates by `b` then `a`), `equals`, `rotate(vector)`, `dot`, `length()`, `normalized()` (the zero
  quaternion becomes no rotation), `conjugate()`, `inverse(): Quaternion?` (`null` for the zero quaternion),
  `spherical_interpolate(target, amount)` and `normalized_interpolate(target, amount)` (both along the shorter
  way; the spherical one falls back to the normalized one when the two are within about 1.8 degrees),
  `to_matrix()`, and `to_string()`.
- **`AxisAlignedBox`** (parts `lowest_x` ... `highest_z`, made with `AxisAlignedBox(lowest, highest)`):
  `lowest()`, `highest()`, `center()`, `size()`, `contains(point)` (the faces count as inside), `intersects(other)`
  (touching counts), `united(other)`, `including(point)`, `transformed(matrix)` (the box around the eight turned
  corners, found from the center and the absolute upper 3x3, not the corners one by one), `to_string()`.
- **`Plane`** (`normal_x`, `normal_y`, `normal_z`, `distance`: the points where `normal . point + distance` is 0;
  `Plane()` is the floor through the origin facing +y): `set_parts`, `set_point_normal(point, normal)` (the normal
  is normalized), `normal()`, `signed_distance_to(point)` (positive in front), `normalize()`, `to_string()`.
- **`Frustum`** (`left`, `right`, `bottom`, `top`, `near`, `far`, each a `Plane` facing in):
  `set_from_view_projection(matrix)` (the planes of a Vulkan clip space, depth 0 to 1, normalized),
  `contains_point(point)`, `intersects_sphere(center, radius)`, `intersects_box(box)` (the corner farthest along
  each plane's normal: a box may be kept when it is only near a corner of the frustum, never dropped when it is
  inside).
- **`Ray`** (`origin_x` ... `direction_z`, made with `Ray(origin, direction)`; the direction need not be of length 1,
  and distances are in lengths of it): `origin()`, `direction()`, `point_at(distance)`, `hit_plane(plane): Float?`
  (`null` when parallel or behind), `hit_box(box): Float?` (the distance where the ray enters, or leaves when it
  starts inside; `null` when it misses or the box is behind), `hit_triangle(first, second, third): Float?` (either
  side, Moller and Trumbore's test; `null` when it misses, is parallel or is behind), `to_string()`.
- **Cost.** Each function answering a vector, matrix or quaternion allocates it; the `set_` functions write in
  place and allocate nothing (`benchmarks/game_maths`). Removing the allocations is open
  (`mortaros_missing_decisions.md`).
- `conformance/stage6/game_vectors`, `game_matrices` and `game_geometry` pin every function, rounding what goes through a sine to
  four places so the C library's last bit does not show.
