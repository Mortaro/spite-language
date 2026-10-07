# Game maths

The specification of [Game maths](../docs/game_maths.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Game maths

The standard library fills every maths gap a game needs, as a game engine package asks for it.

- **Plain classes of numbers** in `library/` (`vector2.spite`, `vector3.spite`, `vector4.spite`, `matrix3.spite`,
  `matrix4.spite`, `quaternion.spite`), with no attribute but their parts, so each fits `Vector<T>` and a
  binary writer. Tree-shaken: a program that names none of them has none of their code, and a program that
  uses `Vector3` carries only the functions it calls. Nothing runs at start-up and nothing is registered.
- **Parts.** Vectors and quaternions: `x`, `y`, `z`, `w`, the axis names, which are the one
  exception to the rule against single letters ([style.md](../docs/style.md#names)). Matrices: `column_C_row_R`,
  column-major, `Matrix4<Float>()` and `Matrix3<Float>()` the identity. A vector is made with all its parts
  (`Vector3(1.0, 2.0, 3.0)`); a quaternion and a matrix are made from their defaults and set.
- **Vectors are generic over their number class**: `generic $number_type: Number`, inferred from the parts
  (`Vector3(1, 2, 3)` is a `Vector3<Integer>`, `Vector3(1.0, 2.0, 3.0)` a `Vector3<Float>`) and written out where a
  type is named (`Vector3<Float>`); each class compiles to its own copy, so a `Vector3<Float>` is three `float`s.
  No `Vector3i`-style name exists. Every member answers the vector's own class and takes it (`scaled(factor)`,
  `linear_interpolate(target, amount)`): `length()`, `distance_to` and `normalized()` on whole numbers truncate as
  integer division does (the root is taken in `Double` and cut), and `normalized()` divides each part by the
  length.
- **Matrices and quaternions are generic over their number class too**: `generic $number_type: Number`, written
  out at every construction since an empty constructor infers nothing (`Matrix4<Float>()`, `Quaternion<Double>()`),
  and every member takes and answers its own class (`Matrix4<Double>.transform_point` takes a `Vector3<Double>`,
  `set_rotation` a `Quaternion<Double>`). Only `Float` and `Double` are accepted: any other class is
  "'crash $number_type == Float or $number_type == Double' always halts in Quaternion<Integer>: ...", reported by
  the class's constructor. Planes, rays, boxes, frustums and curves hold `Float`s and take `Vector2<Float>`,
  `Vector3<Float>`, `Vector4<Float>` and `Matrix4<Float>`.
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
- **`Quaternion`** (x, y, z, then w; `Quaternion<Float>()` is no rotation): `set_identity()`, `set_parts(x, y, z, w)`,
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
- **`Color`** (`red`, `green`, `blue`, `alpha`, each 0 to 1 and not clamped when set; made with all four):
  `equals` (exact), `linear_interpolate(target, amount)`, `to_linear()` and `to_standard_rgb()` (the sRGB curve on
  red, green and blue, alpha untouched), `to_hex()` (`#rrggbb`, with `aa` when alpha is below 255/255),
  `to_rgb_text()` (`rgb(255, 99, 71)`, or `rgba(...)` with alpha to three places), `to_hsl_text()` (hue, saturation
  and lightness to one place), and `to_string()`, which is `to_hex()`. Writing clamps each channel to 0..1 and rounds
  it to a byte.
- **`ColorText`** (`read(text): Color?`; text is trimmed and read without regard to case): `#` and 3, 4, 6 or 8
  hex digits, `#rgb` doubling each digit; `rgb(`/`rgba(` with three or four numbers, separated by commas, spaces or
  a `/` before alpha, each channel 0 to 255 or a percentage and clamped, alpha 0 to 1 or a percentage;
  `hsl(`/`hsla(` with a hue in degrees (a `deg` suffix allowed, any number of turns), saturation and lightness as
  percentages; the CSS named colours and `transparent` (`named_hex(name)` answers one's `#rrggbb`, or `""`).
  Anything else (a wrong count of digits or numbers, a letter where a number goes, a missing `)`) is `null`.
- **`CubicBezier`** (parts `start_x` ... `end_y`, made with four `Vector2`s): `point_at(amount)` (not clamped),
  `y_at_x(x)` (the amount found by at most 8 Newton steps, then 30 halvings if Newton did not settle, `x` held
  between the end points; for a curve whose x goes back on itself the first root found wins).
- **`Easing`**, a singleton bound like any other (`var easing = Easing()`): `smooth_step` and `smoother_step`
  (amount clamped to 0..1), and `in_`, `out_` and `in_out_` forms of `quadratic`, `cubic` and `sine`,
  `in_exponential`/`out_exponential` (exactly 0 at 0 and 1 at 1), `in_back`/`out_back` (overshoot 1.70158),
  `out_elastic` and `out_bounce`, as easings.net writes them; outside 0..1 they extrapolate unless said above.
- **`Noise(seed: UnsignedInteger)`**: `value_2d`, `value_3d` (a random value at each whole point, blended with the
  quintic fade), `gradient_2d`, `gradient_3d` (Perlin's improved noise, gradients from a hash of the point and the
  seed, 0 on every whole point, within -1.5..1.5 in 2D), and `interleaved_gradient(pixel_x, pixel_y)` (Jimenez's
  dither, 0 to 1). Nothing is random at run time: the same seed and point give the same value everywhere.
- **Half precision** (IEEE 754 binary16): `Float.to_half_precision(): UnsignedShort` rounds to nearest even,
  overflows to infinity from 65520, keeps not-a-number as `0x7e00`, and makes subnormals below 6.1e-5;
  `UnsignedShort.half_precision_to_float(): Float` is exact. Both are Spite over `Float.bits(): UnsignedInteger`
  and `UnsignedInteger.bits_as_float(): Float`, which reinterpret the four bytes and which a program may use too,
  as it may `Double.bits(): Long`, `Long.bits_as_double(): Double` and `UnsignedLong.bits_as_double(): Double`.
  **Cost**: none beyond the arithmetic. Each bit view is a primitive the compiler writes in place, a C union of
  the two types, so no memory is touched and nothing is allocated; `benchmarks/a_numbers_bits_are_read_in_place`
  (thirty million round trips through a half) measures it against C.
- **Cost.** These classes hold only numbers, so an answer that never leaves the function that asked for it lives
  in that function's frame and is written there by the function that makes it; one that is stored or printed is
  one heap allocation ([optimizations.md](../docs/optimizations.md#objects-that-never-leave-their-function-live-in-the-frame)).
  The `set_` functions write in place.
  `benchmarks/game_maths` measures both against the same passes in C (`naive.c` and `expert.c`).
- `conformance/stage6/game_vectors`, `game_matrices`, `game_geometry`, `game_colors`, `game_curves` and `half_precision` pin every function, rounding what goes through a sine to
  four places so the C library's last bit does not show.

---

Next: [Foreign libraries and operating systems](foreign_libraries.md).
