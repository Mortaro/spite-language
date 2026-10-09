# Archetype cases across sizes, densities and churn

Written by `bash design/proposals/compiler_archetypes/sweep.sh` on 2026-10-09. Best microseconds of the
interleaved runs (five when the list has at most 65 536 items, three above), each form's whole program as it
times itself, building the list included. Every answer was compared with naive.c's. `spite` is `naive/` built
`--optimized`; the C forms are built with `clang -O2`. The last column names the fastest hand form.
Under each table, the passes alone: what each C form reports for its passes (or, for the events, for handling
them) in the same best run, the list already made.

## shapes_with_optional_parts

| settings | spite | naive | expert_dense | expert_inline | expert_sparse | expert_tables | fastest hand form |
|---|---|---|---|---|---|---|---|
| items=4096 density=1 passes=20 | 426 | 334 | 207 | 242 | 68 | 47 | tables |
| items=4096 density=50 passes=20 | 870 | 884 | 225 | 382 | 206 | 145 | tables |
| items=4096 density=99 passes=20 | 859 | 794 | 213 | 343 | 281 | 194 | tables |
| items=65536 density=1 passes=20 | 6737 | 5363 | 2817 | 3301 | 757 | 637 | tables |
| items=65536 density=50 passes=20 | 23834 | 22895 | 3218 | 13979 | 4047 | 1766 | tables |
| items=65536 density=99 passes=20 | 19081 | 11751 | 2952 | 5573 | 4195 | 2232 | tables |
| items=1048576 density=1 passes=20 | 226105 | 252810 | 74437 | 113308 | 16107 | 9398 | tables |
| items=1048576 density=50 passes=20 | 629768 | 581462 | 52287 | 246044 | 82069 | 29085 | tables |
| items=1048576 density=99 passes=20 | 988557 | 711119 | 57794 | 160987 | 67423 | 51773 | tables |
| items=4194304 density=1 passes=20 | 855229 | 1132813 | 282938 | 541060 | 59444 | 36375 | tables |
| items=4194304 density=50 passes=20 | 2786909 | 2231699 | 299415 | 1043137 | 373927 | 135531 | tables |
| items=4194304 density=99 passes=20 | 3901940 | 2554615 | 265178 | 784475 | 346543 | 254949 | tables |

The passes alone, microseconds:

| settings | naive | expert_dense | expert_inline | expert_sparse | expert_tables | fastest hand form |
|---|---|---|---|---|---|---|
| items=4096 density=1 passes=20 | 167 | 127 | 174 | 3 | 2 | tables |
| items=4096 density=50 passes=20 | 481 | 127 | 267 | 81 | 27 | tables |
| items=4096 density=99 passes=20 | 377 | 132 | 249 | 149 | 98 | tables |
| items=65536 density=1 passes=20 | 2887 | 1977 | 2418 | 65 | 23 | tables |
| items=65536 density=50 passes=20 | 17917 | 2016 | 12760 | 2560 | 555 | tables |
| items=65536 density=99 passes=20 | 6127 | 2042 | 4343 | 2676 | 1273 | tables |
| items=1048576 density=1 passes=20 | 201602 | 57406 | 94241 | 1914 | 667 | tables |
| items=1048576 density=50 passes=20 | 487991 | 33876 | 223024 | 53245 | 8584 | tables |
| items=1048576 density=99 passes=20 | 603893 | 43394 | 142638 | 45090 | 35509 | tables |
| items=4194304 density=1 passes=20 | 961939 | 232309 | 476235 | 11312 | 2165 | tables |
| items=4194304 density=50 passes=20 | 1917374 | 212597 | 944911 | 271754 | 63812 | tables |
| items=4194304 density=99 passes=20 | 2181639 | 207993 | 717964 | 254374 | 195881 | tables |

## inventory_with_fields_set_and_cleared

| settings | spite | naive | expert_dense | expert_hybrid | expert_inline | expert_sparse | expert_tables | fastest hand form |
|---|---|---|---|---|---|---|---|---|
| items=4096 density=1 passes=20 churn=40 | 476 | 420 | 158 | 91 | 252 | 83 | 94 | sparse |
| items=4096 density=99 passes=20 churn=40 | 996 | 747 | 152 | 243 | 340 | 237 | 226 | dense |
| items=4096 density=50 passes=20 churn=0 | 1161 | 717 | 213 | 318 | 468 | 223 | 204 | tables |
| items=4096 density=50 passes=20 churn=4 | 788 | 766 | 203 | 338 | 388 | 233 | 263 | dense |
| items=4096 density=50 passes=20 churn=40 | 1072 | 869 | 176 | 247 | 542 | 232 | 210 | dense |
| items=4096 density=50 passes=20 churn=409 | 1565 | 1461 | 273 | 406 | 923 | 387 | 309 | dense |
